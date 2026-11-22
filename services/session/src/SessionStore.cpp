#include "platypus/session/SessionStore.hpp"

#include <platypus/export/OutlineExport.hpp>
#include <platypus/export/Png.hpp>
#include <platypus/observation/Json.hpp>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <system_error>

namespace platypus::session {

namespace json = observation::json;

namespace {

bool writeAtomically(const std::filesystem::path& file, const void* data, std::size_t size,
                     std::string* error) {
    const auto temp = file.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            if (error) *error = "cannot write " + temp;
            return false;
        }
        out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        if (!out) {
            if (error) *error = "write failed for " + temp;
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temp, file, ec);
    if (ec) {
        if (error) *error = "cannot move " + temp + " into place: " + ec.message();
        return false;
    }
    return true;
}

bool writeText(const std::filesystem::path& file, const std::string& text, std::string* error) {
    return writeAtomically(file, text.data(), text.size(), error);
}

json::Value optionalString(const std::optional<std::string>& s) {
    return s ? json::Value(*s) : json::Value(nullptr);
}

std::string stateOf(const std::optional<CloseReason>& closed) {
    return closed ? std::string(to_string(*closed)) : std::string("open");
}

std::string fixed2(double v) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.2f", v);
    return buffer;
}

}  // namespace

std::string sessionJson(const ObjectSession& session, std::optional<CloseReason> closed,
                        const std::string& closedUtc, const std::vector<ArtifactRef>& artifacts) {
    const auto& profile = session.profile();
    const auto g = session.guidance();

    json::Array captures;
    for (std::size_t i = 0; i < session.captures().size(); ++i) {
        const auto& c = session.captures()[i];
        const bool accepted = std::find(g.acceptedIndices.begin(), g.acceptedIndices.end(), i) !=
                              g.acceptedIndices.end();
        json::Object values;
        for (std::size_t m = 0; m < profile.measurands.size(); ++m)
            values.emplace_back(profile.measurands[m].label + "_" + profile.measurands[m].unit,
                                c.values[m] ? json::Value(*c.values[m]) : json::Value(nullptr));
        json::Object entry{{"observation_id", c.observationId},
                           {"timestamp_utc", c.timestampUtc},
                           {"measured", c.measured},
                           {"accepted", accepted},
                           {"values", std::move(values)}};
        if (!c.refusal.empty()) entry.emplace_back("refusal", c.refusal);
        captures.emplace_back(std::move(entry));
    }

    json::Array measurands;
    for (const auto& s : g.summary) {
        json::Array provenance;
        for (const auto& p : s.provenance)
            provenance.emplace_back(p);
        measurands.emplace_back(json::Object{{"label", s.label},
                                             {"unit", s.unit},
                                             {"n", static_cast<double>(s.n)},
                                             {"mean", s.mean},
                                             {"spread", s.spread},
                                             {"stddev", s.stddev},
                                             {"method", "mean of accepted repeat captures"},
                                             {"provenance", std::move(provenance)}});
    }
    json::Object consensus;
    for (std::size_t a = 0; a < profile.agreementClaims.size(); ++a)
        consensus.emplace_back(profile.agreementClaims[a], optionalString(g.consensus[a]));

    json::Array open;
    for (const auto& q : g.openQuestions)
        open.emplace_back(json::Object{{"field", q.field},
                                       {"reason", q.reason},
                                       {"resolving_view", q.resolvingView},
                                       {"analyzer_in_build", q.analyzerInBuild}});

    json::Array artifactList;
    for (const auto& a : artifacts)
        artifactList.emplace_back(
            json::Object{{"id", a.id}, {"kind", a.kind}, {"path", a.path}, {"note", a.note}});

    json::Array caveats;
    for (const auto& c : profile.caveats)
        caveats.emplace_back(c);

    json::Object root{
        {"schema", "platypus.scout_session/0.1"},
        {"session_id", session.id()},
        {"profile", profile.id},
        {"opened_utc", session.openedUtc()},
        {"closed_utc", closed ? json::Value(closedUtc) : json::Value(nullptr)},
        {"state", stateOf(closed)},
        {"guidance", json::Object{{"stage", std::string(to_string(g.stage))},
                                  {"instruction", g.instruction},
                                  {"model_satisfied", g.modelSatisfied},
                                  {"measured", static_cast<double>(g.measured)},
                                  {"accepted", static_cast<double>(g.accepted)},
                                  {"required", static_cast<double>(g.required)},
                                  {"repeat_tolerance_mm", profile.repeatToleranceMm}}},
        {"captures", std::move(captures)},
        // DERIVED statistics with provenance to each contributing claim; the
        // captures themselves are untouched observation records.
        {"summary", json::Object{{"measurands", std::move(measurands)},
                                 {"agreement_holds", g.agreementHolds},
                                 {"consensus", std::move(consensus)}}},
        {"open_questions", std::move(open)},
        {"artifacts", std::move(artifactList)},
        {"caveats", std::move(caveats)},
        {"human_review", json::Object{{"state", "pending"}}},
    };
    return json::serialize(json::Value(std::move(root)));
}

