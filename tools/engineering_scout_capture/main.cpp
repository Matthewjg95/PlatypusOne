// engineering_scout_capture — development harness for the Scout capture slice.
//
//   engineering_scout_capture [--fake] [--device /dev/video0] [--out DIR]
//                             [--id scan-0042] [--mode WxH] [--reference-mm MM]
//                             [--no-analyze] [--list]
//
// Captures one frame (real V4L2 camera on Linux, deterministic fake anywhere),
// measures and classifies the scene against the calibration reference, and
// saves the image alongside a valid EngineeringObservation JSON record that
// carries the evidence. This is a bring-up tool, not PlatypusOne UI: it lives
// in tools/ because apps/ hold IApp implementations driven by the shell loop,
// and a one-shot CLI does not fit that contract.
//
// This is the only executable that runs the full physical path — camera to
// measured, classified evidence record — so it is what the first UNO Q bench
// session exercises (docs/hardware/TEST_CHECKLISTS.md steps 7-8).
#include <platypus/ai/FastenerClassifier.hpp>
#include <platypus/hal/testing/FakeCamera.hpp>
#include <platypus/observation/CaptureService.hpp>
#include <platypus/vision/ScoutAnalyzer.hpp>

#ifdef __linux__
#include "v4l2/V4l2Camera.hpp"
#endif

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

using namespace platypus;

int fail(const char* stage, hal::Error error) {
    std::fprintf(stderr, "error: %s failed: %.*s\n", stage,
                 static_cast<int>(hal::to_string(error).size()), hal::to_string(error).data());
    return 1;
}

/// Formats services/vision can measure. A UVC webcam offers YUYV and MJPEG and
/// essentially never RGB888 or GREY, so YUYV is the mode that matters on real
/// hardware; MJPEG frames can be stored but not measured without a decoder.
bool analyzable(hal::PixelFormat format) noexcept {
    return format == hal::PixelFormat::Gray8 || format == hal::PixelFormat::RGB888 ||
           format == hal::PixelFormat::YUYV;
}

std::string_view formatName(hal::PixelFormat format) noexcept {
    switch (format) {
        case hal::PixelFormat::Gray8:
            return "gray8";
        case hal::PixelFormat::RGB888:
            return "rgb888";
        case hal::PixelFormat::YUYV:
            return "yuyv";
        case hal::PixelFormat::MJPEG:
            return "mjpeg";
        case hal::PixelFormat::NV12:
            return "nv12";
        case hal::PixelFormat::Unknown:
            break;
    }
    return "unknown";
}

unsigned area(const hal::CameraMode& mode) noexcept {
    return static_cast<unsigned>(mode.width) * mode.height;
}

void printModes(const std::vector<hal::CameraMode>& modes) {
    for (const auto& mode : modes)
        std::printf("  %ux%u %-7s%s\n", mode.width, mode.height,
                    std::string(formatName(mode.format)).c_str(),
                    analyzable(mode.format) ? "  (measurable)" : "");
}

}  // namespace

int main(int argc, char** argv) {
    bool useFake = false;
    bool listOnly = false;
    bool analyze = true;
    std::string device = "/dev/video0";
    std::string outDir = "observations";
    std::string id;
    double referenceMm = vision::CalibrationSpec{}.referenceSideMm;
    unsigned pinWidth = 0;
    unsigned pinHeight = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
        if (arg == "--fake")
            useFake = true;
        else if (arg == "--list")
            listOnly = true;
        else if (arg == "--no-analyze")
            analyze = false;
        else if (arg == "--device")
            device = next();
        else if (arg == "--out")
            outDir = next();
        else if (arg == "--id")
            id = next();
        else if (arg == "--reference-mm")
            referenceMm = std::atof(next());
        else if (arg == "--mode") {
            if (std::sscanf(next(), "%ux%u", &pinWidth, &pinHeight) != 2) {
                std::fprintf(stderr, "error: --mode expects WxH, e.g. 640x480\n");
                return 2;
            }
        } else {
            std::fprintf(stderr,
                         "usage: engineering_scout_capture [--fake] [--device PATH] "
                         "[--out DIR] [--id ID] [--mode WxH] [--reference-mm MM] "
                         "[--no-analyze] [--list]\n");
            return 2;
        }
    }

    if (referenceMm <= 0.0) {
        std::fprintf(stderr, "error: --reference-mm must be positive\n");
        return 2;
    }

#ifndef __linux__
    if (!useFake) {
        std::fprintf(stderr, "note: no V4L2 on this platform; using --fake\n");
        useFake = true;
    }
#endif

    // --- Camera selection (composition root for this tool) -----------------
    std::unique_ptr<hal::ICamera> camera;
    std::string cameraIdentity;
    if (useFake) {
        camera = std::make_unique<hal::testing::FakeCamera>();
        cameraIdentity = "fake";
    }
