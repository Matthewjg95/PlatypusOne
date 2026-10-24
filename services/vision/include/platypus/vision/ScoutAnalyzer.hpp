// PlatypusOS services — Scout analyzer: calibration + deterministic measurement.
//
// MVP build-order step 3 (docs/contest/DIGIKEY_ENGINEERING_SCOUT_MVP.md): given
// one frame containing a dark calibration square of known physical size and a
// dark subject on a light background, recover a mm-per-pixel scale from the
// reference and measure the subject's principal-axis extents in millimetres.
//
// Everything here is deterministic geometry — Otsu binarization, connected
// components, minimum-support-width measurement. No ML, no external dependencies;
// classification is a later Scout chunk and is reported as UNRESOLVED, never
// guessed (contract rule: inference must not masquerade as measurement).
//
// v1 scene contract (documented limits, enforced by error codes, not UB):
//   - background is lighter than the reference and subject
//   - the reference is a filled square, roughly axis-aligned (±~8°; the
//     fill-ratio gate rejects stronger rotations rather than mismeasuring)
//   - reference and subject do not touch
#pragma once

#include <platypus/geometry/Types.hpp>
#include <platypus/hal/ICamera.hpp>
#include <platypus/observation/Observation.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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
    std::size_t holeCount = 0;       ///< enclosed background regions (nut/washer bores)
    double majorAxisAngleRad = 0.0;  ///< length axis vs +x, [-pi/2, pi/2)
    double lengthPx = 0.0;           ///< support extent along the length axis
    double widthPx = 0.0;            ///< MINIMUM support width over a 1-deg sweep —
                                     ///< rotation-invariant (hexagon: across-flats)
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
    /// Camera tilt against the table, read from how the square is squashed:
    /// the ratio of the singular values of its two edge vectors (independent
    /// of how the card is rotated). A lower bound: tilts under ~10 deg are
    /// within corner noise at bench scale.
    double cameraTiltDeg = 0.0;
    /// Largest opposite-side length ratio minus one: perspective keystone.
    double referenceKeystone = 0.0;
    /// The subject's silhouette in pixel coordinates: outer boundary plus
    /// bores, glints filtered out. Scale by mmPerPixel for millimetres; this
    /// is what leaves PlatypusOne as a CAD sketch (services/export).
    geometry::Outline2 subjectOutlinePx;
    /// Equal-area diameters of the outline's holes (those kept: >= 4 mm^2),
    /// in outline order. Geometry only; whether a hole is a bore is inference.
    std::vector<double> holeDiametersMm;
    /// Six-fold harmonic of the outer outline's radius about its centroid,
    /// sampled along the perimeter, as a fraction of the mean radius: ~0 for a
    /// circle (or a square), 0.060 for a regular hexagon.
    double outlineSixFold = 0.0;
};

enum class AnalyzeError : std::uint8_t {
    None = 0,
    InvalidFrame,        ///< empty frame or pixel buffer inconsistent with mode
    UnsupportedFormat,   ///< analyzer accepts Gray8, RGB888 and YUYV only
    NoReferenceTarget,   ///< no blob passes the square-reference gates
    ReferenceAmbiguous,  ///< two comparable square candidates; scene must have one
    NoSubject,           ///< nothing measurable besides the reference
};

[[nodiscard]] std::string_view to_string(AnalyzeError error) noexcept;

/// Tilt at or above which the operator is warned: sizes read high because the
/// square and the part are no longer at the same scale.
inline constexpr double kTiltWarningDeg = 12.0;

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
///   (no UNRESOLVED and no family-specific recommendation: perception
///   states geometry; what remains open depends on what the part is, which
///   is the reasoner's call — docs/architecture/AI_PIPELINE.md)
/// plus a tilt recommendation when the camera is tilted.
/// sourceArtifactId must be the id of the frame's artifact in the record.
// --- best-effort inspection -------------------------------------------------
//
// analyzeFrame refuses a scene it cannot measure in millimetres. That is right
// for a measurement, but on a poor bench it leaves nothing to work with (bench,
// 2026-10-18: seven refused captures in a dim room with the operator's shadow
// on the card). inspectFrame always reports what the frame does support:
//   - capture quality, on every frame;
//   - the strict analysis, unchanged, when the scene passes;
//   - otherwise the pixel facts of the largest part, and, when some blob is
//     nearly square, a scale through it at reduced confidence, naming the
//     reference checks it failed.
// The fallback claims are all named "best_effort_*", never the strict names,
// so nothing that aggregates measurements (sessions, the classifier) can take
// one for a measurement.

/// Capture-quality facts, measured on luma before illumination flattening.
struct FrameQuality {
    double lumaP01 = 0.0, lumaP50 = 0.0, lumaP99 = 0.0;  ///< 0..255
    double contrast = 0.0;                               ///< (p99 - p01) / 255
    double clippedFraction = 0.0;                        ///< share of pixels at luma >= 250
    double darkFraction = 0.0;                           ///< share of pixels at luma <= 5
    /// Mean of the strongest 0.2 % of |Sobel gradient| values (luma units per
    /// pixel): edge steepness. Blur lowers it; sensor noise raises it a
    /// little. An index for comparing captures of similar scenes, not an
    /// absolute focus metric.
    double sharpnessIndex = 0.0;
};

struct Inspection {
    FrameQuality quality;
    AnalyzeOutcome strict;  ///< the measurement, when the scene passes
    std::uint8_t binarizationThreshold = 0;
    std::size_t blobCount = 0;  ///< blobs above the noise floor
    // Filled only when the strict analysis refused:
    std::optional<BlobStats> subject;        ///< largest blob that is not the near-reference
    std::optional<BlobStats> nearReference;  ///< best square-like blob, if any
    std::vector<std::string> failedChecks;   ///< reference gates it failed, with values
    double bestEffortMmPerPixel = 0.0;       ///< through nearReference; 0 without one
    double bestEffortConfidence = 0.0;       ///< 0..1, from which checks failed
};

/// Deterministic like analyzeFrame, and never refuses a frame it can decode.
[[nodiscard]] Inspection inspectFrame(const hal::Frame& frame, const CalibrationSpec& spec);

/// Appends the inspection to a record. Capture quality always goes in as
/// OBSERVED. A passing scene then gets exactly appendEvidence. A refused one
/// gets OBSERVED pixel facts, DERIVED best_effort_* sizes with confidence and
/// the failed checks in their method, and an UNRESOLVED "measurement" naming
/// why the strict analysis refused, plus quality-driven recommendations.
void appendInspection(observation::EngineeringObservation& record, const Inspection& inspection,
                      const CalibrationSpec& spec, std::string_view sourceArtifactId);

/// One line for the operator: what this capture could and could not give.
[[nodiscard]] std::string summarize(const Inspection& inspection);

void appendEvidence(observation::EngineeringObservation& record, const ScoutAnalysis& analysis,
                    const CalibrationSpec& spec, std::string_view sourceArtifactId);

}  // namespace platypus::vision
