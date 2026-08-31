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
    BoltOrScrew,  ///< rod-like silhouette; head style unresolved from above
    NutOrWasher,  ///< compact silhouette with a bore; thickness unresolved
};

[[nodiscard]] std::string_view to_string(FastenerClass value) noexcept;

/// Nearest standard metric size for a measured dimension.
struct NominalMatch {
    std::string designation;   ///< e.g. "M6"
    double referenceMm = 0.0;  ///< the table value the measurement matched
    double fitError = 0.0;     ///< relative error |measured - reference| / reference
    std::string basis;         ///< "shaft_diameter" or "hex_across_flats"
    double confidence = 0.0;   ///< 0..1, decays with fitError
};

struct FastenerClassification {
    FastenerClass fastenerClass = FastenerClass::Unknown;
    double confidence = 0.0;              ///< 0..1; 0 when Unknown
    std::optional<NominalMatch> nominal;  ///< absent when no table entry fits
    std::string rationale;                ///< deterministic, human-readable why
};

/// Classify one analyzed scene. Total and deterministic: every input yields a
/// classification (possibly Unknown with the reason in rationale).
[[nodiscard]] FastenerClassification classify(const vision::ScoutAnalysis& analysis);

/// Append the classification to a record that already carries the analyzer's
/// evidence (vision::appendEvidence):
///   INFERRED   — fastener_class and, when matched, nominal_size; confidence,
///                provenance to the analyzer's claims, and method are always
///                present
///   UNRESOLVED — the analyzer's "not attempted" placeholders for
///                fastener_class / nominal_size are replaced by what genuinely
///                remains open (bolt vs screw, nut vs washer, or the reason
///                nothing could be inferred)
/// plus a recommended observation when the remaining ambiguity has a concrete
/// next step.
void appendClassification(observation::EngineeringObservation& record,
                          const FastenerClassification& classification);

}  // namespace platypus::ai
