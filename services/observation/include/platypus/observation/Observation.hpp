// PlatypusOS services — Engineering Observation contract v0.1 (C++ mirror).
//
// Implements docs/architecture/ENGINEERING_OBSERVATION_CONTRACT.md: the
// evidence model that keeps observed / derived / inferred / unresolved
// strictly distinct, with provenance on every claim. This module owns the
// record types, JSON (de)serialization, and contract validation ONLY —
// capture (camera), inference (AI), and presentation (UI) live elsewhere and
// consume these types.
//
// Contract rules enforced by validate():
//   - inferred claims must carry confidence, provenance, and method (§design
//     constraints 4–5)
//   - confidence, when present, is within [0, 1]
//   - claim ids are unique; provenance references resolve to a known claim
//     id, artifact id, or "artifact:" prefixed identifier
//   - inference never overwrites evidence: enforced structurally (separate,
//     append-only vectors; there is no replace API)
#pragma once

#include "Json.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace platypus::observation {

inline constexpr std::string_view kSchemaVersion = "0.1";

/// Claim values are physical quantities, counts, flags, or labels.
using ClaimValue = std::variant<double, bool, std::string>;

/// One evidence claim. Which vector it lives in (observed/derived/inferred)
/// is its evidence class — there is deliberately no class field to get out of
/// sync with placement.
struct Claim {
    std::string id;                        ///< unique within the record, e.g. "drv-diameter"
    std::string name;                      ///< e.g. "shaft_diameter"
    ClaimValue value;
    std::optional<std::string> unit;       ///< SI unit string when applicable
    std::optional<double> confidence;      ///< 0..1; required for inferred claims
    std::vector<std::string> provenance;   ///< claim ids / artifact ids this rests on
    std::string method;                    ///< measurement/algorithm/model identity
};

/// A raw input the record references: image, crop, sensor dump, calibration
/// detection. Files live beside the JSON record (contract: no database).
struct Artifact {
    std::string id;                        ///< e.g. "image-0042"
    std::string kind;                      ///< e.g. "image/png", "calibration"
    std::string path;                      ///< relative to the record's directory
};

struct Unresolved {
    std::string name;                      ///< e.g. "thread_pitch"
    std::string reason;                    ///< why current evidence is insufficient
};

struct RecommendedObservation {
    std::string action;                    ///< concrete acquisition instruction
    std::vector<std::string> resolves;     ///< names of unresolved items it targets
};

enum class ReviewState { Pending, Accepted, Rejected, Corrected };

[[nodiscard]] std::string_view to_string(ReviewState state) noexcept;
[[nodiscard]] std::optional<ReviewState> reviewStateFromString(std::string_view text) noexcept;

struct HumanReview {
    ReviewState state = ReviewState::Pending;
    std::optional<std::string> notes;
};

/// One capture/inspection event.
struct EngineeringObservation {
    std::string observationId;             ///< stable unique id, e.g. "scan-0042"
    std::string timestampUtc;              ///< ISO 8601; producer supplies the clock
    /// Producer identity + device metadata, e.g. {"app","engineering_scout"},
    /// {"camera","uvc0"}. Kept as open key/value pairs: the contract is
    /// sensor-agnostic and sources differ per app. Keys must be unique —
    /// validate() reports duplicates, and toJson() keeps only the first
    /// occurrence so serialized output always re-parses.
    std::vector<std::pair<std::string, std::string>> source;
    std::vector<Artifact> artifacts;
    std::vector<Claim> observed;
    std::vector<Claim> derived;
    std::vector<Claim> inferred;
    std::vector<Unresolved> unresolved;
    std::vector<RecommendedObservation> recommendedNextObservations;
    HumanReview humanReview;
};

/// Serializes in contract field order with a schema_version stamp. Output is
/// deterministic (stable ordering) so records diff cleanly in git.
[[nodiscard]] std::string toJson(const EngineeringObservation& record);

struct DecodeOutcome {
    std::optional<EngineeringObservation> record;
    std::string error;                     ///< first problem found, empty on success
    [[nodiscard]] bool ok() const noexcept { return record.has_value(); }
};

/// Strict on structure it understands, tolerant of unknown fields (forward
/// compatibility, mirroring the MCU-bridge "ignore unknown topics" rule).
/// observation_id is the only decode-required field: decoding stays tolerant
/// so a draft/damaged record can be loaded for repair, while contract
/// completeness (timestamp_utc present, etc.) is validate()'s job.
[[nodiscard]] DecodeOutcome fromJson(std::string_view text);

/// Returns every contract violation found (empty = valid). Violations are
/// human-readable and stable enough to assert on in tests.
[[nodiscard]] std::vector<std::string> validate(const EngineeringObservation& record);

}  // namespace platypus::observation
