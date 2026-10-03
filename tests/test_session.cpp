// Scout sessions: repeated captures of one object, guidance until the
// measurement is repeatable, honest open questions, and CAD export.
#include <platypus/observation/Json.hpp>
#include <platypus/session/SessionStore.hpp>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

using namespace platypus;
using session::CloseReason;
using session::Stage;

/// A measured record as the analyzer + classifier would write it.
observation::EngineeringObservation record(const std::string& id, double lengthMm, double widthMm,
                                           const std::string& cls = "bolt_or_screw",
                                           const std::string& nominal = "M6") {
    observation::EngineeringObservation r;
    r.observationId = id;
    r.timestampUtc = "2026-09-30T01:00:00Z";
    r.derived = {{"sa-mm-per-px", "mm_per_pixel", 0.25, "mm/px", std::nullopt, {}, "t"},
                 {"sa-subj-length-mm", "subject_length", lengthMm, "mm", std::nullopt, {}, "t"},
                 {"sa-subj-width-mm", "subject_width", widthMm, "mm", std::nullopt, {}, "t"}};
    r.inferred = {{"fc-class", "fastener_class", cls, std::nullopt, 0.75, {"x"}, "t"},
                  {"fc-nominal", "nominal_size", nominal, std::nullopt, 0.9, {"x"}, "t"}};
    r.unresolved = {
        {"thread_pitch", "a single top-down silhouette cannot resolve the thread profile"}};
    return r;
}

/// A refused scene: the record exists, but carries no claims.
observation::EngineeringObservation refused(const std::string& id) {
    observation::EngineeringObservation r;
    r.observationId = id;
    r.timestampUtc = "2026-09-30T01:00:00Z";
    return r;
}

session::ObjectSession newSession() {
    return {"sess-0001", session::fastenerProfile(), "2026-09-30T01:00:00Z"};
}

void add(session::ObjectSession& s, const observation::EngineeringObservation& r,
         const std::string& refusal = {}) {
    s.add(session::captureFrom(r, s.profile(), refusal));
}

void test_guidance_walks_the_three_stages() {
    auto s = newSession();
    auto g = s.guidance();
    assert(g.stage == Stage::NeedMeasurement);
    assert(g.instruction.find("tap CAPTURE") != std::string::npos);

    // A refused scene: the analyzer's reason is what the operator sees.
    add(s, refused("scan-0001"), "No 20 mm square found. Put the whole square in view.");
    g = s.guidance();
    assert(g.stage == Stage::NeedMeasurement && g.measured == 0);
    assert(g.instruction.find("No 20 mm square") != std::string::npos);

    add(s, record("scan-0002", 44.5, 9.50));
    g = s.guidance();
    assert(g.stage == Stage::NeedRepeats);
    assert(g.instruction.find("(2 more)") != std::string::npos);

    add(s, record("scan-0003", 44.6, 9.55));
    add(s, record("scan-0004", 44.4, 9.45));
    g = s.guidance();
    assert(g.stage == Stage::Complete && g.modelSatisfied);
    assert(g.accepted == 3 && g.measured == 3);
    assert(std::abs(g.summary[0].mean - 44.5) < 1e-9);
    assert(std::abs(g.summary[0].spread - 0.2) < 1e-9);
    assert(g.summary[0].n == 3);
    assert(g.summary[0].provenance[0] == "scan-0002#sa-subj-length-mm");
    assert(g.consensus[0] == std::optional<std::string>("bolt_or_screw"));
    assert(g.instruction.find("FINISH") != std::string::npos);
}

void test_one_bad_frame_is_set_aside_not_averaged() {
    auto s = newSession();
    add(s, record("scan-0001", 44.5, 9.5));
    add(s, record("scan-0002", 49.8, 12.1));  // the part moved; shadow changed
    add(s, record("scan-0003", 44.6, 9.5));
    add(s, record("scan-0004", 44.4, 9.5));
    const auto g = s.guidance();
    assert(g.modelSatisfied && g.accepted == 3 && g.measured == 4);
    assert(std::abs(g.summary[0].mean - 44.5) < 1e-9);  // the outlier did not drag it
    for (const auto i : g.acceptedIndices)
        assert(i != 1);
}

void test_disagreeing_captures_say_by_how_much() {
    auto s = newSession();
    add(s, record("scan-0001", 44.0, 9.5));
    add(s, record("scan-0002", 45.2, 9.5));
    add(s, record("scan-0003", 46.4, 9.5));
    const auto g = s.guidance();
    assert(g.stage == Stage::NeedRepeats && !g.modelSatisfied);
    assert(g.instruction.find("Captures differ by 2.4 mm") != std::string::npos);
}

