// Scout analyzer tests: synthetic scenes with exactly known geometry prove the
// calibration scale and principal-axis measurements, the scene-condition
// errors, determinism, and the contract evidence emission.
#include <platypus/vision/ScoutAnalyzer.hpp>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

using namespace platypus;
using vision::AnalyzeError;
using vision::CalibrationSpec;

constexpr std::uint16_t kWidth = 640;
constexpr std::uint16_t kHeight = 480;

/// Scene builder: white RGB888 canvas, dark shapes drawn into it.
class Scene {
   public:
    Scene() : pixels_(std::make_shared<std::vector<std::byte>>()) {
        pixels_->assign(static_cast<std::size_t>(kWidth) * kHeight * 3, std::byte{235});
    }

    /// Filled axis-aligned square with its top-left at (x, y).
    void addSquare(int x, int y, int side) {
        for (int dy = 0; dy < side; ++dy)
            for (int dx = 0; dx < side; ++dx)
                darken(x + dx, y + dy);
    }

    /// Filled rectangle of length-by-width, centred at (cx, cy), rotated by
    /// angleRad. A pixel is inside when its centre maps into the rectangle.
    void addRotatedRect(double cx, double cy, double length, double width, double angleRad) {
        const double c = std::cos(angleRad);
        const double s = std::sin(angleRad);
        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
                const double dx = x - cx;
                const double dy = y - cy;
                const double u = c * dx + s * dy;
                const double v = -s * dx + c * dy;
                if (std::abs(u) <= length / 2.0 && std::abs(v) <= width / 2.0) darken(x, y);
            }
        }
    }

    [[nodiscard]] hal::Frame frame() const {
        return hal::Frame({kWidth, kHeight, hal::PixelFormat::RGB888, 30.0f}, pixels_,
                          std::chrono::steady_clock::time_point{});
    }

    [[nodiscard]] hal::Frame grayFrame() const {
        auto gray = std::make_shared<std::vector<std::byte>>();
        gray->reserve(static_cast<std::size_t>(kWidth) * kHeight);
        for (std::size_t i = 0; i < pixels_->size(); i += 3)
            gray->push_back((*pixels_)[i]);
        return hal::Frame({kWidth, kHeight, hal::PixelFormat::Gray8, 30.0f}, std::move(gray),
                          std::chrono::steady_clock::time_point{});
    }

   private:
    void darken(int x, int y) {
        if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return;
        const auto base = (static_cast<std::size_t>(y) * kWidth + static_cast<std::size_t>(x)) * 3;
        (*pixels_)[base] = (*pixels_)[base + 1] = (*pixels_)[base + 2] = std::byte{20};
    }

    std::shared_ptr<std::vector<std::byte>> pixels_;
};

/// 40 px reference square + a 240×36 px rod rotated 25°; spec 20 mm square
/// means 0.5 mm/px exactly, so the rod should measure 120×18 mm.
Scene measurementScene() {
    Scene scene;
    scene.addSquare(50, 50, 40);
    scene.addRotatedRect(400.0, 280.0, 240.0, 36.0, 25.0 * 3.14159265358979 / 180.0);
    return scene;
}

void test_measures_calibrated_scene() {
    const CalibrationSpec spec{20.0};
    const auto outcome = vision::analyzeFrame(measurementScene().frame(), spec);
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
    const auto grayOutcome = vision::analyzeFrame(measurementScene().grayFrame(), spec);
    assert(grayOutcome.ok());
    assert(std::abs(grayOutcome.analysis->subjectLengthMm - a.subjectLengthMm) < 1e-9);
}

void test_determinism() {
    const CalibrationSpec spec{20.0};
    const auto a = vision::analyzeFrame(measurementScene().frame(), spec);
    const auto b = vision::analyzeFrame(measurementScene().frame(), spec);
    assert(a.ok() && b.ok());
    assert(a.analysis->mmPerPixel == b.analysis->mmPerPixel);
    assert(a.analysis->subjectLengthMm == b.analysis->subjectLengthMm);
    assert(a.analysis->subjectWidthMm == b.analysis->subjectWidthMm);
    assert(a.analysis->binarizationThreshold == b.analysis->binarizationThreshold);
}

void test_scene_conditions_are_reported() {
    const CalibrationSpec spec{20.0};

    // Empty canvas: nothing to calibrate against.
    assert(vision::analyzeFrame(Scene().frame(), spec).error == AnalyzeError::NoReferenceTarget);

    // Reference alone: nothing to measure.
    Scene referenceOnly;
    referenceOnly.addSquare(50, 50, 40);
    assert(vision::analyzeFrame(referenceOnly.frame(), spec).error == AnalyzeError::NoSubject);

    // Two comparable squares: the operator must remove one.
    Scene twoSquares;
    twoSquares.addSquare(50, 50, 40);
    twoSquares.addSquare(300, 300, 36);
    assert(vision::analyzeFrame(twoSquares.frame(), spec).error ==
           AnalyzeError::ReferenceAmbiguous);

    // Invalid inputs.
    assert(vision::analyzeFrame(hal::Frame{}, spec).error == AnalyzeError::InvalidFrame);
    assert(vision::analyzeFrame(measurementScene().frame(), CalibrationSpec{0.0}).error ==
           AnalyzeError::InvalidFrame);
    const auto yuyv = hal::Frame({kWidth, kHeight, hal::PixelFormat::YUYV, 30.0f},
                                 std::make_shared<std::vector<std::byte>>(16, std::byte{0}),
                                 std::chrono::steady_clock::time_point{});
    assert(vision::analyzeFrame(yuyv, spec).error == AnalyzeError::UnsupportedFormat);
}

void test_evidence_emission() {
    const CalibrationSpec spec{20.0};
    const auto outcome = vision::analyzeFrame(measurementScene().frame(), spec);
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
    assert(record.observed.size() == 6);
    assert(record.derived.size() == 3);
    assert(record.unresolved.size() == 3);
    assert(record.recommendedNextObservations.size() == 1);

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
    assert(decoded.record->derived.size() == 3);
}

}  // namespace

void test_scout_analyzer() {
    test_measures_calibrated_scene();
    test_determinism();
    test_scene_conditions_are_reported();
    test_evidence_emission();
    std::puts("test_scout_analyzer: OK");
}
