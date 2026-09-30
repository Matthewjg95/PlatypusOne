// PlatypusOS services — Scout sessions: repeated captures of one object.
//
// A capture says what one frame shows; a session says what is known about the
// object. It groups captures, tells the operator what to capture next, and on
// finish hands the result to CAD (docs/architecture/SCOUT_SESSIONS_AND_BEYOND.md).
//
// Contract rules, kept structurally:
//   - Captures stay independent observation records; a session only
//     references them by id and never rewrites them.
//   - Aggregation is repeat statistics of the *same* measurand across
//     accepted captures, with provenance to every contributing claim — not
//     multi-view fusion, which the contract defers.
//   - An open question is reported with the view that would answer it and
//     whether this build can analyze that view.
//
// Guidance runs in three stages: get a measurement, make it repeatable, then
// complete the picture. "Accepted" captures are those agreeing with the
// median of all measured captures, so one bad frame (the part moved, the light
// changed) is set aside and reported rather than poisoning the estimate.
#pragma once

#include <platypus/geometry/Types.hpp>
#include <platypus/observation/Observation.hpp>
#include <platypus/session/ObjectProfile.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace platypus::session {

/// 8-bit image kept in memory for the finish-time PNG artifact.
struct Image {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t channels = 3;  ///< 1 gray, 3 RGB
    std::vector<std::uint8_t> pixels;
};

/// What a session keeps of one capture.
struct Capture {
    std::string observationId;
    std::string timestampUtc;
    bool measured = false;
    std::string refusal;  ///< operator-facing reason when not measured
    /// Per profile measurand, in profile order; nullopt when absent.
    std::vector<std::optional<double>> values;
    std::vector<std::string> valueClaimIds;  ///< for provenance, parallel to values
    /// Per profile agreement claim, in profile order.
    std::vector<std::optional<std::string>> agreement;
    std::vector<observation::Unresolved> unresolved;
    std::vector<observation::RecommendedObservation> recommended;
    geometry::Outline2 outlinePx;
    double mmPerPx = 0.0;
    std::optional<Image> preview;
};

/// Reads a capture out of its observation record by claim name. `refusal` is
/// the analyzer's reason when the scene could not be measured; the record of
/// a refused scene carries no claims to explain itself.
[[nodiscard]] Capture captureFrom(const observation::EngineeringObservation& record,
                                  const ObjectProfile& profile, std::string refusal = {});

enum class Stage { NeedMeasurement, NeedRepeats, Complete };
enum class CloseReason { ModelSatisfied, ClosedByOperator };

[[nodiscard]] std::string_view to_string(Stage stage) noexcept;
[[nodiscard]] std::string_view to_string(CloseReason reason) noexcept;

struct MeasurandSummary {
    std::string label;
    std::string unit;
    std::size_t n = 0;
    double mean = 0.0;
    double spread = 0.0;                  ///< max - min over accepted captures
    double stddev = 0.0;                  ///< sample standard deviation (0 when n < 2)
    std::vector<std::string> provenance;  ///< "<observation_id>#<claim_id>"
};

struct OpenQuestion {
    std::string field;
    std::string reason;
    std::string resolvingView;
    bool analyzerInBuild = false;
};

struct Guidance {
    Stage stage = Stage::NeedMeasurement;
    std::string instruction;  ///< one line for the operator
    bool modelSatisfied = false;
    std::size_t measured = 0;
    std::size_t accepted = 0;
    std::size_t required = 0;
    std::vector<std::size_t> acceptedIndices;  ///< into captures()
    std::vector<MeasurandSummary> summary;     ///< over accepted captures
    /// Per agreement claim: the consensus value, or nullopt when accepted
    /// captures disagree or none carries it.
    std::vector<std::optional<std::string>> consensus;
    bool agreementHolds = true;
    std::vector<OpenQuestion> openQuestions;
};

class ObjectSession {
   public:
    ObjectSession(std::string id, ObjectProfile profile, std::string openedUtc);

    void add(Capture capture);

    [[nodiscard]] Guidance guidance() const;
    /// The accepted capture closest to the accepted median — the one whose
    /// outline and image represent the object in exports.
    [[nodiscard]] std::optional<std::size_t> representativeCapture() const;

    [[nodiscard]] const std::string& id() const noexcept { return id_; }
    [[nodiscard]] const ObjectProfile& profile() const noexcept { return profile_; }
    [[nodiscard]] const std::string& openedUtc() const noexcept { return openedUtc_; }
    [[nodiscard]] const std::vector<Capture>& captures() const noexcept { return captures_; }

   private:
    [[nodiscard]] std::vector<std::size_t> acceptedSet() const;

    std::string id_;
    ObjectProfile profile_;
    std::string openedUtc_;
    std::vector<Capture> captures_;
};

}  // namespace platypus::session
