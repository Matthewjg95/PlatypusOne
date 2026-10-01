// Fastener classifier tests: synthetic silhouettes with exact geometry drive
// the full analyzer → classifier path, proving the class rules, the nominal
// tables, the confidence gates, and the inferred-evidence contract rules.
#include <platypus/ai/FastenerClassifier.hpp>
#include <platypus/hal/testing/SyntheticScene.hpp>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace platypus;
using ai::FastenerClass;
using Scene = hal::testing::SyntheticScene;
using vision::CalibrationSpec;

constexpr double kPi = 3.14159265358979;

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

/// 120 mm M12 bolt at 0.5 mm/px: 24 px shank, 36 px (18 mm AF) head.
void addM12Bolt(Scene& scene) {
    scene.addBolt(400.0, 280.0, 240.0, 24.0, 30.0 * kPi / 180.0, 36.0, 16.0);
}

void test_bolt_classification() {
    // Shank 12 mm under an 18 mm head: the nominal comes from the shank.
    auto scene = calibratedScene();
    addM12Bolt(scene);
    const auto analysis = analyze(scene);
    assert(analysis.subject.holeCount == 0);

    const auto result = ai::classify(analysis);
    assert(result.fastenerClass == FastenerClass::BoltOrScrew);
    assert(result.confidence >= 0.55 && result.confidence <= 0.9);
    assert(result.nominal.has_value());
    // M12 must be among the candidates; whether 1/2-13 UNC (12.7 mm) is named
    // beside it depends on raster error, as it would on a real bench.
    assert(result.nominal->designation.find("M12") != std::string::npos);
    assert(result.nominal->basis == "shaft_diameter");
    assert(result.nominal->fitError < 0.1);
    assert(result.shankWidthMm && std::abs(*result.shankWidthMm - 12.0) < 0.75);
    assert(result.endToShank > 1.3);
}

void test_quarter_twenty_is_not_assumed_metric() {
    // The bench screw: 1/4-20 UNC, 44.45 mm long, 9.48 mm head. A metric-only
    // table called it "M6"; the silhouette cannot tell, so it must not either.
    auto scene = calibratedScene();
    scene.addBolt(400.0, 280.0, 88.9, 12.7, 0.3, 18.96, 8.0);
    const auto analysis = analyze(scene);

    observation::EngineeringObservation record;
    record.observationId = "scan-0004";
    record.timestampUtc = "2026-09-30T12:00:00Z";
    record.source = {{"app", "test"}};
    record.artifacts.push_back({"source-image", "image/x-portable-graymap", "source.pgm"});
    vision::appendEvidence(record, analysis, CalibrationSpec{20.0}, "source-image");
    const auto result = ai::classify(analysis);
    ai::appendClassification(record, result);

    assert(result.fastenerClass == FastenerClass::BoltOrScrew);
    assert(result.nominal && !result.nominal->alternative.empty());
    assert(result.nominal->designation.find("1/4-20 UNC") != std::string::npos);
    assert(result.nominal->designation.find("M6") != std::string::npos);
    assert(result.nominal->confidence <= 0.45);
    bool pitchExplains = false;
    for (const auto& item : record.unresolved)
        if (item.name == "thread_pitch" && item.reason.find("UNC") != std::string::npos)
            pitchExplains = true;
    assert(pitchExplains);
    assert(observation::validate(record).empty());
}

void test_headless_rod_is_not_a_screw() {
    // A pen or a pin: rod-like, but nothing says fastener.
    auto scene = calibratedScene();
    scene.addRect(400.0, 280.0, 240.0, 24.0, 30.0 * kPi / 180.0);
    const auto result = ai::classify(analyze(scene));
    assert(result.fastenerClass == FastenerClass::Unknown);
    assert(!result.nominal.has_value());
    assert(result.rationale.find("no head") != std::string::npos);
}

void test_pcb_is_outside_the_library() {
    // A board with two mounting holes: the record must not ask fastener
    // questions about it (thread pitch, nominal size), only say what it is not.
    auto scene = calibratedScene();
    scene.addRect(400.0, 280.0, 160.0, 100.0, 0.0);
    scene.addBore(340.0, 250.0, 6.0);
    scene.addBore(460.0, 310.0, 6.0);
    const auto analysis = analyze(scene);
    assert(analysis.subject.holeCount == 2);

    observation::EngineeringObservation record;
    record.observationId = "scan-0003";
    record.timestampUtc = "2026-09-30T12:00:00Z";
    record.source = {{"app", "test"}};
    record.artifacts.push_back({"source-image", "image/x-portable-graymap", "source.pgm"});
    vision::appendEvidence(record, analysis, CalibrationSpec{20.0}, "source-image");
    ai::appendClassification(record, ai::classify(analysis));

    assert(observation::validate(record).empty());
    assert(record.inferred.empty());
    bool objectClass = false;
    for (const auto& item : record.unresolved) {
        assert(item.name != "thread_pitch");
        assert(item.name != "nominal_size");
        assert(item.name != "fastener_class");
        if (item.name == "object_class") objectClass = true;
    }
    assert(objectClass);
    for (const auto& r : record.recommendedNextObservations)
        for (const auto& field : r.resolves)
            assert(field != "thread_pitch");
}

void test_square_plate_with_hole_is_not_a_nut() {
    auto scene = calibratedScene();
    scene.addRect(400.0, 280.0, 72.0, 66.0, 0.0);
    scene.addBore(400.0, 280.0, 9.0);
    const auto result = ai::classify(analyze(scene));
    assert(result.fastenerClass == FastenerClass::Unknown);
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
    // A bolt whose 30 mm shank is far from every table entry.
    auto scene = calibratedScene();
    scene.addBolt(380.0, 280.0, 300.0, 60.0, 0.0, 90.0, 30.0);
    const auto result = ai::classify(analyze(scene));
    assert(result.fastenerClass == FastenerClass::BoltOrScrew);
    assert(!result.nominal.has_value());
}

void test_evidence_contract() {
    auto scene = calibratedScene();
    addM12Bolt(scene);
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
        if (item.name == "object_class") classUnresolved = true;
    assert(classUnresolved);
}

}  // namespace

void test_fastener_classifier() {
    test_bolt_classification();
    test_nut_classification();
    test_unknown_classification();
    test_quarter_twenty_is_not_assumed_metric();
    test_headless_rod_is_not_a_screw();
    test_pcb_is_outside_the_library();
    test_square_plate_with_hole_is_not_a_nut();
    test_no_nominal_when_off_table();
    test_evidence_contract();
    test_unknown_keeps_unresolved();
    std::puts("test_fastener_classifier: OK");
}
