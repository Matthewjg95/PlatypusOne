// PlatypusOS services — fastener classification and nominal-size matching.
//
// MVP build-order step 4 (docs/contest/DIGIKEY_ENGINEERING_SCOUT_MVP.md):
// given a ScoutAnalysis, infer the fastener family and the likely metric
// nominal size. Deliberately rule-based and deterministic for v1 — the
// classification interface is the contract; a learned model can replace the
// rules behind it later without touching callers.
//
// Everything this module produces is INFERRED evidence: it always carries
// confidence, provenance, and method (contract §design constraints 4–5), is
// capped below certainty (a single silhouette can never be sure), and what
// the silhouette cannot answer stays UNRESOLVED (bolt vs screw, nut vs
// washer) rather than being guessed.
#pragma once

#include <platypus/observation/Observation.hpp>
#include <platypus/vision/ScoutAnalyzer.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace platypus::ai {

enum class FastenerClass : std::uint8_t {
    Unknown = 0,
    BoltOrScrew,  ///< rod-like silhouette with a head; head style unresolved from above
    NutOrWasher,  ///< compact with a centred bore, outline between round and hex
    Washer,       ///< round outline with a centred bore
    Nut,          ///< hexagonal outline with a centred bore
};

[[nodiscard]] std::string_view to_string(FastenerClass value) noexcept;

/// Nearest standard metric size for a measured dimension.
struct NominalMatch {
    std::string designation;   ///< e.g. "M6"
    double referenceMm = 0.0;  ///< the table value the measurement matched
    double fitError = 0.0;     ///< relative error |measured - reference| / reference
    std::string basis;         ///< "shaft_diameter", "hex_across_flats" or "bore_clearance"
    /// Set when a size from the other standard (metric vs UNC) is equally
    /// consistent with the measurement; designation then names both.
    std::string alternative;
    double confidence = 0.0;  ///< 0..1, decays with fitError
};

struct FastenerClassification {
    FastenerClass fastenerClass = FastenerClass::Unknown;
    double confidence = 0.0;              ///< 0..1; 0 when Unknown
    std::optional<NominalMatch> nominal;  ///< absent when no table entry fits
    std::optional<double> shankWidthMm;   ///< bolts/screws: the shank, not the head
    double endToShank = 0.0;              ///< bolts/screws: head width over shank width
    std::optional<double> boreMm;         ///< nuts/washers: the centred hole's diameter
    std::string rationale;                ///< deterministic, human-readable why
};

/// Outline width along the part's length axis.
struct WidthProfile {
    double shankPx = 0.0;     ///< median width over the middle 60% of the length
    double endToShank = 0.0;  ///< widest end (outer 12.5%) over the shank
};
[[nodiscard]] std::optional<WidthProfile> widthProfile(const geometry::Outline2& outline,
                                                       double axisAngleRad);

/// Classify one analyzed scene. Total and deterministic: every input yields a
/// classification (possibly Unknown with the reason in rationale). A class is
/// claimed only on positive evidence for that family: a rod with a head for
/// bolt/screw; a round or hex outline with one centred bore for nut/washer.
/// Everything else (a PCB, a pen, a bracket) is Unknown: outside the library,
/// never forced into the nearest fastener.
[[nodiscard]] FastenerClassification classify(const vision::ScoutAnalysis& analysis);

/// Append the classification to a record that already carries the analyzer's
/// evidence (vision::appendEvidence):
///   INFERRED   — fastener_class and, when matched, nominal_size; confidence,
///                provenance to the analyzer's claims, and method are always
///                present
///   UNRESOLVED — only the questions the inferred family raises: thread pitch
///                and bolt vs screw for a bolt; thickness for a washer; height
///                and the internal thread for a nut; nut vs washer when the
///                outline is neither clearly round nor hex; for an unknown
///                part, what it is not and its thickness. The analyzer raises
///                none (docs/architecture/AI_PIPELINE.md).
/// plus a recommended observation when a capture would answer one.
void appendClassification(observation::EngineeringObservation& record,
                          const FastenerClassification& classification);

}  // namespace platypus::ai
