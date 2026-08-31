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

constexpr std::string_view kMethod = "ai.fastener_classifier.v2";

/// Rod-like at or above this length/width ratio.
constexpr double kMinRodAspect = 2.5;
/// Compact (nut/washer candidate) at or below this ratio; a regular hexagon's
/// across-corners / across-flats is ~1.155, well inside.
constexpr double kMaxCompactAspect = 1.4;
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

}  // namespace

FastenerClassification classify(const vision::ScoutAnalysis& analysis) {
    FastenerClassification result;
    const double lengthMm = analysis.subjectLengthMm;
    const double widthMm = analysis.subjectWidthMm;
    if (widthMm <= 0.0 || lengthMm <= 0.0) {
        result.rationale = "degenerate subject extents; nothing to classify";
        return result;
    }
    const double aspect = lengthMm / widthMm;

    if (aspect >= kMinRodAspect) {
        result.fastenerClass = FastenerClass::BoltOrScrew;
        result.confidence = std::min(kMaxConfidence, 0.55 + 0.08 * (aspect - kMinRodAspect));
        // Shaft table only: the minor extent of a mostly-rod silhouette is the
        // shank. The shaft and across-flats tables overlap (12 mm shank vs
        // 13 mm M8 head), so mixing them makes the match ambiguous; a
        // head-dominated outline is a documented limitation, not a guess.
        result.nominal = bestMatch(widthMm, kShaftDiametersMm, "shaft_diameter");
        result.rationale = "rod-like silhouette (aspect " + formatMm(aspect) +
                           " >= " + formatMm(kMinRodAspect) + ")";
        return result;
    }

    if (aspect <= kMaxCompactAspect && analysis.subject.holeCount >= 1) {
        result.fastenerClass = FastenerClass::NutOrWasher;
        result.confidence = aspect <= 1.2 ? 0.75 : 0.6;
        result.nominal = bestMatch(widthMm, kHexAcrossFlatsMm, "hex_across_flats");
        result.rationale = "compact silhouette (aspect " + formatMm(aspect) + ") with " +
                           std::to_string(analysis.subject.holeCount) + " bore(s)";
        return result;
    }

    result.rationale = aspect <= kMaxCompactAspect
                           ? "compact silhouette without a bore; not a recognizable fastener family"
                           : "aspect " + formatMm(aspect) + " between compact and rod-like gates";
    return result;
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
        record.unresolved.push_back({"fastener_class", classification.rationale});
        record.unresolved.push_back(
            {"nominal_size", "no fastener class; nominal matching not applicable"});
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

    if (classification.nominal) {
        const auto& nominal = *classification.nominal;
        char detail[128];
        std::snprintf(detail, sizeof(detail),
                      "; nearest %s table entry %.2f mm, relative error %.3f",
                      nominal.basis.c_str(), nominal.referenceMm, nominal.fitError);
        record.inferred.push_back(observation::Claim{"fc-nominal",
                                                     "nominal_size",
                                                     nominal.designation,
                                                     std::nullopt,
                                                     nominal.confidence,
                                                     {"fc-class", "sa-subj-width-mm"},
                                                     method + detail});
    } else {
        record.unresolved.push_back(
            {"nominal_size", "no standard metric size within tolerance of the measured width"});
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