#ifdef __linux__
    else {
        auto v4l2 = std::make_unique<unoq::V4l2Camera>(device);
        cameraIdentity = v4l2->deviceIdentity();
        if (cameraIdentity.empty()) cameraIdentity = device;
        camera = std::move(v4l2);
    }
#endif

    const auto modes = camera->supportedModes();
    if (modes.empty()) {
        std::fprintf(stderr, "error: no supported modes on %s\n",
                     useFake ? "fake camera" : device.c_str());
        return 1;
    }
    if (listOnly) {
        std::printf("modes (%s):\n", cameraIdentity.c_str());
        printModes(modes);
        return 0;
    }

    // --- Mode negotiation ---------------------------------------------------
    // Prefer the largest measurable mode; fall back to the largest mode of any
    // kind so a capture-only record is still possible on an MJPEG-only device.
    std::vector<hal::CameraMode> candidates;
    for (const auto& mode : modes)
        if (pinWidth == 0 || (mode.width == pinWidth && mode.height == pinHeight))
            candidates.push_back(mode);
    if (candidates.empty()) {
        std::fprintf(stderr, "error: no %ux%u mode on %s; available:\n", pinWidth, pinHeight,
                     cameraIdentity.c_str());
        printModes(modes);
        return 1;
    }

    const hal::CameraMode* best = nullptr;
    for (const auto& mode : candidates)
        if (analyzable(mode.format) && (!best || area(mode) > area(*best))) best = &mode;
    const bool measurable = best != nullptr;
    if (!best)
        for (const auto& mode : candidates)
            if (!best || area(mode) > area(*best)) best = &mode;
    const hal::CameraMode selected = *best;

    std::printf("camera: %s\n", cameraIdentity.c_str());
    std::printf("mode:   %ux%u %s\n", selected.width, selected.height,
                std::string(formatName(selected.format)).c_str());
    if (analyze && !measurable)
        std::fprintf(stderr,
                     "warning: %s frames cannot be measured (no decoder); capturing only. "
                     "Run --list to see whether this device offers yuyv.\n",
                     std::string(formatName(selected.format)).c_str());

    if (const auto status = camera->open(selected); !status)
        return fail("camera open", status.error());

    // --- Capture, measure, classify, persist --------------------------------
    observation::CaptureService service(outDir);
    observation::CaptureConfig config;
    config.observationId = id.empty() ? service.nextObservationId() : id;
    config.timestampUtc = observation::CaptureService::currentUtcTimestamp();
    config.source = {{"app", "engineering_scout_capture"},
                     {"camera", useFake ? "fake" : device},
                     {"camera_identity", cameraIdentity}};

    const vision::CalibrationSpec spec{referenceMm};
    std::optional<vision::ScoutAnalysis> analysis;
    std::optional<ai::FastenerClassification> classification;
    auto sceneError = vision::AnalyzeError::None;

    if (analyze && measurable) {
        // Runs inside CaptureService's all-or-nothing write: claims land in the
        // same record as the image, or nothing is written at all. A scene the
        // analyzer rejects is not an error here — it yields an honest
        // capture-only record and a reported scene condition.
        config.enrich = [&](const hal::Frame& frame, observation::EngineeringObservation& record) {
            const auto outcome = vision::analyzeFrame(frame, spec);
            if (!outcome.ok()) {
                sceneError = outcome.error;
                return;
            }
            vision::appendEvidence(record, *outcome.analysis, spec, "source-image");
            classification = ai::classify(*outcome.analysis);
            ai::appendClassification(record, *classification);
            analysis = *outcome.analysis;
        };
    }

    const auto result = service.capture(*camera, config);
    camera->close();
    if (!result) return fail("capture", result.error());

    const auto& r = result.value();
    std::printf("observation: %s\n", r.record.observationId.c_str());
    std::printf("  image:  %s\n", r.imagePath.string().c_str());
    std::printf("  record: %s\n", r.recordPath.string().c_str());

    if (analysis) {
        std::printf("  scale:  %.4f mm/px (reference %.2f mm)\n", analysis->mmPerPixel,
                    referenceMm);
        std::printf("  subject: %.2f x %.2f mm\n", analysis->subjectLengthMm,
                    analysis->subjectWidthMm);
        if (classification) {
            const auto name = ai::to_string(classification->fastenerClass);
            std::printf("  class:  %.*s (confidence %.2f)\n", static_cast<int>(name.size()),
                        name.data(), classification->confidence);
            if (classification->nominal)
                std::printf("  nominal: %s (%s)\n", classification->nominal->designation.c_str(),
                            classification->nominal->basis.c_str());
        }
    } else if (analyze && measurable) {
        const auto reason = vision::to_string(sceneError);
        std::printf("  analysis: no measurement — %.*s\n", static_cast<int>(reason.size()),
                    reason.data());
    }
    return 0;
}
