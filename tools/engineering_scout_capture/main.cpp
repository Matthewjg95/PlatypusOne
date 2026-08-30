// engineering_scout_capture — development harness for the Scout capture slice.
//
//   engineering_scout_capture [--fake] [--device /dev/video0] [--out DIR]
//                             [--id scan-0042] [--list]
//
// Captures one frame (real V4L2 camera on Linux, deterministic fake anywhere),
// saves it with a valid EngineeringObservation JSON record, and prints the
// resulting paths. This is a bring-up tool, not PlatypusOne UI: it lives in
// tools/ because apps/ hold IApp implementations driven by the shell loop,
// and a one-shot CLI does not fit that contract.
#include <platypus/hal/testing/FakeCamera.hpp>
#include <platypus/observation/CaptureService.hpp>

#ifdef __linux__
#include "V4l2Camera.hpp"
#endif

#include <cstdio>
#include <memory>
#include <string>

namespace {

using namespace platypus;

int fail(const char* stage, hal::Error error) {
    std::fprintf(stderr, "error: %s failed: %.*s\n", stage,
                 static_cast<int>(hal::to_string(error).size()),
                 hal::to_string(error).data());
    return 1;
}

void printModes(const std::vector<hal::CameraMode>& modes) {
    for (const auto& mode : modes)
        std::printf("  %ux%u format=%u\n", mode.width, mode.height,
                    static_cast<unsigned>(mode.format));
}

}  // namespace

int main(int argc, char** argv) {
    bool useFake = false;
    bool listOnly = false;
    std::string device = "/dev/video0";
    std::string outDir = "observations";
    std::string id;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
        if (arg == "--fake") useFake = true;
        else if (arg == "--list") listOnly = true;
        else if (arg == "--device") device = next();
        else if (arg == "--out") outDir = next();
        else if (arg == "--id") id = next();
        else {
            std::fprintf(stderr,
                         "usage: engineering_scout_capture [--fake] [--device PATH] "
                         "[--out DIR] [--id ID] [--list]\n");
            return 2;
        }
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

    // Prefer the largest MJPEG mode (compressed stills, the standard UVC
    // path); otherwise take the first mode the device offers.
    hal::CameraMode selected = modes.front();
    unsigned bestArea = 0;
    for (const auto& mode : modes) {
        if (mode.format != hal::PixelFormat::MJPEG) continue;
        const auto area = static_cast<unsigned>(mode.width) * mode.height;
        if (area > bestArea) {
            bestArea = area;
            selected = mode;
        }
    }

    if (const auto status = camera->open(selected); !status)
        return fail("camera open", status.error());

    // --- Capture through the service ---------------------------------------
    observation::CaptureService service(outDir);
    observation::CaptureConfig config;
    config.observationId = id.empty() ? service.nextObservationId() : id;
    config.timestampUtc = observation::CaptureService::currentUtcTimestamp();
    config.source = {{"app", "engineering_scout_capture"},
                     {"camera", useFake ? "fake" : device},
                     {"camera_identity", cameraIdentity}};

    const auto result = service.capture(*camera, config);
    camera->close();
    if (!result) return fail("capture", result.error());

    const auto& r = result.value();
    std::printf("observation: %s\n", r.record.observationId.c_str());
    std::printf("  image:  %s\n", r.imagePath.string().c_str());
    std::printf("  record: %s\n", r.recordPath.string().c_str());
    return 0;
}
