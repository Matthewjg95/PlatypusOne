// Scout capture slice tests: fake camera → saved artifact → valid record.
// Everything here runs hardware-free; the V4L2 backend shares only the
// ICamera seam and is verified on the physical UNO Q per TEST_CHECKLISTS §4.
#include <platypus/hal/testing/FakeCamera.hpp>
#include <platypus/observation/CaptureService.hpp>

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

}  // namespace

void test_scout_capture() {
    test_successful_capture();
    test_determinism();
    test_failures_leave_no_evidence();
    test_next_observation_id();
    std::puts("test_scout_capture: OK");
}