void test_stable_but_contradictory_classification_is_not_satisfied() {
    auto s = newSession();
    add(s, record("scan-0001", 44.5, 9.5, "bolt_or_screw"));
    add(s, record("scan-0002", 44.5, 9.5, "bolt_or_screw"));
    add(s, record("scan-0003", 44.5, 9.5, "nut_or_washer"));
    const auto g = s.guidance();
    assert(g.stage == Stage::Complete && !g.modelSatisfied && !g.agreementHolds);
    assert(g.instruction.find("disagree on fastener_class") != std::string::npos);
}

void test_open_questions_are_honest_about_this_build() {
    auto s = newSession();
    for (const char* id : {"scan-0001", "scan-0002", "scan-0003"})
        add(s, record(id, 44.5, 9.5));
    const auto g = s.guidance();
    assert(g.openQuestions.size() == 1);
    const auto& q = g.openQuestions[0];
    assert(q.field == "thread_pitch");
    assert(!q.analyzerInBuild);  // requested, never claimed resolved
    assert(q.resolvingView.find("side-on") != std::string::npos);
}

void test_representative_capture_is_the_typical_one() {
    auto s = newSession();
    add(s, record("scan-0001", 44.7, 9.5));
    add(s, record("scan-0002", 44.5, 9.5));  // closest to the median
    add(s, record("scan-0003", 44.3, 9.5));
    assert(s.representativeCapture() == std::optional<std::size_t>(1));
}

std::string readFile(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void test_store_saves_and_exports_a_finished_session() {
    const auto root = std::filesystem::temp_directory_path() / "platypus_test_sessions";
    std::filesystem::remove_all(root);
    session::SessionStore store(root);
    assert(store.nextSessionId() == "sess-0001");

    session::ObjectSession s(store.nextSessionId(), session::fastenerProfile(),
                             "2026-09-30T01:00:00Z");
    for (const char* id : {"scan-0001", "scan-0002", "scan-0003"}) {
        auto c = session::captureFrom(record(id, 44.5, 9.5), s.profile());
        c.outlinePx.outer = {{10, 10}, {190, 10}, {190, 48}, {10, 48}};  // px
        c.preview = session::Image{4, 2, 3, std::vector<std::uint8_t>(24, 128)};
        s.add(std::move(c));
        assert(store.save(s));
    }
    assert(store.nextSessionId() == "sess-0002");  // the folder now exists

    const auto open = observation::json::parse(readFile(root / "sess-0001" / "session.json"));
    assert(open.ok() && open.value->find("state")->asString() == "open");

    const auto result = store.finish(s, CloseReason::ModelSatisfied, "2026-09-30T01:05:00Z");
    assert(result.ok());
    for (const char* f :
         {"outline.json", "outline.dxf", "source.png", "summary.md", "session.json"})
        assert(std::filesystem::exists(result.directory / f));
    assert(result.artifacts.size() == 4);

    const auto closed = observation::json::parse(readFile(result.directory / "session.json"));
    assert(closed.ok());
    const auto& v = *closed.value;
    assert(v.find("state")->asString() == "model_satisfied");
    assert(v.find("schema")->asString() == "platypus.scout_session/0.1");
    assert(v.find("artifacts")->asArray().size() == 4);
    assert(v.find("captures")->asArray().size() == 3);
    const auto& length = v.find("summary")->find("measurands")->asArray()[0];
    assert(length.find("mean")->asNumber() == 44.5);
    assert(length.find("provenance")->asArray().size() == 3);

    const auto forge = observation::json::parse(readFile(result.directory / "outline.json"));
    assert(forge.ok() && forge.value->find("format")->asString() == "shadowscan-outline");

    const auto md = readFile(result.directory / "summary.md");
    assert(md.find("Model satisfied") != std::string::npos);
    assert(md.find("thread_pitch") != std::string::npos);
    assert(md.find("parallax") != std::string::npos);  // caveats travel with the data

    std::filesystem::remove_all(root);
}

}  // namespace

void test_session() {
    test_guidance_walks_the_three_stages();
    test_one_bad_frame_is_set_aside_not_averaged();
    test_disagreeing_captures_say_by_how_much();
    test_stable_but_contradictory_classification_is_not_satisfied();
    test_open_questions_are_honest_about_this_build();
    test_representative_capture_is_the_typical_one();
    test_store_saves_and_exports_a_finished_session();
    std::puts("test_session: OK");
}
