// Fastener classifier tests: synthetic silhouettes with exact geometry drive
// the full analyzer → classifier path, proving the class rules, the nominal
// tables, the confidence gates, and the inferred-evidence contract rules.
#include <platypus/ai/FastenerClassifier.hpp>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

using namespace platypus;
using ai::FastenerClass;
using vision::CalibrationSpec;

constexpr std::uint16_t kWidth = 640;
constexpr std::uint16_t kHeight = 480;
constexpr double kPi = 3.14159265358979;

/// Gray8 scene builder: light canvas, dark shapes, optional erasing for bores.
class Scene {
   public:
    Scene() : pixels_(std::make_shared<std::vector<std::byte>>()) {
        pixels_->assign(static_cast<std::size_t>(kWidth) * kHeight, std::byte{235});
    }

    void addSquare(int x, int y, int side) {
        for (int dy = 0; dy < side; ++dy)
            for (int dx = 0; dx < side; ++dx)
                set(x + dx, y + dy, std::byte{20});
    }

    void addRect(double cx, double cy, double length, double width, double angleRad) {
        const double c = std::cos(angleRad);
        const double s = std::sin(angleRad);
        for (int y = 0; y < kHeight; ++y)
            for (int x = 0; x < kWidth; ++x) {
                const double dx = x - cx;
                const double dy = y - cy;
                const double u = c * dx + s * dy;
                const double v = -s * dx + c * dy;
                if (std::abs(u) <= length / 2.0 && std::abs(v) <= width / 2.0)
                    set(x, y, std::byte{20});
            }
    }

    /// Regular flat-top hexagon: intersection of three slabs at 0/60/120°.
    void addHexagon(double cx, double cy, double acrossFlats) {
        const double half = acrossFlats / 2.0;
        const double root3 = std::sqrt(3.0);
        for (int y = 0; y < kHeight; ++y)
            for (int x = 0; x < kWidth; ++x) {
                const double dx = x - cx;
                const double dy = y - cy;
                if (std::abs(dy) <= half && std::abs(root3 * dx + dy) <= 2.0 * half &&
                    std::abs(root3 * dx - dy) <= 2.0 * half)
                    set(x, y, std::byte{20});
            }
    }

    /// Erase a disc back to background (a bore).
    void addBore(double cx, double cy, double radius) {
        for (int y = 0; y < kHeight; ++y)
            for (int x = 0; x < kWidth; ++x) {
                const double dx = x - cx;
                const double dy = y - cy;
                if (dx * dx + dy * dy <= radius * radius) set(x, y, std::byte{235});
            }
    }

    [[nodiscard]] hal::Frame frame() const {
        return hal::Frame({kWidth, kHeight, hal::PixelFormat::Gray8, 30.0f}, pixels_,
                          std::chrono::steady_clock::time_point{});
    }

   private:
    void set(int x, int y, std::byte value) {
        if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return;
        (*pixels_)[static_cast<std::size_t>(y) * kWidth + static_cast<std::size_t>(x)] = value;
    }

    std::shared_ptr<std::vector<std::byte>> pixels_;
};

/// 40 px reference square at 20 mm spec: exactly 0.5 mm/px.
Scene calibratedScene() {
    Scene scene;
    scene.addSquare(50, 50, 40);
    return scene;
}

vision::ScoutAnalysis analyze(const Scene& scene) {
    const auto outcome = vision::analyzeFrame(scene.frame(), CalibrationSpec{20.0});
    assert(outcome.ok());
    return *outcome.analysis;
}

void test_bolt_classification() {
    // 240×24 px rod at 30° → 120×12 mm: aspect 10, width matches M12 shaft.
    auto scene = calibratedScene();
    scene.addRect(400.0, 280.0, 240.0, 24.0, 30.0 * kPi / 180.0);
    const auto analysis = analyze(scene);
    assert(analysis.subject.holeCount == 0);

    const auto result = ai::classify(analysis);
    assert(result.fastenerClass == FastenerClass::BoltOrScrew);
    assert(result.confidence >= 0.55 && result.confidence <= 0.9);
    assert(result.nominal.has_value());
    assert(result.nominal->designation == "M12");
    assert(result.nominal->basis == "shaft_diameter");
    assert(result.nominal->fitError < 0.1);
}