std::string summaryMarkdown(const ObjectSession& session, CloseReason closed,
                            const std::string& closedUtc,
                            const std::vector<ArtifactRef>& artifacts) {
    const auto& profile = session.profile();
    const auto g = session.guidance();
    std::string md;
    md += "# Scout session " + session.id() + " - " + profile.name + "\n\n";
    md += "Opened " + session.openedUtc() + ", closed " + closedUtc + ".\n\n";
    if (closed == CloseReason::ModelSatisfied)
        md += "**Model satisfied:** " + std::to_string(g.accepted) + " of " +
              std::to_string(g.measured) + " measured captures agree within " +
              fixed2(profile.repeatToleranceMm) + " mm.\n\n";
    else
        md += "**Closed by the operator** before the model was satisfied (" +
              std::to_string(g.accepted) + " accepted of " + std::to_string(g.required) +
              " needed). Treat the figures below as provisional.\n\n";

    md +=
        "## Measurements\n\n| Quantity | Mean | Spread | Std dev | Captures "
        "|\n|---|---|---|---|---|\n";
    for (const auto& s : g.summary)
        md += "| " + s.label + " | " + fixed2(s.mean) + " " + s.unit + " | " + fixed2(s.spread) +
              " " + s.unit + " | " + fixed2(s.stddev) + " " + s.unit + " | " + std::to_string(s.n) +
              " |\n";

    md += "\n## Classification (inferred)\n\n";
    for (std::size_t a = 0; a < profile.agreementClaims.size(); ++a)
        md += "- " + profile.agreementClaims[a] + ": " +
              (g.consensus[a] ? *g.consensus[a] : std::string("no agreement / not determined")) +
              "\n";
    if (!g.agreementHolds) md += "- Accepted captures disagree - review before relying on it.\n";

    if (!g.openQuestions.empty()) {
        md += "\n## Still open\n\n";
        for (const auto& q : g.openQuestions)
            md += "- **" + q.field + "** - " + q.reason + ". Needs " + q.resolvingView +
                  (q.analyzerInBuild ? "." : " (not analyzed in this build).") + "\n";
    }

    md += "\n## Captures\n\n";
    for (std::size_t i = 0; i < session.captures().size(); ++i) {
        const auto& c = session.captures()[i];
        const bool accepted = std::find(g.acceptedIndices.begin(), g.acceptedIndices.end(), i) !=
                              g.acceptedIndices.end();
        md += "- " + c.observationId + ": " +
              (!c.measured ? "refused - " + c.refusal
               : accepted  ? "accepted"
                           : "set aside (outlier)") +
              "\n";
    }

    if (!artifacts.empty()) {
        md += "\n## Artifacts\n\n";
        for (const auto& a : artifacts)
            md += "- `" + a.path + "` - " + a.note + "\n";
    }
    md += "\n## Caveats\n\n";
    for (const auto& c : profile.caveats)
        md += "- " + c + "\n";
    return md;
}

SessionStore::SessionStore(std::filesystem::path root) : root_(std::move(root)) {}

std::string SessionStore::nextSessionId() const {
    for (int n = 1; n < 100000; ++n) {
        char id[32];
        std::snprintf(id, sizeof(id), "sess-%04d", n);
        std::error_code ec;
        if (!std::filesystem::exists(root_ / id, ec)) return id;
    }
    return "sess-overflow";
}

bool SessionStore::save(const ObjectSession& session, std::string* error) const {
    std::error_code ec;
    const auto dir = root_ / session.id();
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        if (error) *error = "cannot create " + dir.string() + ": " + ec.message();
        return false;
    }
    return writeText(dir / "session.json", sessionJson(session), error);
}

FinishResult SessionStore::finish(const ObjectSession& session, CloseReason reason,
                                  const std::string& closedUtc) const {
    FinishResult result;
    result.directory = root_ / session.id();
    std::error_code ec;
    std::filesystem::create_directories(result.directory, ec);
    if (ec) {
        result.error = "cannot create " + result.directory.string() + ": " + ec.message();
        return result;
    }

    if (const auto rep = session.representativeCapture()) {
        const auto& c = session.captures()[*rep];
        if (!c.outlinePx.empty() && c.mmPerPx > 0.0) {
            if (!writeText(result.directory / "outline.json",
                           exporter::outlineForgeJson(c.outlinePx, c.mmPerPx), &result.error))
                return result;
            result.artifacts.push_back(
                {"outline-forge", "application/json", "outline.json",
                 "silhouette of " + c.observationId + " for mesh2cad Outline Forge"});
            if (!writeText(result.directory / "outline.dxf",
                           exporter::outlineDxf(c.outlinePx, c.mmPerPx), &result.error))
                return result;
            result.artifacts.push_back(
                {"outline-dxf", "application/dxf", "outline.dxf",
                 "silhouette of " + c.observationId + " in mm for Fusion 360 / KiCad"});
        }
        if (c.preview) {
            const auto png = exporter::encodePng(c.preview->pixels, c.preview->width,
                                                 c.preview->height, c.preview->channels);
            if (!png.empty()) {
                if (!writeAtomically(result.directory / "source.png", png.data(), png.size(),
                                     &result.error))
                    return result;
                result.artifacts.push_back({"source-image", "image/png", "source.png",
                                            "camera frame of " + c.observationId});
            }
        }
    }

    if (!writeText(result.directory / "summary.md",
                   summaryMarkdown(session, reason, closedUtc, result.artifacts), &result.error))
        return result;
    result.artifacts.push_back({"summary", "text/markdown", "summary.md", "handoff note"});

    writeText(result.directory / "session.json",
              sessionJson(session, reason, closedUtc, result.artifacts), &result.error);
    return result;
}

}  // namespace platypus::session
