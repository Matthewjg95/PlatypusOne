// Scout capture slice tests: fake camera → saved artifact → valid record.
// Everything here runs hardware-free; the V4L2 backend shares only the
// ICamera seam and is verified on the physical UNO Q per TEST_CHECKLISTS §4.
#include <platypus/ai/FastenerClassifier.hpp>
#include <platypus/hal/testing/FakeCamera.hpp>
#include <platypus/hal/testing/SyntheticScene.hpp>
#include <platypus/observation/CaptureService.hpp>
#include <platypus/vision/ScoutAnalyzer.hpp>

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

namespace fs = std::filesystem;
using namespace platypus;
using observation::CaptureConfig;
using observation::CaptureService;

/// Fresh scratch root per test run; removed on destruction.
struct ScratchRoot {
    ScratchRoot() : path(fs::temp_directory_path() / "platypus-test-observations") {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    ~ScratchRoot() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    fs::path path;
};

std::string readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

CaptureConfig testConfig(std::string id) {
    CaptureConfig config;
    config.observationId = std::move(id);
    config.timestampUtc = "2026-08-29T12:00:00Z";
    config.source = {{"app", "engineering_scout_capture"}, {"camera", "fake"}};
    return config;
}

void test_successful_capture() {
    ScratchRoot scratch;
    hal::testing::FakeCamera camera;
    assert(camera.open(hal::testing::FakeCamera::kFixtureMode).ok());

    CaptureService service(scratch.path);
    const auto result = service.capture(camera, testConfig("scan-0001"));
    assert(result.ok());
    const auto& r = result.value();

    // Files exist where the record says they are.
    assert(fs::exists(r.imagePath));
    assert(fs::exists(r.recordPath));
    assert(r.imagePath.filename() == "source.ppm");  // RGB888 fixture -> PPM

    // Image artifact appears in the record with a relative, provenance-ready path.
    assert(r.record.artifacts.size() == 1);
    assert(r.record.artifacts[0].id == "source-image");
    assert(r.record.artifacts[0].path == "source.ppm");
    assert(r.record.artifacts[0].kind == "image/x-portable-pixmap");

    // PPM header + full payload landed on disk.
    const auto image = readFile(r.imagePath);
    assert(image.rfind("P6\n640 480\n255\n", 0) == 0);
    assert(image.size() == 15 + 640u * 480u * 3u);

    // Saved JSON parses, validates, and round-trips source metadata,
    // including the frame facts the service appended.
    const auto decoded = observation::fromJson(readFile(r.recordPath));
    assert(decoded.ok());
    assert(observation::validate(*decoded.record).empty());
    const auto& source = decoded.record->source;
    const auto has = [&](const char* k, const char* v) {
        for (const auto& [key, value] : source)
            if (key == k && value == v) return true;
        return false;
    };
    assert(has("app", "engineering_scout_capture"));
    assert(has("camera", "fake"));
    assert(has("frame_width", "640"));
    assert(has("frame_height", "480"));
    assert(has("pixel_format", "rgb888"));
    assert(decoded.record->timestampUtc == "2026-08-29T12:00:00Z");
}

void test_determinism() {
    // Two captures of the fixture produce byte-identical images.
    ScratchRoot scratch;
    hal::testing::FakeCamera camera;
    assert(camera.open(hal::testing::FakeCamera::kFixtureMode).ok());
    CaptureService service(scratch.path);

    const auto a = service.capture(camera, testConfig("scan-0001"));
    const auto b = service.capture(camera, testConfig("scan-0002"));
    assert(a.ok() && b.ok());
    assert(readFile(a.value().imagePath) == readFile(b.value().imagePath));
}

void test_failures_leave_no_evidence() {
    ScratchRoot scratch;
    CaptureService service(scratch.path);

    // Camera capture failure -> error surfaces, nothing on disk.
    hal::testing::FakeCamera failing({.failOpen = false, .failCapture = true});
    assert(failing.open(hal::testing::FakeCamera::kFixtureMode).ok());
    const auto failed = service.capture(failing, testConfig("scan-0001"));
    assert(!failed.ok() && failed.error() == hal::Error::Timeout);
    assert(!fs::exists(scratch.path / "scan-0001"));

    // Unopened camera -> NotInitialized, nothing on disk.
    hal::testing::FakeCamera closed;
    const auto uninit = service.capture(closed, testConfig("scan-0002"));
    assert(!uninit.ok() && uninit.error() == hal::Error::NotInitialized);
    assert(!fs::exists(scratch.path / "scan-0002"));

    // Invalid config rejected before any I/O.
    hal::testing::FakeCamera camera;
    assert(camera.open(hal::testing::FakeCamera::kFixtureMode).ok());
    auto badId = testConfig("has/slash");
    assert(service.capture(camera, badId).error() == hal::Error::InvalidArgument);
    auto noTime = testConfig("scan-0003");
    noTime.timestampUtc.clear();
    assert(service.capture(camera, noTime).error() == hal::Error::InvalidArgument);

    // Existing observation directory is never overwritten.
    assert(service.capture(camera, testConfig("scan-0004")).ok());
    assert(service.capture(camera, testConfig("scan-0004")).error() == hal::Error::Busy);
}

void test_next_observation_id() {
    ScratchRoot scratch;
    CaptureService service(scratch.path);
    assert(service.nextObservationId() == "scan-0001");

    hal::testing::FakeCamera camera;
    assert(camera.open(hal::testing::FakeCamera::kFixtureMode).ok());
    assert(service.capture(camera, testConfig("scan-0001")).ok());
    assert(service.capture(camera, testConfig("scan-0002")).ok());
    assert(service.nextObservationId() == "scan-0003");

    // Gaps are filled first (ids are directory names, not a counter).
    std::error_code ec;
    fs::remove_all(scratch.path / "scan-0001", ec);
    assert(service.nextObservationId() == "scan-0001");
}

/// A camera that hands back one measurable scene in the format a UVC webcam
/// actually delivers. The V4L2 backend shares only the ICamera seam, so this
/// is how the capture → measure → classify path is verified without hardware.
class SceneCamera final : public hal::ICamera {
   public:
    static constexpr hal::CameraMode kMode{640, 480, hal::PixelFormat::YUYV, 30.0f};

