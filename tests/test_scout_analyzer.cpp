// Scout analyzer tests: synthetic scenes with exactly known geometry prove the
// calibration scale and principal-axis measurements, the scene-condition
// errors, determinism, and the contract evidence emission.
#include <platypus/hal/testing/SyntheticScene.hpp>
#include <platypus/vision/ScoutAnalyzer.hpp>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

using namespace platypus;
using hal::testing::SyntheticScene;
using vision::AnalyzeError;
using vision::CalibrationSpec;

constexpr std::uint16_t kWidth = 640;
constexpr std::uint16_t kHeight = 480;

/// 40 px reference square + a 240×36 px rod rotated 25°; spec 20 mm square
/// means 0.5 mm/px exactly, so the rod should measure 120×18 mm.
SyntheticScene measurementScene() {
    SyntheticScene scene;
    scene.addSquare(50, 50, 40);
    scene.addRect(400.0, 280.0, 240.0, 36.0, 25.0 * 3.14159265358979 / 180.0);
    return scene;
}

void test_measures_calibrated_scene() {
    const CalibrationSpec spec{20.0};
    const auto outcome = vision::analyzeFrame(measurementScene().rgbFrame(), spec);
    assert(outcome.ok());
    const auto& a = *outcome.analysis;

    // The 40x40 square gives an exact scale.
    assert(a.reference.areaPx == 1600);
    assert(std::abs(a.mmPerPixel - 0.5) < 1e-12);
    assert(a.reference.fillRatio > 0.99);

    // Rasterization costs at most ~2 px on each principal extent.
    assert(std::abs(a.subjectLengthMm - 120.0) < 1.5);
    assert(std::abs(a.subjectWidthMm - 18.0) < 1.5);
    assert(std::abs(a.subject.majorAxisAngleRad - 25.0 * 3.14159265358979 / 180.0) < 0.02);

    // Gray8 input measures identically.
    const auto grayOutcome = vision::analyzeFrame(measurementScene().frame(), spec);
    assert(grayOutcome.ok());
    assert(std::abs(grayOutcome.analysis->subjectLengthMm - a.subjectLengthMm) < 1e-9);

    // YUYV is what a UVC webcam delivers; neutral chroma must measure exactly
    // like Gray8, since only the luma byte is read.
    const auto yuyvOutcome = vision::analyzeFrame(measurementScene().yuyvFrame(), spec);
    assert(yuyvOutcome.ok());
    assert(yuyvOutcome.analysis->binarizationThreshold ==
           grayOutcome.analysis->binarizationThreshold);
    assert(yuyvOutcome.analysis->reference.areaPx == grayOutcome.analysis->reference.areaPx);
    assert(std::abs(yuyvOutcome.analysis->mmPerPixel - grayOutcome.analysis->mmPerPixel) < 1e-12);
    assert(std::abs(yuyvOutcome.analysis->subjectLengthMm - grayOutcome.analysis->subjectLengthMm) <
           1e-9);
    assert(std::abs(yuyvOutcome.analysis->subjectWidthMm - grayOutcome.analysis->subjectWidthMm) <
           1e-9);

    // A YUYV buffer sized as if it were Gray8 must be rejected, not misread.
    const auto truncated = measurementScene().frame();
    const auto badOutcome = vision::analyzeFrame(
        hal::Frame({truncated.mode().width, truncated.mode().height, hal::PixelFormat::YUYV, 30.0f},
                   std::make_shared<std::vector<std::byte>>(truncated.pixels().begin(),
                                                            truncated.pixels().end()),
                   std::chrono::steady_clock::time_point{}),
        spec);
    assert(!badOutcome.ok());
    assert(badOutcome.error == vision::AnalyzeError::InvalidFrame);
}

void test_determinism() {
    const CalibrationSpec spec{20.0};
    const auto a = vision::analyzeFrame(measurementScene().rgbFrame(), spec);
    const auto b = vision::analyzeFrame(measurementScene().rgbFrame(), spec);
    assert(a.ok() && b.ok());
    assert(a.analysis->mmPerPixel == b.analysis->mmPerPixel);
    assert(a.analysis->subjectLengthMm == b.analysis->subjectLengthMm);
    assert(a.analysis->subjectWidthMm == b.analysis->subjectWidthMm);
    assert(a.analysis->binarizationThreshold == b.analysis->binarizationThreshold);
}

void test_scene_conditions_are_reported() {
    const CalibrationSpec spec{20.0};

    // Empty canvas: nothing to calibrate against.
    assert(vision::analyzeFrame(SyntheticScene().rgbFrame(), spec).error ==
           AnalyzeError::NoReferenceTarget);

    // Reference alone: nothing to measure.
    SyntheticScene referenceOnly;
    referenceOnly.addSquare(50, 50, 40);
    assert(vision::analyzeFrame(referenceOnly.rgbFrame(), spec).error == AnalyzeError::NoSubject);

    // Two comparable squares: the operator must remove one.
    SyntheticScene twoSquares;
    twoSquares.addSquare(50, 50, 40);
    twoSquares.addSquare(300, 300, 36);
    assert(vision::analyzeFrame(twoSquares.rgbFrame(), spec).error ==
           AnalyzeError::ReferenceAmbiguous);

    // Invalid inputs.
    assert(vision::analyzeFrame(hal::Frame{}, spec).error == AnalyzeError::InvalidFrame);
    assert(vision::analyzeFrame(measurementScene().rgbFrame(), CalibrationSpec{0.0}).error ==
           AnalyzeError::InvalidFrame);
    // Compressed formats stay unsupported: measuring them needs a decoder the
    // tree deliberately does not carry. YUYV, by contrast, is now measured
    // directly (see test_measures_calibrated_scene).
    const auto mjpeg = hal::Frame({kWidth, kHeight, hal::PixelFormat::MJPEG, 30.0f},
                                  std::make_shared<std::vector<std::byte>>(16, std::byte{0}),
                                  std::chrono::steady_clock::time_point{});
    assert(vision::analyzeFrame(mjpeg, spec).error == AnalyzeError::UnsupportedFormat);
}