void test_nut_classification() {
    // Hexagon with 20 px across-flats (10 mm = M6) and a 5 px-radius bore.
    auto scene = calibratedScene();
    scene.addHexagon(400.0, 280.0, 20.0);
    scene.addBore(400.0, 280.0, 5.0);
    const auto analysis = analyze(scene);
    assert(analysis.subject.holeCount == 1);

    const auto result = ai::classify(analysis);
    assert(result.fastenerClass == FastenerClass::NutOrWasher);
    assert(result.confidence >= 0.6);
    assert(result.nominal.has_value());
    assert(result.nominal->designation == "M6");
    assert(result.nominal->basis == "hex_across_flats");
}

void test_unknown_classification() {
    // Compact silhouette without a bore is honestly unknown.
    auto scene = calibratedScene();
    scene.addRect(400.0, 280.0, 30.0, 26.0, 0.0);
    const auto result = ai::classify(analyze(scene));
    assert(result.fastenerClass == FastenerClass::Unknown);
    assert(result.confidence == 0.0);
    assert(!result.nominal.has_value());
    assert(!result.rationale.empty());
}

void test_no_nominal_when_off_table() {
    // Aspect says rod, but a 30 mm width is far from every table entry.
    auto scene = calibratedScene();
    scene.addRect(380.0, 280.0, 300.0, 60.0, 0.0);
    const auto result = ai::classify(analyze(scene));
    assert(result.fastenerClass == FastenerClass::BoltOrScrew);
    assert(!result.nominal.has_value());
}

void test_evidence_contract() {
    auto scene = calibratedScene();
    scene.addRect(400.0, 280.0, 240.0, 24.0, 30.0 * kPi / 180.0);
    const auto analysis = analyze(scene);
    const auto classification = ai::classify(analysis);

    observation::EngineeringObservation record;
    record.observationId = "scan-0001";
    record.timestampUtc = "2026-08-30T12:00:00Z";
    record.source = {{"app", "test"}};
    record.artifacts.push_back({"source-image", "image/x-portable-graymap", "source.pgm"});

    vision::appendEvidence(record, analysis, CalibrationSpec{20.0}, "source-image");
    ai::appendClassification(record, classification);

    // Contract-valid, and the inferred claims carry the mandatory fields.
    assert(observation::validate(record).empty());
    assert(record.inferred.size() == 2);
    for (const auto& claim : record.inferred) {
        assert(claim.confidence.has_value());
        assert(!claim.provenance.empty());
        assert(!claim.method.empty());
    }

    // The "not attempted" placeholders were replaced by genuine unknowns.
    bool sawBoltVsScrew = false;
    for (const auto& item : record.unresolved) {
        assert(item.name != "fastener_class");
        assert(item.name != "nominal_size");
        if (item.name == "bolt_vs_screw") sawBoltVsScrew = true;
    }
    assert(sawBoltVsScrew);

    // Round trip preserves the inferred evidence.
    const auto decoded = observation::fromJson(observation::toJson(record));
    assert(decoded.ok());
    assert(observation::validate(*decoded.record).empty());
    assert(decoded.record->inferred.size() == 2);
    assert(std::get<std::string>(decoded.record->inferred[0].value) == "bolt_or_screw");
}

void test_unknown_keeps_unresolved() {
    auto scene = calibratedScene();
    scene.addRect(400.0, 280.0, 30.0, 26.0, 0.0);
    const auto analysis = analyze(scene);

    observation::EngineeringObservation record;
    record.observationId = "scan-0002";
    record.timestampUtc = "2026-08-30T12:00:00Z";
    record.source = {{"app", "test"}};
    record.artifacts.push_back({"source-image", "image/x-portable-graymap", "source.pgm"});
    vision::appendEvidence(record, analysis, CalibrationSpec{20.0}, "source-image");
    ai::appendClassification(record, ai::classify(analysis));

    assert(observation::validate(record).empty());
    assert(record.inferred.empty());
    bool classUnresolved = false;
    for (const auto& item : record.unresolved)
        if (item.name == "fastener_class") classUnresolved = true;
    assert(classUnresolved);
}

}  // namespace

void test_fastener_classifier() {
    test_bolt_classification();
    test_nut_classification();
    test_unknown_classification();
    test_no_nominal_when_off_table();
    test_evidence_contract();
    test_unknown_keeps_unresolved();
    std::puts("test_fastener_classifier: OK");
}
