// PlatypusOS services — evidence capture: camera frame → observation record.
//
// The first physical-evidence path through PlatypusOS: acquires one frame
// through any ICamera, persists it as a source artifact, and writes a valid
// EngineeringObservation JSON record beside it:
//
//   <root>/<observation_id>/source.<ext>      (frame bytes)
//   <root>/<observation_id>/observation.json  (contract record)
//
// Hardware-agnostic (fake and V4L2 cameras behave identically) and
// all-or-nothing: a failed capture or write leaves NO observation directory —
// evidence is never half-recorded. No database (contract constraint 2); the
// directory + JSON + image IS the record.
//
// Scope: capture only. No detection, calibration, measurement, or inference —
// those append claims to records in later Scout chunks.
#pragma once

#include "Observation.hpp"

#include <platypus/hal/ICamera.hpp>

#include <chrono>
#include <filesystem>

namespace platypus::observation {

struct CaptureConfig {
    std::string observationId;      ///< directory name; use nextObservationId()
    std::string timestampUtc;       ///< caller's clock (see currentUtcTimestamp())
    /// App/device/camera identity for the record's source block, e.g.
    /// {"app","engineering_scout"}, {"camera","/dev/video0"}. The service
    /// appends frame_width / frame_height / pixel_format from the real frame.
    std::vector<std::pair<std::string, std::string>> source;
    std::chrono::milliseconds captureTimeout{2000};
};

struct CaptureResult {
    std::filesystem::path directory;
    std::filesystem::path imagePath;
    std::filesystem::path recordPath;
    EngineeringObservation record;
};

class CaptureService {
public:
    explicit CaptureService(std::filesystem::path observationsRoot);

    /// Camera must already be open. On any failure the observations root is
    /// left untouched (no partial evidence). Error mapping:
    ///   camera errors pass through; existing observationId -> Busy;
    ///   invalid config -> InvalidArgument; write failures -> IoFailure.
    hal::Result<CaptureResult> capture(hal::ICamera& camera, const CaptureConfig& config);

    /// First free "scan-NNNN" id under the root, starting at scan-0001.
    [[nodiscard]] std::string nextObservationId() const;

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }

    /// ISO 8601 UTC "YYYY-MM-DDTHH:MM:SSZ" from the system clock. Kept as a
    /// helper (not called internally) so tests can inject fixed timestamps.
    [[nodiscard]] static std::string currentUtcTimestamp();

private:
    std::filesystem::path root_;
};

/// File extension + artifact MIME kind for a pixel format. MJPEG frames are
/// standalone JPEG files; uncompressed formats use dependency-free container
/// formats where one exists (PPM/PGM) and raw dumps otherwise.
[[nodiscard]] std::string_view imageExtensionFor(hal::PixelFormat format) noexcept;
[[nodiscard]] std::string_view imageKindFor(hal::PixelFormat format) noexcept;

}  // namespace platypus::observation
