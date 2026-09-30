#include "platypus/session/Session.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <variant>

namespace platypus::session {

namespace {

const observation::Claim* byName(const std::vector<observation::Claim>& claims,
                                 const std::string& name) {
    for (const auto& c : claims)
        if (c.name == name) return &c;
    return nullptr;
}

double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    const auto n = v.size();
    return n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0;
}

std::string format1(const char* fmt, double value) {
    char buffer[160];
    std::snprintf(buffer, sizeof(buffer), fmt, value);
    return buffer;
}

}  // namespace

std::string_view to_string(Stage stage) noexcept {
    switch (stage) {
        case Stage::NeedMeasurement:
            return "need_measurement";
        case Stage::NeedRepeats:
            return "need_repeats";
        case Stage::Complete:
            return "complete";
    }
    return "unknown";
}

std::string_view to_string(CloseReason reason) noexcept {
    switch (reason) {
        case CloseReason::ModelSatisfied:
            return "model_satisfied";
        case CloseReason::ClosedByOperator:
            return "closed_by_operator";
    }
    return "unknown";
}

Capture captureFrom(const observation::EngineeringObservation& record, const ObjectProfile& profile,
                    std::string refusal) {
    Capture c;
    c.observationId = record.observationId;
    c.timestampUtc = record.timestampUtc;
    c.refusal = std::move(refusal);
    c.measured = !profile.measurands.empty();
    for (const auto& m : profile.measurands) {
        const auto* claim = byName(record.derived, m.claimName);
        const auto* value = claim ? std::get_if<double>(&claim->value) : nullptr;
        c.values.push_back(value ? std::optional<double>(*value) : std::nullopt);
        c.valueClaimIds.push_back(claim ? claim->id : std::string{});
        if (!value) c.measured = false;
    }
    for (const auto& name : profile.agreementClaims) {
        const auto* claim = byName(record.inferred, name);
        const auto* value = claim ? std::get_if<std::string>(&claim->value) : nullptr;
        c.agreement.push_back(value ? std::optional<std::string>(*value) : std::nullopt);
    }
    if (const auto* scale = byName(record.derived, "mm_per_pixel"))
        if (const auto* v = std::get_if<double>(&scale->value)) c.mmPerPx = *v;
    c.unresolved = record.unresolved;
    c.recommended = record.recommendedNextObservations;
    return c;
}

ObjectSession::ObjectSession(std::string id, ObjectProfile profile, std::string openedUtc)
    : id_(std::move(id)), profile_(std::move(profile)), openedUtc_(std::move(openedUtc)) {}

void ObjectSession::add(Capture capture) {
    captures_.push_back(std::move(capture));
}

std::vector<std::size_t> ObjectSession::acceptedSet() const {
    std::vector<std::size_t> measured;
    for (std::size_t i = 0; i < captures_.size(); ++i)
        if (captures_[i].measured) measured.push_back(i);
    if (measured.empty()) return {};

    // Accept captures within half the tolerance of the median on every
    // measurand, so the accepted set's spread never exceeds the tolerance.
    std::vector<double> medians;
    for (std::size_t m = 0; m < profile_.measurands.size(); ++m) {
        std::vector<double> v;
        for (const auto i : measured)
            v.push_back(*captures_[i].values[m]);
        medians.push_back(median(std::move(v)));
    }
    std::vector<std::size_t> accepted;
    for (const auto i : measured) {
        bool within = true;
        for (std::size_t m = 0; m < medians.size(); ++m)
            if (std::abs(*captures_[i].values[m] - medians[m]) > profile_.repeatToleranceMm / 2.0)
                within = false;
        if (within) accepted.push_back(i);
    }
    return accepted;
}

std::optional<std::size_t> ObjectSession::representativeCapture() const {
    const auto accepted = acceptedSet();
    if (accepted.empty()) {
        for (std::size_t i = captures_.size(); i-- > 0;)
            if (captures_[i].measured) return i;
        return std::nullopt;
    }
    std::vector<double> medians;
    for (std::size_t m = 0; m < profile_.measurands.size(); ++m) {
        std::vector<double> v;
        for (const auto i : accepted)
            v.push_back(*captures_[i].values[m]);
        medians.push_back(median(std::move(v)));
    }
    std::size_t best = accepted.front();
    double bestDistance = -1.0;
    for (const auto i : accepted) {
        double d = 0.0;
        for (std::size_t m = 0; m < medians.size(); ++m)
            d += std::abs(*captures_[i].values[m] - medians[m]);
        if (bestDistance < 0.0 || d < bestDistance) {
            bestDistance = d;
            best = i;
        }
    }
    return best;
}

