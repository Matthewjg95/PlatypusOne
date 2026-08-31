// PlatypusOS services — Scout analyzer: calibration + deterministic measurement.
//
// MVP build-order step 3 (docs/contest/DIGIKEY_ENGINEERING_SCOUT_MVP.md): given
// one frame containing a dark calibration square of known physical size and a
// dark subject on a light background, recover a mm-per-pixel scale from the
// reference and measure the subject's principal-axis extents in millimetres.
//
// Everything here is deterministic geometry — Otsu binarization, connected
// components, second-moment principal axes. No ML, no external dependencies;
// classification is a later Scout chunk and is reported as UNRESOLVED, never
// guessed (contract rule: inference must not masquerade as measurement).
//
// v1 scene contract (documented limits, enforced by error codes, not UB):
//   - background is lighter than the reference and subject
//   - the reference is a filled square, roughly axis-aligned (±~8°; the
//     fill-ratio gate rejects stronger rotations rather than mismeasuring)
//   - reference and subject do not touch
#pragma once

#include <platypus/hal/ICamera.hpp>
#include <platypus/observation/Observation.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace platypus::vision {

/// Physical description of the calibration reference in the scene.
struct CalibrationSpec {
    double referenceSideMm = 20.0;  ///< side length of the filled square
};

/// Pixel-space statistics for one segmented blob.
struct BlobStats {
    std::int32_t minX = 0;  ///< inclusive bounding box
    std::int32_t minY = 0;
    std::int32_t maxX = 0;
    std::int32_t maxY = 0;
    std::size_t areaPx = 0;
    double centroidX = 0.0;
    double centroidY = 0.0;
    double fillRatio = 0.0;          ///< areaPx / bounding-box area
    double majorAxisAngleRad = 0.0;  ///< principal axis vs +x, [-pi/2, pi/2)
    double lengthPx = 0.0;           ///< extent along the major principal axis
    double widthPx = 0.0;            ///< extent along the minor principal axis
};

/// Everything measured from one frame. Pixel facts are OBSERVED evidence;
/// the mm values are DERIVED through the calibration scale.
struct ScoutAnalysis {
    std::uint8_t binarizationThreshold = 0;  ///< Otsu result actually applied
    BlobStats reference;
    BlobStats subject;
    double mmPerPixel = 0.0;  ///< referenceSideMm / sqrt(reference.areaPx)
    double subjectLengthMm = 0.0;
    double subjectWidthMm = 0.0;
};

enum class AnalyzeError : std::uint8_t {
    None = 0,
    InvalidFrame,        ///< empty frame or pixel buffer inconsistent with mode
    UnsupportedFormat,   ///< analyzer accepts RGB888 and Gray8 only
    NoReferenceTarget,   ///< no blob passes the square-reference gates
    ReferenceAmbiguous,  ///< two comparable square candidates; scene must have one
    NoSubject,           ///< nothing measurable besides the reference
};

[[nodiscard]] std::string_view to_string(AnalyzeError error) noexcept;

/// DecodeOutcome-style result: vision failures are scene conditions the UI
/// must explain to the operator, not HAL faults, so they carry their own enum.
struct AnalyzeOutcome {
    std::optional<ScoutAnalysis> analysis;
    AnalyzeError error = AnalyzeError::None;
    [[nodiscard]] bool ok() const noexcept { return analysis.has_value(); }
};

/// Analyze one frame. Deterministic: identical frames produce identical
/// results on every run and platform.
[[nodiscard]] AnalyzeOutcome analyzeFrame(const hal::Frame& frame, const CalibrationSpec& spec);

/// Append the analysis to a record as contract evidence:
///   OBSERVED  — pixel-space facts (threshold, areas, extents, axis angle)
///   DERIVED   — mm/px scale and the subject's mm dimensions, with provenance
///               chains back to the observed claims and the source artifact
///   UNRESOLVED — fastener_class / nominal_size / thread_pitch, with reasons
/// plus one recommended next observation for the thread pitch.
/// sourceArtifactId must be the id of the frame's artifact in the record.
void appendEvidence(observation::EngineeringObservation& record, const ScoutAnalysis& analysis,
                    const CalibrationSpec& spec, std::string_view sourceArtifactId);

}  // namespace platypus::vision
