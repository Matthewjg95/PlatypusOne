// Best-effort capture: every frame yields quality facts and whatever it
// supports, and nothing a refused scene yields can pass for a measurement.
#include "KioskCore.hpp"

#include <platypus/hal/testing/SyntheticScene.hpp>
#include <platypus/observation/CaptureService.hpp>
#include <platypus/observation/Observation.hpp>
#include <platypus/vision/ScoutAnalyzer.hpp>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

namespace {

using namespace platypus;
using hal::testing::SyntheticScene;

constexpr double kPi = 3.14159265358979;
const vision::CalibrationSpec kSpec{20.0};

bool hasClaim(const std::vector<observation::Claim>& claims, const std::string& name) {
    for (const auto& c : claims)
        if (c.name == name) return true;
    return false;
}

const observation::Claim* claim(const std::vector<observation::Claim>& claims,
                                const std::string& name) {
    for (const auto& c : claims)
        if (c.name == name) return &c;
    return nullptr;
}

observation::EngineeringObservation recordFor(const vision::Inspection& insp) {
    observation::EngineeringObservation r;
    r.observationId = "scan-test";
    r.timestampUtc = "2026-10-24T00:00:00Z";
    r.artifacts.push_back({"source-image", "image/x-portable-graymap", "source.pgm"});
    vision::appendInspection(r, insp, kSpec, "source-image");
    assert(observation::validate(r).empty());
    return r;
}

/// None of the strict measurement names may appear on a refused capture.
void assertNoStrictMeasurement(const observation::EngineeringObservation& r) {
    for (const auto* name : {"mm_per_pixel", "subject_length", "subject_width", "camera_tilt"}) {
        assert(!hasClaim(r.derived, name));
        assert(!hasClaim(r.observed, name));
    }
}

void test_good_scene_is_unchanged_plus_quality() {
    SyntheticScene scene;
    scene.addSquare(50, 50, 80);
    scene.addBolt(400.0, 280.0, 160.0, 32.0, 30.0 * kPi / 180.0, 52.0, 20.0);
    const auto insp = vision::inspectFrame(scene.frame(), kSpec);
    assert(insp.strict.ok() && vision::summarize(insp) == "measured");
    assert(insp.quality.contrast > 0.8 && insp.quality.sharpnessIndex > 10.0);
    const auto r = recordFor(insp);
    assert(hasClaim(r.derived, "subject_length") && hasClaim(r.observed, "capture_contrast"));
    assert(!hasClaim(r.derived, "best_effort_subject_length"));
    std::puts("test_inspect_good_scene: OK");
}

void test_part_without_square_keeps_pixels() {
    SyntheticScene scene;
    scene.addBolt(320.0, 240.0, 200.0, 40.0, 0.4, 64.0, 24.0);
    const auto insp = vision::inspectFrame(scene.frame(), kSpec);
    assert(!insp.strict.ok() && insp.strict.error == vision::AnalyzeError::NoReferenceTarget);
    assert(insp.subject && !insp.nearReference && insp.bestEffortMmPerPixel == 0.0);
    assert(insp.subject->lengthPx > 190.0 && insp.subject->lengthPx < 215.0);
    const auto r = recordFor(insp);
    assertNoStrictMeasurement(r);
    assert(hasClaim(r.observed, "best_effort_subject_length"));
    assert(!hasClaim(r.derived, "best_effort_mm_per_pixel"));
    assert(!r.unresolved.empty() && r.unresolved.back().name == "measurement");
    assert(vision::summarize(insp) == "best effort: pixels only (no 20 mm square)");
    std::puts("test_inspect_pixels_only: OK");
}

void test_squashed_square_gives_flagged_scale() {
    // A tilted view squashes the card: 80 x 60 px fails the 0.90 aspect gate.
    SyntheticScene scene;
    scene.addRect(100.0, 100.0, 80.0, 60.0, 0.0);
    scene.addBolt(420.0, 300.0, 160.0, 32.0, 0.5, 52.0, 20.0);
    const auto insp = vision::inspectFrame(scene.frame(), kSpec);
    assert(!insp.strict.ok() && insp.nearReference && insp.subject);
    assert(insp.failedChecks.size() == 1 && insp.failedChecks[0].rfind("reference_aspect", 0) == 0);
    assert(std::abs(insp.bestEffortConfidence - 0.4) < 1e-9);
    // 20 mm over sqrt(80 * 60) px
    assert(std::abs(insp.bestEffortMmPerPixel - 20.0 / std::sqrt(4800.0)) < 0.01);
    const auto r = recordFor(insp);
    assertNoStrictMeasurement(r);
    const auto* scale = claim(r.derived, "best_effort_mm_per_pixel");
    assert(scale && scale->confidence && *scale->confidence < 0.5);
    assert(scale->method.find("reference_aspect") != std::string::npos);
    assert(hasClaim(r.derived, "best_effort_subject_length"));
    assert(vision::summarize(insp) == "best effort: low-confidence scale");
    std::puts("test_inspect_flagged_scale: OK");
}

void test_square_alone_is_a_sound_scale() {
    SyntheticScene scene;
    scene.addSquare(200, 150, 80);
    const auto insp = vision::inspectFrame(scene.frame(), kSpec);
    assert(insp.strict.error == vision::AnalyzeError::NoSubject);
    assert(insp.nearReference && insp.failedChecks.empty() && !insp.subject);
    assert(std::abs(insp.bestEffortConfidence - 0.9) < 1e-9);
    assert(vision::summarize(insp) == "best effort: square only (no part)");
    (void)recordFor(insp);
    std::puts("test_inspect_square_only: OK");
}

void test_dim_scene_gets_advice() {
    // A dim, flat bench: mid-grey paper, dark-grey ink.
    SyntheticScene scene(640, 480, 95, 60);
    scene.addSquare(50, 50, 80);
    scene.addBolt(400.0, 280.0, 160.0, 32.0, 0.5, 52.0, 20.0);
    const auto insp = vision::inspectFrame(scene.frame(), kSpec);
    assert(insp.quality.contrast < 0.25 && insp.quality.lumaP50 < 100.0);
    const auto r = recordFor(insp);
    bool advised = false;
    for (const auto& rec : r.recommendedNextObservations)
        advised = advised || rec.action.find("low contrast") != std::string::npos;
    assert(advised);
    std::puts("test_inspect_dim_scene: OK");
}

void test_capture_writes_attached_files() {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "platypus_test_attach";
    std::error_code ec;
    fs::remove_all(root, ec);
    observation::CaptureService service(root);
    kiosk::SceneCamera camera;
    (void)camera.open(kiosk::SceneCamera::kMode);
    const auto attach = [](const hal::Frame&) {
        std::vector<observation::AttachedFile> f;
        f.push_back({"raw-frame", "image/x-raw-srggb10p", "raw.raw10",
                     std::vector<std::byte>(16, std::byte{0x5A})});
        return f;
    };
    const auto out = kiosk::captureOnce(camera, service, kSpec, "synthetic", "synthetic", attach);
    assert(out.record && out.measured);
    const fs::path dir = root / out.record->observationId;
    assert(fs::file_size(dir / "raw.raw10") == 16);
    bool listed = false;
    for (const auto& a : out.record->artifacts)
        listed = listed || (a.id == "raw-frame" && a.path == "raw.raw10");
    assert(listed);

    // A name that escapes the directory fails the whole capture, leaving nothing.
    observation::CaptureConfig bad;
    bad.observationId = "scan-bad";
    bad.timestampUtc = "2026-10-24T00:00:00Z";
    bad.attach = [](const hal::Frame&) {
        return std::vector<observation::AttachedFile>{{"x", "y", "../escape", {}}};
    };
    assert(!service.capture(camera, bad));
    assert(!fs::exists(root / "scan-bad"));
    fs::remove_all(root, ec);
    std::puts("test_inspect_attached_files: OK");
}

void test_refused_capture_status_says_what_was_kept() {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "platypus_test_besteffort";
    std::error_code ec;
    fs::remove_all(root, ec);
    observation::CaptureService service(root);
    kiosk::SceneCamera camera;
    (void)camera.open(kiosk::SceneCamera::kMode);
    camera.setPose(-1);  // part removed: square only
    const auto out = kiosk::captureOnce(camera, service, kSpec, "synthetic", "synthetic");
    assert(out.record && !out.measured && !out.refusal.empty());
    assert(out.status.find("square only") != std::string::npos);
    assert(hasClaim(out.record->observed, "capture_contrast"));
    fs::remove_all(root, ec);
    std::puts("test_inspect_refused_status: OK");
}

}  // namespace

void test_inspect() {
    test_good_scene_is_unchanged_plus_quality();
    test_part_without_square_keeps_pixels();
    test_squashed_square_gives_flagged_scale();
    test_square_alone_is_a_sound_scale();
    test_dim_scene_gets_advice();
    test_capture_writes_attached_files();
    test_refused_capture_status_says_what_was_kept();
}