void test_evidence_emission() {
    const CalibrationSpec spec{20.0};
    const auto outcome = vision::analyzeFrame(measurementScene().rgbFrame(), spec);
    assert(outcome.ok());

    observation::EngineeringObservation record;
    record.observationId = "scan-0001";
    record.timestampUtc = "2026-08-30T12:00:00Z";
    record.source = {{"app", "test"}};
    record.artifacts.push_back({"source-image", "image/x-portable-pixmap", "source.ppm"});

    vision::appendEvidence(record, *outcome.analysis, spec, "source-image");

    // The record satisfies the contract: unique ids, resolving provenance,
    // no inferred claims smuggled in.
    const auto violations = observation::validate(record);
    assert(violations.empty());
    assert(record.inferred.empty());
    assert(record.observed.size() == 9);  // + reference_keystone, outline_six_fold
    assert(record.derived.size() == 4);   // + camera_tilt
    // Perception states geometry only: no family questions before inference.
    assert(record.unresolved.empty());
    assert(record.recommendedNextObservations.empty());

    // The derived length is the observed pixel length through the scale.
    const auto& lengthClaim = record.derived[1];
    assert(lengthClaim.id == "sa-subj-length-mm");
    assert(lengthClaim.unit.has_value() && *lengthClaim.unit == "mm");
    assert(std::abs(std::get<double>(lengthClaim.value) - outcome.analysis->subjectLengthMm) <
           1e-12);

    // Serialized form still validates after a round trip.
    const auto decoded = observation::fromJson(observation::toJson(record));
    assert(decoded.ok());
    assert(observation::validate(*decoded.record).empty());
    assert(decoded.record->derived.size() == 4);
}

/// Bench evidence (2026-09-29, late-night desk lamp): the paper in one corner
/// of a real webcam frame was ~7x darker than at the centre. With one global
/// Otsu threshold the dim paper merged with the square and the screw into a
/// single blob touching the frame edge, and the scene was refused as "no
/// reference". The same measurement scene under an equally harsh vignette
/// must measure as if evenly lit.
void test_measures_through_uneven_lighting() {
    const CalibrationSpec spec{20.0};
    const auto evenFrame = measurementScene().frame();  // Gray8
    const auto& mode = evenFrame.mode();
    const auto src = evenFrame.pixels();

    // Radial falloff: 1.0 at the centre down to 0.15 at the corners (~7x).
    auto shaded = std::make_shared<std::vector<std::byte>>(src.size());
    const double cx = (mode.width - 1) / 2.0, cy = (mode.height - 1) / 2.0;
    const double halfDiagonal = std::sqrt(cx * cx + cy * cy);
    for (std::uint16_t y = 0; y < mode.height; ++y)
        for (std::uint16_t x = 0; x < mode.width; ++x) {
            const double r = std::hypot(x - cx, y - cy) / halfDiagonal;
            const double gain = 1.0 - 0.85 * r * r;
            const std::size_t i = std::size_t{y} * mode.width + x;
            const auto v = std::to_integer<int>(src[i]);
            (*shaded)[i] = static_cast<std::byte>(static_cast<int>(std::lround(v * gain)));
        }
    const hal::Frame frame(mode, shaded, evenFrame.timestamp());

    const auto outcome = vision::analyzeFrame(frame, spec);
    assert(outcome.ok());
    const auto& a = *outcome.analysis;
    // Same tolerances as the evenly lit scene: the lighting must not move the
    // measurement, only stop hiding it.
    assert(std::abs(a.mmPerPixel - 0.5) < 0.01);
    assert(std::abs(a.subjectLengthMm - 120.0) < 1.5);
    assert(std::abs(a.subjectWidthMm - 18.0) < 1.5);
}

/// Bench evidence (2026-09-29): with the fastener out of frame, a speck on the
/// paper was "measured" as a 2.1 x 1.8 mm subject. Anything smaller than the
/// smallest in-scope part is refused; an M3-nut-sized silhouette is not.
void test_rejects_specks_as_subjects() {
    const CalibrationSpec spec{20.0};
    // 100 px reference -> 0.2 mm/px, 0.04 mm^2/px.
    {
        SyntheticScene scene;
        scene.addSquare(60, 60, 100);
        scene.addSquare(400, 300, 12);  // 144 px (> the 64 px blob floor) = 5.8 mm^2
        const auto outcome = vision::analyzeFrame(scene.frame(), spec);
        assert(!outcome.ok());
        assert(outcome.error == AnalyzeError::NoSubject);
    }
    {
        SyntheticScene scene;
        scene.addSquare(60, 60, 100);
        scene.addRect(400.0, 300.0, 30.0, 27.5, 0.0);  // 6 x 5.5 mm, M3-nut sized
        const auto outcome = vision::analyzeFrame(scene.frame(), spec);
        assert(outcome.ok());
    }
}

}  // namespace

void test_scout_analyzer() {
    test_measures_calibrated_scene();
    test_determinism();
    test_scene_conditions_are_reported();
    test_evidence_emission();
    test_measures_through_uneven_lighting();
    test_rejects_specks_as_subjects();
    std::puts("test_scout_analyzer: OK");
}
