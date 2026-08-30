#include "platypus/observation/Observation.hpp"

#include <algorithm>
#include <unordered_set>

namespace platypus::observation {

namespace {

json::Value claimValueToJson(const ClaimValue& value) {
    if (const auto* d = std::get_if<double>(&value)) return json::Value(*d);
    if (const auto* b = std::get_if<bool>(&value)) return json::Value(*b);
    return json::Value(std::get<std::string>(value));
}

json::Value claimToJson(const Claim& claim) {
    json::Object obj;
    obj.emplace_back("id", claim.id);
    obj.emplace_back("name", claim.name);
    obj.emplace_back("value", claimValueToJson(claim.value));
    if (claim.unit) obj.emplace_back("unit", *claim.unit);
    if (claim.confidence) obj.emplace_back("confidence", *claim.confidence);
    if (!claim.provenance.empty()) {
        json::Array refs;
        for (const auto& ref : claim.provenance) refs.emplace_back(ref);
        obj.emplace_back("provenance", std::move(refs));
    }
    obj.emplace_back("method", claim.method);
    return json::Value(std::move(obj));
}

json::Value claimsToJson(const std::vector<Claim>& claims) {
    json::Array arr;
    for (const auto& claim : claims) arr.push_back(claimToJson(claim));
    return json::Value(std::move(arr));
}

bool decodeString(const json::Value& parent, std::string_view key, std::string& out,
                  std::string& error, bool required) {
    const auto* v = parent.find(key);
    if (!v) {
        if (required) error = std::string("missing required field: ") + std::string(key);
        return !required;
    }
    if (!v->isString()) {
        error = std::string("field is not a string: ") + std::string(key);
        return false;
    }
    out = v->asString();
    return true;
}

bool decodeClaim(const json::Value& v, Claim& out, std::string& error) {
    if (!v.isObject()) { error = "claim is not an object"; return false; }
    if (!decodeString(v, "id", out.id, error, /*required=*/true)) return false;
    if (!decodeString(v, "name", out.name, error, /*required=*/true)) return false;
    if (!decodeString(v, "method", out.method, error, /*required=*/false)) return false;

    const auto* value = v.find("value");
    if (!value) { error = "claim missing value: " + out.id; return false; }
    if (value->isNumber()) out.value = value->asNumber();
    else if (value->isBool()) out.value = value->asBool();
    else if (value->isString()) out.value = value->asString();
    else { error = "claim value must be number, bool, or string: " + out.id; return false; }

    if (const auto* unit = v.find("unit")) {
        if (!unit->isString()) { error = "unit is not a string: " + out.id; return false; }
        out.unit = unit->asString();
    }
    if (const auto* conf = v.find("confidence")) {
        if (!conf->isNumber()) { error = "confidence is not a number: " + out.id; return false; }
        out.confidence = conf->asNumber();
    }
    if (const auto* prov = v.find("provenance")) {
        if (!prov->isArray()) { error = "provenance is not an array: " + out.id; return false; }
        for (const auto& ref : prov->asArray()) {
            if (!ref.isString()) { error = "provenance entry is not a string: " + out.id; return false; }
            out.provenance.push_back(ref.asString());
        }
    }
    return true;
}

bool decodeClaims(const json::Value& root, std::string_view key,
                  std::vector<Claim>& out, std::string& error) {
    const auto* arr = root.find(key);
    if (!arr) return true;  // absent = empty
    if (!arr->isArray()) { error = std::string(key) + " is not an array"; return false; }
    for (const auto& entry : arr->asArray()) {
        Claim claim;
        if (!decodeClaim(entry, claim, error)) return false;
        out.push_back(std::move(claim));
    }
    return true;
}

/// Provenance may reference a claim id, an artifact id, or the explicit
/// "artifact:<id>" form used in the contract example.
bool referenceResolves(const std::string& ref,
                       const std::unordered_set<std::string>& claimIds,
                       const std::unordered_set<std::string>& artifactIds) {
    if (claimIds.count(ref) || artifactIds.count(ref)) return true;
    constexpr std::string_view kPrefix = "artifact:";
    if (ref.rfind(kPrefix, 0) == 0)
        return artifactIds.count(ref.substr(kPrefix.size())) > 0;
    return false;
}

}  // namespace

std::string_view to_string(ReviewState state) noexcept {
    switch (state) {
        case ReviewState::Pending:   return "pending";
        case ReviewState::Accepted:  return "accepted";
        case ReviewState::Rejected:  return "rejected";
        case ReviewState::Corrected: return "corrected";
    }
    return "pending";
}

std::optional<ReviewState> reviewStateFromString(std::string_view text) noexcept {
    if (text == "pending") return ReviewState::Pending;
    if (text == "accepted") return ReviewState::Accepted;
    if (text == "rejected") return ReviewState::Rejected;
    if (text == "corrected") return ReviewState::Corrected;
    return std::nullopt;
}

std::string toJson(const EngineeringObservation& record) {
    json::Object root;
    root.emplace_back("schema_version", std::string(kSchemaVersion));
    root.emplace_back("observation_id", record.observationId);
    root.emplace_back("timestamp_utc", record.timestampUtc);

    // First occurrence wins on duplicate source keys: JSON objects cannot
    // carry duplicates and our parser rejects them, so toJson must never
    // emit a document fromJson refuses. validate() reports the duplicates.
    json::Object source;
    for (const auto& [key, value] : record.source) {
        const bool seen = std::any_of(source.begin(), source.end(),
                                      [&](const auto& kv) { return kv.first == key; });
        if (!seen) source.emplace_back(key, value);
    }
    root.emplace_back("source", std::move(source));

    json::Array artifacts;
    for (const auto& artifact : record.artifacts) {
        json::Object obj;
        obj.emplace_back("id", artifact.id);
        obj.emplace_back("kind", artifact.kind);
        obj.emplace_back("path", artifact.path);
        artifacts.emplace_back(std::move(obj));
    }
    root.emplace_back("artifacts", std::move(artifacts));

    root.emplace_back("observed", claimsToJson(record.observed));
    root.emplace_back("derived", claimsToJson(record.derived));
    root.emplace_back("inferred", claimsToJson(record.inferred));

    json::Array unresolved;
    for (const auto& item : record.unresolved) {
        json::Object obj;
        obj.emplace_back("name", item.name);
        obj.emplace_back("reason", item.reason);
        unresolved.emplace_back(std::move(obj));
    }
    root.emplace_back("unresolved", std::move(unresolved));

    json::Array recommendations;
    for (const auto& rec : record.recommendedNextObservations) {
        json::Object obj;
        obj.emplace_back("action", rec.action);
        if (!rec.resolves.empty()) {
            json::Array names;
            for (const auto& name : rec.resolves) names.emplace_back(name);
            obj.emplace_back("resolves", std::move(names));
        }
        recommendations.emplace_back(std::move(obj));
    }
    root.emplace_back("recommended_next_observations", std::move(recommendations));

    json::Object review;
    review.emplace_back("state", std::string(to_string(record.humanReview.state)));
    if (record.humanReview.notes) review.emplace_back("notes", *record.humanReview.notes);
    root.emplace_back("human_review", std::move(review));

    return json::serialize(json::Value(std::move(root)));
}

DecodeOutcome fromJson(std::string_view text) {
    DecodeOutcome outcome;

    auto parsed = json::parse(text);
    if (!parsed.ok()) {
        outcome.error = "JSON parse error at offset " + std::to_string(parsed.offset) +
                        ": " + parsed.error;
        return outcome;
    }
    const auto& root = *parsed.value;
    if (!root.isObject()) { outcome.error = "record is not a JSON object"; return outcome; }

    EngineeringObservation record;
    std::string error;

    if (!decodeString(root, "observation_id", record.observationId, error, /*required=*/true) ||
        !decodeString(root, "timestamp_utc", record.timestampUtc, error, /*required=*/false)) {
        outcome.error = std::move(error);
        return outcome;
    }

    if (const auto* source = root.find("source")) {
        if (!source->isObject()) { outcome.error = "source is not an object"; return outcome; }
        for (const auto& [key, value] : source->asObject()) {
            if (!value.isString()) {
                outcome.error = "source values must be strings: " + key;
                return outcome;
            }
            record.source.emplace_back(key, value.asString());
        }
    }

    if (const auto* artifacts = root.find("artifacts")) {
        if (!artifacts->isArray()) { outcome.error = "artifacts is not an array"; return outcome; }
        for (const auto& entry : artifacts->asArray()) {
            Artifact artifact;
            if (!entry.isObject() ||
                !decodeString(entry, "id", artifact.id, error, /*required=*/true) ||
                !decodeString(entry, "kind", artifact.kind, error, /*required=*/false) ||
                !decodeString(entry, "path", artifact.path, error, /*required=*/false)) {
                outcome.error = error.empty() ? "artifact is not an object" : std::move(error);
                return outcome;
            }
            record.artifacts.push_back(std::move(artifact));
        }
    }

    if (!decodeClaims(root, "observed", record.observed, error) ||
        !decodeClaims(root, "derived", record.derived, error) ||
        !decodeClaims(root, "inferred", record.inferred, error)) {
        outcome.error = std::move(error);
        return outcome;
    }

    if (const auto* unresolved = root.find("unresolved")) {
        if (!unresolved->isArray()) { outcome.error = "unresolved is not an array"; return outcome; }
        for (const auto& entry : unresolved->asArray()) {
            Unresolved item;
            if (!entry.isObject() ||
                !decodeString(entry, "name", item.name, error, /*required=*/true) ||
                !decodeString(entry, "reason", item.reason, error, /*required=*/false)) {
                outcome.error = error.empty() ? "unresolved entry is not an object" : std::move(error);
                return outcome;
            }
            record.unresolved.push_back(std::move(item));
        }
    }

    if (const auto* recs = root.find("recommended_next_observations")) {
        if (!recs->isArray()) {
            outcome.error = "recommended_next_observations is not an array";
            return outcome;
        }
        for (const auto& entry : recs->asArray()) {
            RecommendedObservation rec;
            if (!entry.isObject() ||
                !decodeString(entry, "action", rec.action, error, /*required=*/true)) {
                outcome.error = error.empty() ? "recommendation is not an object" : std::move(error);
                return outcome;
            }
            if (const auto* resolves = entry.find("resolves")) {
                if (!resolves->isArray()) {
                    outcome.error = "resolves is not an array: " + rec.action;
                    return outcome;
                }
                for (const auto& name : resolves->asArray()) {
                    if (!name.isString()) {
                        outcome.error = "resolves entry is not a string: " + rec.action;
                        return outcome;
                    }
                    rec.resolves.push_back(name.asString());
                }
            }
            record.recommendedNextObservations.push_back(std::move(rec));
        }
    }

    if (const auto* review = root.find("human_review")) {
        if (!review->isObject()) { outcome.error = "human_review is not an object"; return outcome; }
        std::string state;
        if (!decodeString(*review, "state", state, error, /*required=*/false)) {
            outcome.error = std::move(error);
            return outcome;
        }
        if (!state.empty()) {
            const auto parsedState = reviewStateFromString(state);
            if (!parsedState) { outcome.error = "unknown review state: " + state; return outcome; }
            record.humanReview.state = *parsedState;
        }
        std::string notes;
        if (!decodeString(*review, "notes", notes, error, /*required=*/false)) {
            outcome.error = std::move(error);
            return outcome;
        }
        if (review->find("notes")) record.humanReview.notes = std::move(notes);
    }

    outcome.record = std::move(record);
    return outcome;
}

std::vector<std::string> validate(const EngineeringObservation& record) {
    std::vector<std::string> violations;

    if (record.observationId.empty())
        violations.push_back("observation_id is empty");
    // timestamp_utc is part of the contract's minimal record.
    if (record.timestampUtc.empty())
        violations.push_back("timestamp_utc is empty");

    std::unordered_set<std::string> sourceKeys;
    for (const auto& [key, value] : record.source)
        if (!sourceKeys.insert(key).second)
            violations.push_back("duplicate source key: " + key);

    std::unordered_set<std::string> claimIds;
    std::unordered_set<std::string> artifactIds;
    for (const auto& artifact : record.artifacts) {
        if (artifact.id.empty()) violations.push_back("artifact with empty id");
        else if (!artifactIds.insert(artifact.id).second)
            violations.push_back("duplicate artifact id: " + artifact.id);
    }

    const auto collectIds = [&](const std::vector<Claim>& claims, const char* className) {
        for (const auto& claim : claims) {
            if (claim.id.empty()) {
                violations.push_back(std::string(className) + " claim with empty id: " + claim.name);
            } else if (!claimIds.insert(claim.id).second) {
                violations.push_back("duplicate claim id: " + claim.id);
            }
        }
    };
    collectIds(record.observed, "observed");
    collectIds(record.derived, "derived");
    collectIds(record.inferred, "inferred");

    const auto checkCommon = [&](const Claim& claim) {
        if (claim.confidence && (*claim.confidence < 0.0 || *claim.confidence > 1.0))
            violations.push_back("confidence out of [0,1]: " + claim.id);
        for (const auto& ref : claim.provenance)
            if (!referenceResolves(ref, claimIds, artifactIds))
                violations.push_back("unresolvable provenance reference '" + ref +
                                     "' in claim: " + claim.id);
    };
    for (const auto& claim : record.observed) checkCommon(claim);
    for (const auto& claim : record.derived) {
        checkCommon(claim);
        // Contract: a derived value must point at the observations it used
        // and record its method.
        if (claim.provenance.empty())
            violations.push_back("derived claim without provenance: " + claim.id);
        if (claim.method.empty())
            violations.push_back("derived claim without method: " + claim.id);
    }
    for (const auto& claim : record.inferred) {
        checkCommon(claim);
        // Contract rules 4–5: inference always carries confidence,
        // provenance, and model/method identity.
        if (!claim.confidence)
            violations.push_back("inferred claim without confidence: " + claim.id);
        if (claim.provenance.empty())
            violations.push_back("inferred claim without provenance: " + claim.id);
        if (claim.method.empty())
            violations.push_back("inferred claim without method: " + claim.id);
    }

    return violations;
}

}  // namespace platypus::observation