Guidance ObjectSession::guidance() const {
    Guidance g;
    g.required = profile_.requiredRepeats;
    for (const auto& c : captures_)
        if (c.measured) ++g.measured;
    g.acceptedIndices = acceptedSet();
    g.accepted = g.acceptedIndices.size();

    // Repeat statistics over the accepted captures.
    for (std::size_t m = 0; m < profile_.measurands.size(); ++m) {
        MeasurandSummary s;
        s.label = profile_.measurands[m].label;
        s.unit = profile_.measurands[m].unit;
        std::vector<double> v;
        for (const auto i : g.acceptedIndices) {
            v.push_back(*captures_[i].values[m]);
            s.provenance.push_back(captures_[i].observationId + "#" +
                                   captures_[i].valueClaimIds[m]);
        }
        s.n = v.size();
        if (!v.empty()) {
            double sum = 0.0;
            for (const auto x : v)
                sum += x;
            s.mean = sum / static_cast<double>(v.size());
            const auto [lo, hi] = std::minmax_element(v.begin(), v.end());
            s.spread = *hi - *lo;
            if (v.size() > 1) {
                double sq = 0.0;
                for (const auto x : v)
                    sq += (x - s.mean) * (x - s.mean);
                s.stddev = std::sqrt(sq / static_cast<double>(v.size() - 1));
            }
        }
        g.summary.push_back(std::move(s));
    }

    // Agreement: every accepted capture must say the same thing, including
    // agreeing that a claim is absent (no nominal size for this part).
    std::size_t disagreeing = profile_.agreementClaims.size();
    for (std::size_t a = 0; a < profile_.agreementClaims.size(); ++a) {
        std::optional<std::string> value;
        bool agree = true;
        bool first = true;
        for (const auto i : g.acceptedIndices) {
            const auto& v = captures_[i].agreement[a];
            if (first) {
                value = v;
                first = false;
            } else if (v != value) {
                agree = false;
            }
        }
        g.consensus.push_back(agree ? value : std::nullopt);
        if (!agree) {
            g.agreementHolds = false;
            if (disagreeing == profile_.agreementClaims.size()) disagreeing = a;
        }
    }

    // Open questions: what the accepted (else all measured) captures could
    // not answer, with the view that would and whether this build can use it.
    std::vector<std::size_t> basis = g.acceptedIndices;
    if (basis.empty())
        for (std::size_t i = 0; i < captures_.size(); ++i)
            if (captures_[i].measured) basis.push_back(i);
    for (const auto i : basis)
        for (const auto& u : captures_[i].unresolved) {
            const bool seen = std::any_of(g.openQuestions.begin(), g.openQuestions.end(),
                                          [&](const OpenQuestion& q) { return q.field == u.name; });
            if (seen) continue;
            OpenQuestion q{u.name, u.reason, {}, false};
            if (const auto* rule = profile_.rule(u.name)) {
                q.resolvingView = rule->resolvingView;
                q.analyzerInBuild = rule->analyzerInBuild;
            } else {
                for (const auto& r : captures_[i].recommended)
                    if (std::find(r.resolves.begin(), r.resolves.end(), u.name) != r.resolves.end())
                        q.resolvingView = r.action;
            }
            g.openQuestions.push_back(std::move(q));
        }

    // Stage and the one instruction the operator sees.
    if (captures_.empty()) {
        g.stage = Stage::NeedMeasurement;
        g.instruction = "Place the part beside the 20 mm square and tap CAPTURE.";
    } else if (g.measured == 0) {
        g.stage = Stage::NeedMeasurement;
        g.instruction = captures_.back().refusal.empty()
                            ? "No measurement yet. Place the part beside the square."
                            : captures_.back().refusal;
    } else if (g.accepted < g.required) {
        g.stage = Stage::NeedRepeats;
        if (!captures_.back().measured && !captures_.back().refusal.empty()) {
            g.instruction = captures_.back().refusal;
        } else if (g.measured >= g.required) {
            // Enough attempts, but they do not agree: say by how much.
            double worst = 0.0;
            for (std::size_t m = 0; m < profile_.measurands.size(); ++m) {
                double lo = 0.0, hi = 0.0;
                bool first = true;
                for (const auto& c : captures_) {
                    if (!c.measured) continue;
                    const double x = *c.values[m];
                    lo = first ? x : std::min(lo, x);
                    hi = first ? x : std::max(hi, x);
                    first = false;
                }
                worst = std::max(worst, hi - lo);
            }
            g.instruction = format1(
                "Captures differ by %.1f mm. Check lighting and focus, then capture again.", worst);
        } else {
            const auto more = g.required - g.accepted;
            g.instruction = "Capture again: shift or rotate the part on the sheet (" +
                            std::to_string(more) + " more).";
        }
    } else {
        g.stage = Stage::Complete;
        if (!g.agreementHolds) {
            g.instruction = "Measurement is stable, but captures disagree on " +
                            profile_.agreementClaims[disagreeing] +
                            ". Capture again with the part flat and fully in view.";
        } else {
            g.modelSatisfied = true;
            g.instruction = "Measurement is stable. Tap FINISH to export.";
        }
    }
    return g;
}

}  // namespace platypus::session
