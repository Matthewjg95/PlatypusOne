#include "platypus/ai/FastenerClassifier.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <span>
#include <vector>

namespace platypus::ai {

std::string_view to_string(FastenerClass value) noexcept {
    switch (value) {
        case FastenerClass::Unknown:
            return "unknown";
        case FastenerClass::BoltOrScrew:
            return "bolt_or_screw";
        case FastenerClass::NutOrWasher:
            return "nut_or_washer";
    }
    return "unknown";
}

namespace {

constexpr std::string_view kMethod = "ai.fastener_classifier.v3";

/// Rod-like at or above this length/width ratio.
constexpr double kMinRodAspect = 2.5;
/// Compact (nut/washer candidate) at or below this ratio; a regular hexagon's
/// across-corners / across-flats is ~1.155, well inside.
constexpr double kMaxCompactAspect = 1.4;
/// A bolt or screw from above has a head: one end of the silhouette at least
/// this much wider than the shank. A rod without one (pin, dowel, pen) is not
/// claimed as a fastener.
constexpr double kMinHeadToShank = 1.2;
/// Outer-outline area over the inscribed-ellipse area of its extents: 1.0 for
/// a circle, ~0.96 for a hex nut, ~1.27 for a rectangle. Nuts and washers are
/// round or hexagonal; a rectangular part with a hole is not one.
constexpr double kMaxRoundFill = 1.12;
/// A nut or washer bore sits at the centre: offset over width.
constexpr double kMaxBoreOffset = 0.2;
/// Inference from a single silhouette is never certain.
constexpr double kMaxConfidence = 0.9;
/// Nominal matches worse than this relative error are not claimed at all.
constexpr double kMaxNominalFitError = 0.15;

struct TableEntry {
    const char* designation;
    double mm;
};

/// ISO 262 coarse metric shaft diameters.
constexpr std::array<TableEntry, 7> kShaftDiametersMm{{{"M3", 3.0},
                                                       {"M4", 4.0},
                                                       {"M5", 5.0},
                                                       {"M6", 6.0},
                                                       {"M8", 8.0},
                                                       {"M10", 10.0},
                                                       {"M12", 12.0}}};
/// ASME B1.1 UNC coarse major diameters. A silhouette resolves about
/// +-0.5 mm, which cannot separate most of these from their metric
/// neighbours (1/4-20 is 6.35 mm, M6 is 6.00): only the thread pitch can.
constexpr std::array<TableEntry, 8> kUncDiametersMm{{{"#4-40 UNC", 2.845},
                                                     {"#6-32 UNC", 3.505},
                                                     {"#8-32 UNC", 4.166},
                                                     {"#10-24 UNC", 4.826},
                                                     {"1/4-20 UNC", 6.35},
                                                     {"5/16-18 UNC", 7.938},
                                                     {"3/8-16 UNC", 9.525},
                                                     {"1/2-13 UNC", 12.7}}};
/// Sizes this close to the best match are indistinguishable from it.
constexpr double kShaftUncertaintyMm = 0.5;

/// ISO 4032 hex across-flats widths.
constexpr std::array<TableEntry, 7> kHexAcrossFlatsMm{{{"M3", 5.5},
                                                       {"M4", 7.0},
                                                       {"M5", 8.0},
                                                       {"M6", 10.0},
                                                       {"M8", 13.0},
                                                       {"M10", 16.0},
                                                       {"M12", 18.0}}};

std::string formatMm(double value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f", value);
    return buffer;
}

std::optional<NominalMatch> bestMatch(double measuredMm, std::span<const TableEntry> table,
                                      std::string basis) {
    if (measuredMm <= 0.0) return std::nullopt;
    // Nearest by ABSOLUTE distance: relative-error selection biases upward
    // between entries (16.99 mm would pick 18 over 16 because the larger
    // denominator forgives more). The gate and confidence stay relative.
    const TableEntry* best = nullptr;
    double bestAbs = 0.0;
    for (const auto& entry : table) {
        const double error = std::abs(measuredMm - entry.mm);
        if (!best || error < bestAbs) {
            best = &entry;
            bestAbs = error;
        }
    }
    if (!best) return std::nullopt;
    const double relative = bestAbs / best->mm;
    if (relative > kMaxNominalFitError) return std::nullopt;
    NominalMatch match;
    match.designation = best->designation;
    match.referenceMm = best->mm;
    match.fitError = relative;
    match.basis = std::move(basis);
    match.confidence = std::min(kMaxConfidence, 1.0 - relative / kMaxNominalFitError);
    return match;
}

double polygonArea(const std::vector<geometry::Vec2>& pts) {
    double a = 0.0;
    for (std::size_t i = 0; i < pts.size(); ++i) {
        const auto& p = pts[i];
        const auto& q = pts[(i + 1) % pts.size()];
        a += static_cast<double>(p.x) * q.y - static_cast<double>(q.x) * p.y;
    }
    return std::abs(a) / 2.0;
}

geometry::Vec2 centroid(const std::vector<geometry::Vec2>& pts) {
    double x = 0.0, y = 0.0;
    for (const auto& p : pts) {
        x += p.x;
        y += p.y;
    }
    const auto n = static_cast<double>(std::max<std::size_t>(1, pts.size()));
    return {static_cast<float>(x / n), static_cast<float>(y / n)};
}

FastenerClassification unknown(std::string why) {
    FastenerClassification r;
    r.rationale = std::move(why);
    return r;
}

/// Nearest shank size across metric and UNC. When the other standard has a
/// size within the measurement uncertainty, both are named and confidence is
/// halved: the thread pitch, not the silhouette, decides between them.
std::optional<NominalMatch> shaftMatch(double measuredMm) {
    auto metric = bestMatch(measuredMm, kShaftDiametersMm, "shaft_diameter");
    auto unc = bestMatch(measuredMm, kUncDiametersMm, "shaft_diameter");
    if (!metric && !unc) return std::nullopt;
    if (!metric || !unc) return metric ? metric : unc;
    const double dm = std::abs(measuredMm - metric->referenceMm);
    const double du = std::abs(measuredMm - unc->referenceMm);
    auto& best = dm <= du ? *metric : *unc;
    const auto& other = dm <= du ? *unc : *metric;
    if (std::max(dm, du) - std::min(dm, du) > kShaftUncertaintyMm) return best;
    best.designation += " or " + other.designation;
    best.alternative = other.designation;
    best.confidence *= 0.5;
    return best;
}

}  // namespace

std::optional<WidthProfile> widthProfile(const geometry::Outline2& outline, double axisAngleRad) {
    if (outline.empty()) return std::nullopt;
    // Walk the outer outline densely (simplification leaves long straight
    // edges with no vertices) in the part's own frame: t along the length
    // axis, s across it. Width at t is the spread of s.
    const double c = std::cos(axisAngleRad), sn = std::sin(axisAngleRad);
    std::vector<std::pair<double, double>> ts;
    const auto& o = outline.outer;
    for (std::size_t i = 0; i < o.size(); ++i) {
        const auto& p = o[i];
        const auto& q = o[(i + 1) % o.size()];
        const double len = std::hypot(q.x - p.x, q.y - p.y);
        const int steps = std::max(1, static_cast<int>(std::ceil(len / 0.5)));
        for (int k = 0; k < steps; ++k) {
            const double f = static_cast<double>(k) / steps;
            const double x = p.x + (q.x - p.x) * f, y = p.y + (q.y - p.y) * f;
            ts.emplace_back(x * c + y * sn, -x * sn + y * c);
        }
    }
    double tMin = ts.front().first, tMax = tMin;
    for (const auto& pt : ts) {
        tMin = std::min(tMin, pt.first);
        tMax = std::max(tMax, pt.first);
    }
    constexpr int kBins = 24;
    const double span = tMax - tMin;
    if (span <= 0.0) return std::nullopt;
    std::array<double, kBins> lo{}, hi{};
    std::array<bool, kBins> seen{};
    for (const auto& pt : ts) {
        const int b = std::min(kBins - 1, static_cast<int>((pt.first - tMin) / span * kBins));
        if (!seen[b]) {
            lo[b] = hi[b] = pt.second;
            seen[b] = true;
        } else {
            lo[b] = std::min(lo[b], pt.second);
            hi[b] = std::max(hi[b], pt.second);
        }
    }
    std::vector<double> middle;
    double endA = 0.0, endB = 0.0;
    for (int b = 0; b < kBins; ++b) {
        if (!seen[b]) continue;
        const double width = hi[b] - lo[b];
        // The outer 3 bins (12.5%) are each end; a head is rarely shorter.
        if (b < 3)
            endA = std::max(endA, width);
        else if (b >= kBins - 3)
            endB = std::max(endB, width);
        else if (b >= kBins / 5 && b < kBins - kBins / 5)
            middle.push_back(width);
    }
    if (middle.empty()) return std::nullopt;
    std::sort(middle.begin(), middle.end());
    WidthProfile w;
    w.shankPx = middle[middle.size() / 2];
    if (w.shankPx <= 0.0) return std::nullopt;
    w.endToShank = std::max(endA, endB) / w.shankPx;
    return w;
}

FastenerClassification classify(const vision::ScoutAnalysis& analysis) {
    FastenerClassification result;
    const double lengthMm = analysis.subjectLengthMm;
    const double widthMm = analysis.subjectWidthMm;
    if (widthMm <= 0.0 || lengthMm <= 0.0) {
        result.rationale = "degenerate subject extents; nothing to classify";
        return result;
    }
    const double aspect = lengthMm / widthMm;
    const auto& outline = analysis.subjectOutlinePx;
    const std::size_t holes = analysis.subject.holeCount;

    // Every fastener claim needs positive evidence for that family, not just
    // a proportion: anything else is outside this build's library and is said
    // to be, rather than forced into the nearest class.
    if (aspect >= kMinRodAspect) {
        if (holes > 0)
            return unknown("rod-like (aspect " + formatMm(aspect) + ") but with " +
                           std::to_string(holes) + " hole(s); bolts and screws have none");
        const auto profile = widthProfile(outline, analysis.subject.majorAxisAngleRad);
        if (!profile) return unknown("rod-like but no outline to check for a head");
        if (profile->endToShank < kMinHeadToShank)
            return unknown("rod-like (aspect " + formatMm(aspect) + ") with no head (end/shank " +
                           formatMm(profile->endToShank) +
                           "); could be a pin, rod or pen - not claimed as a screw");
        FastenerClassification bolt;
        bolt.fastenerClass = FastenerClass::BoltOrScrew;
        bolt.confidence = std::min(kMaxConfidence, 0.55 + 0.08 * (aspect - kMinRodAspect));
        bolt.shankWidthMm = profile->shankPx * analysis.mmPerPixel;
        bolt.endToShank = profile->endToShank;
        // The shank, not the overall width: the overall width of a part with
        // a head is the head, which would read one or two sizes too big.
        bolt.nominal = shaftMatch(*bolt.shankWidthMm);
        bolt.rationale = "rod-like silhouette (aspect " + formatMm(aspect) +
                         ") with a head (end/shank " + formatMm(profile->endToShank) + ")";
        return bolt;
    }

    if (aspect <= kMaxCompactAspect) {
        if (holes != 1)
            return unknown(holes == 0 ? "compact silhouette without a bore; not a nut or washer"
                                      : "compact with " + std::to_string(holes) +
                                            " holes; a nut or washer has exactly one");
        if (outline.empty() || outline.holes.size() != 1)
            return unknown("compact with a bore, but no outline to check its shape");
        const double ellipse =
            3.14159265358979 / 4.0 * analysis.subject.lengthPx * analysis.subject.widthPx;
        const double fill = polygonArea(outline.outer) / ellipse;
        if (fill > kMaxRoundFill)
            return unknown("compact with a bore but a squared-off outline (fill " + formatMm(fill) +
                           "); nuts and washers are round or hexagonal");
        const auto co = centroid(outline.outer);
        const auto ch = centroid(outline.holes.front());
        const double offset = std::hypot(co.x - ch.x, co.y - ch.y) / analysis.subject.widthPx;
        if (offset > kMaxBoreOffset)
            return unknown("compact with an off-centre hole (offset " + formatMm(offset) +
                           " of width); not a nut or washer");
        FastenerClassification nut;
        nut.fastenerClass = FastenerClass::NutOrWasher;
        nut.confidence = aspect <= 1.2 ? 0.75 : 0.6;
        nut.nominal = bestMatch(widthMm, kHexAcrossFlatsMm, "hex_across_flats");
        nut.rationale =
            "round/hex silhouette (aspect " + formatMm(aspect) + ") with a centred bore";
        return nut;
    }

    return unknown("aspect " + formatMm(aspect) +
                   " is neither rod-like nor compact; outside the fastener library");
}

void appendClassification(observation::EngineeringObservation& record,
                          const FastenerClassification& classification) {
    const std::string method(kMethod);

    // The analyzer parked these as "not attempted"; classification has now
    // been attempted, so they are restated below with what actually remains.
    std::erase_if(record.unresolved, [](const observation::Unresolved& item) {
        return item.name == "fastener_class" || item.name == "nominal_size";
    });

    if (classification.fastenerClass == FastenerClass::Unknown) {
        // Not a fastener this build recognizes: the fastener-only questions
        // (thread pitch, nominal size) do not apply and are not asked. The
        // record keeps the measurement and says plainly what it is not.
        std::erase_if(record.unresolved, [](const observation::Unresolved& item) {
            return item.name == "thread_pitch";
        });
        std::erase_if(record.recommendedNextObservations,
                      [](const observation::RecommendedObservation& r) {
                          return std::find(r.resolves.begin(), r.resolves.end(), "thread_pitch") !=
                                 r.resolves.end();
                      });
        record.unresolved.push_back(
            {"object_class", "not a recognized fastener: " + classification.rationale});
        return;
    }

    record.inferred.push_back(
        observation::Claim{"fc-class",
                           "fastener_class",
                           std::string(to_string(classification.fastenerClass)),
                           std::nullopt,
                           classification.confidence,
                           {"sa-subj-length-mm", "sa-subj-width-mm", "sa-subj-holes"},
                           method + "; " + classification.rationale});

    if (classification.shankWidthMm) {
        record.derived.push_back(observation::Claim{
            "fc-shank-mm",
            "shank_width",
            *classification.shankWidthMm,
            "mm",
            std::nullopt,
            {"sa-mm-per-px", "sa-subj-axis-rad"},
            method + "; median outline width over the middle 60% of the length axis"});
    }

    if (classification.nominal) {
        const auto& nominal = *classification.nominal;
        char detail[128];
        std::snprintf(detail, sizeof(detail),
                      "; nearest %s table entry %.2f mm, relative error %.3f",
                      nominal.basis.c_str(), nominal.referenceMm, nominal.fitError);
        record.inferred.push_back(observation::Claim{
            "fc-nominal",
            "nominal_size",
            nominal.designation,
            std::nullopt,
            nominal.confidence,
            {"fc-class", classification.shankWidthMm ? "fc-shank-mm" : "sa-subj-width-mm"},
            method + detail});
    } else {
        record.unresolved.push_back(
            {"nominal_size", "no standard metric size within tolerance of the measured width"});
    }

    if (classification.fastenerClass == FastenerClass::BoltOrScrew && classification.nominal &&
        !classification.nominal->alternative.empty()) {
        for (auto& item : record.unresolved)
            if (item.name == "thread_pitch")
                item.reason = "metric or UNC: " + classification.nominal->designation +
                              " are within the silhouette's uncertainty; the thread pitch "
                              "decides, and needs a side view";
    }

    if (classification.fastenerClass == FastenerClass::BoltOrScrew) {
        record.unresolved.push_back(
            {"bolt_vs_screw", "head style is not visible in a top-down silhouette"});
        record.recommendedNextObservations.push_back(
            {"capture the head from the side to distinguish bolt from screw and read the drive",
             {"bolt_vs_screw"}});
    } else {
        record.unresolved.push_back(
            {"nut_vs_washer", "thickness is not visible in a top-down silhouette"});
        record.recommendedNextObservations.push_back(
            {"capture a side profile to measure thickness and separate nut from washer",
             {"nut_vs_washer"}});
    }
}

}  // namespace platypus::ai
