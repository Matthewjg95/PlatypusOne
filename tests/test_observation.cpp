// Engineering Observation contract tests: JSON round-trip fidelity, contract
// validation rules, parser strictness, and forward compatibility.
#include <platypus/observation/Observation.hpp>

#include <cassert>
#include <cstdio>

namespace {

using namespace platypus::observation;

/// The Engineering Scout Q v0.1 example from the contract document.
EngineeringObservation scoutExample() {
    EngineeringObservation record;
    record.observationId = "scan-0042";
    record.timestampUtc = "2026-08-24T21:00:00Z";
    record.source = {{"app", "engineering_scout"}, {"camera", "uvc0"}};
    record.artifacts = {{"image-0042", "image/png", "image-0042.png"}};

    record.observed.push_back(
        {"obs-marker", "aruco_marker_side", 20.0, "mm", std::nullopt, {}, "aruco_4x4"});

    record.derived.push_back({"drv-diameter", "shaft_diameter", 4.94, "mm", 0.96,
                              {"obs-marker", "artifact:image-0042"}, "calibrated_contour_v1"});
    record.derived.push_back({"drv-length", "overall_length", 19.8, "mm", 0.94,
                              {"obs-marker", "artifact:image-0042"}, "calibrated_contour_v1"});

    record.inferred.push_back({"inf-class", "part_class", std::string("socket_head_cap_screw"),
                               std::nullopt, 0.93, {"artifact:image-0042"}, "classifier_v1"});
    record.inferred.push_back({"inf-nominal", "likely_nominal_size", std::string("M5 x 20"),
                               std::nullopt, 0.89,
                               {"drv-diameter", "drv-length", "inf-class"},
                               "fastener_nominal_match_v1"});

    record.unresolved.push_back({"thread_pitch", "not resolvable from current view"});
    record.recommendedNextObservations.push_back(
        {"Capture a closer side view of the threaded region to estimate pitch.",
         {"thread_pitch"}});
    return record;
}

void test_roundtrip() {
    const auto record = scoutExample();
    assert(validate(record).empty());

    const auto text = toJson(record);
    const auto decoded = fromJson(text);
    assert(decoded.ok());

    const auto& r = *decoded.record;
    assert(r.observationId == "scan-0042");
    assert(r.source.size() == 2 && r.source[0].first == "app");
    assert(r.artifacts.size() == 1 && r.artifacts[0].id == "image-0042");
    assert(r.observed.size() == 1);
    assert(std::get<double>(r.observed[0].value) == 20.0);
    assert(r.observed[0].unit && *r.observed[0].unit == "mm");
    assert(!r.observed[0].confidence);  // optional for deterministic observations
    assert(r.derived.size() == 2);
    assert(r.derived[0].provenance.size() == 2);
    assert(r.inferred.size() == 2);
    assert(std::get<std::string>(r.inferred[1].value) == "M5 x 20");
    assert(r.inferred[1].confidence && *r.inferred[1].confidence == 0.89);
    assert(r.unresolved.size() == 1 && r.unresolved[0].name == "thread_pitch");
    assert(r.recommendedNextObservations.size() == 1);
    assert(r.recommendedNextObservations[0].resolves == std::vector<std::string>{"thread_pitch"});
    assert(r.humanReview.state == ReviewState::Pending);
    assert(validate(r).empty());

    // Serialization is deterministic: a second pass over the decoded record
    // reproduces the byte-identical document (records must diff cleanly).
    assert(toJson(r) == text);
}

void test_validation_rules() {
    auto record = scoutExample();

    // Contract rule 4: inference without confidence is a violation.
    record.inferred[0].confidence.reset();
    // Contract rule 5: inference without provenance/method is a violation.
    record.inferred[1].provenance.clear();
    record.inferred[1].method.clear();
    // Dangling provenance must be caught.
    record.derived[0].provenance.push_back("obs-ghost");
    // Confidence outside [0,1] must be caught.
    record.derived[1].confidence = 1.5;

    const auto violations = validate(record);
    assert(violations.size() == 5);

    const auto contains = [&](std::string_view needle) {
        for (const auto& v : violations)
            if (v.find(needle) != std::string::npos) return true;
        return false;
    };
    assert(contains("inferred claim without confidence: inf-class"));
    assert(contains("inferred claim without provenance: inf-nominal"));
    assert(contains("inferred claim without method: inf-nominal"));
    assert(contains("unresolvable provenance reference 'obs-ghost'"));
    assert(contains("confidence out of [0,1]: drv-length"));

    // Duplicate claim ids across classes are a violation (ids are record-wide).
    auto dup = scoutExample();
    dup.inferred[0].id = "drv-diameter";
    assert(!validate(dup).empty());

    // timestamp_utc is part of the contract's minimal record.
    auto noTime = scoutExample();
    noTime.timestampUtc.clear();
    const auto timeViolations = validate(noTime);
    assert(timeViolations.size() == 1 && timeViolations[0] == "timestamp_utc is empty");
}

void test_source_key_uniqueness() {
    auto record = scoutExample();
    record.source.emplace_back("app", "shadow-copy");  // duplicate key

    // validate() reports the duplicate...
    const auto violations = validate(record);
    assert(violations.size() == 1 && violations[0] == "duplicate source key: app");

    // ...and toJson() must still emit a document fromJson() accepts, keeping
    // the first occurrence.
    const auto decoded = fromJson(toJson(record));
    assert(decoded.ok());
    assert(decoded.record->source.size() == 2);
    assert(decoded.record->source[0] ==
           (std::pair<std::string, std::string>{"app", "engineering_scout"}));
}

void test_string_escaping() {
    EngineeringObservation record;
    record.observationId = "scan-\"quoted\"\\backslash";
    record.timestampUtc = "line\nbreak\ttab";
    record.unresolved.push_back({"unicode", "µ-thread \x01 control"});

    const auto text = toJson(record);
    const auto decoded = fromJson(text);
    assert(decoded.ok());
    assert(decoded.record->observationId == record.observationId);
    assert(decoded.record->timestampUtc == record.timestampUtc);
    assert(decoded.record->unresolved[0].reason == record.unresolved[0].reason);
}

void test_parser_strictness() {
    assert(!fromJson("").ok());
    assert(!fromJson("{").ok());
    assert(!fromJson("{}x").ok());                       // trailing garbage
    assert(!fromJson("{}").ok());                        // observation_id required
    assert(!fromJson(R"({"observation_id": 42})").ok()); // wrong type
    assert(!fromJson(R"({"observation_id": "a", "observed": {}})").ok());
    assert(!fromJson(R"({"observation_id": "a", "observation_id": "b"})").ok());  // dup key
    assert(!fromJson(R"({"observation_id": "a", "human_review": {"state": "maybe"}})").ok());

    // \u escape handling, including a surrogate pair (🔩 U+1F529).
    const auto decoded =
        fromJson(R"({"observation_id": "µm 🔩"})");
    assert(decoded.ok());
    assert(decoded.record->observationId == "\xC2\xB5m \xF0\x9F\x94\xA9");
}

void test_forward_compatibility() {
    // Unknown fields are ignored (same rule as unknown MCU-bridge topics):
    // a v0.2 writer must not break a v0.1 reader.
    const auto decoded = fromJson(R"({
        "schema_version": "0.2",
        "observation_id": "scan-1",
        "future_field": {"nested": [1, 2, 3]},
        "observed": [{"id": "o1", "name": "x", "value": 1.5, "novel_key": true}]
    })");
    assert(decoded.ok());
    assert(decoded.record->observationId == "scan-1");
    assert(decoded.record->observed.size() == 1);
}

}  // namespace

void test_observation() {
    test_roundtrip();
    test_validation_rules();
    test_source_key_uniqueness();
    test_string_escaping();
    test_parser_strictness();
    test_forward_compatibility();
    std::puts("test_observation: OK");
}