    [[nodiscard]] std::vector<hal::CameraMode> supportedModes() const override { return {kMode}; }
    hal::Status open(const hal::CameraMode&) override {
        open_ = true;
        return {};
    }
    hal::Status close() override {
        open_ = false;
        return {};
    }
    [[nodiscard]] bool isOpen() const noexcept override { return open_; }
    hal::Status setControls(const hal::CameraControls&) override { return {}; }
    hal::Result<hal::Frame> capture(std::chrono::milliseconds) override {
        if (!open_) return hal::Error::NotInitialized;
        // 40 px square at 20 mm gives exactly 0.5 mm/px; the rod is 120 x 18 mm.
        hal::testing::SyntheticScene scene;
        scene.addSquare(50, 50, 40);
        scene.addRect(400.0, 280.0, 240.0, 36.0, 0.0);
        return scene.yuyvFrame();
    }
    hal::Status startStream(std::function<void(const hal::Frame&)>) override {
        return hal::Error::NotSupported;
    }
    hal::Status stopStream() override { return {}; }

   private:
    bool open_ = false;
};

/// The capture harness's wiring: measurement and classification claims must
/// reach the record inside CaptureService's all-or-nothing write.
void test_enrichment_writes_measured_evidence() {
    ScratchRoot scratch;
    CaptureService service(scratch.path);
    SceneCamera camera;
    assert(camera.open(SceneCamera::kMode).ok());

    const vision::CalibrationSpec spec{20.0};
    auto config = testConfig("scan-0010");
    config.enrich = [&](const hal::Frame& frame, observation::EngineeringObservation& record) {
        const auto outcome = vision::analyzeFrame(frame, spec);
        assert(outcome.ok());  // YUYV must be measurable, not rejected
        vision::appendEvidence(record, *outcome.analysis, spec, "source-image");
        ai::appendClassification(record, ai::classify(*outcome.analysis));
    };

    const auto result = service.capture(camera, config);
    assert(result.ok());

    // The frame is stored in its own format, and the record carries evidence
    // rather than the capture-only skeleton.
    assert(result.value().imagePath.filename() == "source.yuyv");
    assert(!result.value().record.observed.empty());
    assert(!result.value().record.derived.empty());
    assert(validate(result.value().record).empty());

    // The measurement actually landed on disk, not just in the returned copy.
    const auto json = readFile(result.value().recordPath);
    assert(json.find("mm_per_pixel") != std::string::npos);
    assert(json.find("yuyv") != std::string::npos);
}

/// A scene the analyzer rejects must still leave an honest capture-only
/// record — never a half-written one, and never invented claims.
void test_unmeasurable_scene_still_captures() {
    ScratchRoot scratch;
    CaptureService service(scratch.path);
    SceneCamera camera;
    assert(camera.open(SceneCamera::kMode).ok());

    auto config = testConfig("scan-0011");
    config.enrich = [](const hal::Frame&, observation::EngineeringObservation&) {
        // Analyzer found nothing usable: append nothing.
    };

    const auto result = service.capture(camera, config);
    assert(result.ok());
    assert(result.value().record.observed.empty());
    assert(validate(result.value().record).empty());
    assert(fs::exists(result.value().imagePath));
}

}  // namespace

void test_scout_capture() {
    test_successful_capture();
    test_determinism();
    test_failures_leave_no_evidence();
    test_next_observation_id();
    test_enrichment_writes_measured_evidence();
    test_unmeasurable_scene_still_captures();
    std::puts("test_scout_capture: OK");
}
