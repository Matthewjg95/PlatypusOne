// scout_kiosk core — the portable half of the kiosk.
//
// Everything the kiosk shows and does that does not touch Linux hardware:
// layout, the preview / session / summary screens, one capture through the
// analyzer + classifier, and the synthetic camera. The kiosk binary adds only
// V4L2 discovery and the DRM display; tools/ui_preview renders these same
// screens on any host, so the session flow can be reviewed without a board.
#pragma once

#include <platypus/apps/EngineeringScoutApp.hpp>
#include <platypus/hal/ICamera.hpp>
#include <platypus/hal/testing/SyntheticScene.hpp>
#include <platypus/observation/CaptureService.hpp>
#include <platypus/renderer/Renderer.hpp>
#include <platypus/session/SessionStore.hpp>
#include <platypus/vision/ScoutAnalyzer.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace platypus::kiosk {

using renderer::Color;
using renderer::Rect;

inline constexpr Color kBackground{12, 14, 18};
inline constexpr Color kPanel{28, 32, 40};
inline constexpr Color kText{230, 232, 236};
inline constexpr Color kMuted{140, 146, 158};
inline constexpr Color kAccent{255, 196, 0};
inline constexpr Color kGood{80, 200, 120};
inline constexpr Color kWarn{255, 110, 90};

/// Packed YUYV (BT.601, limited range) to tightly packed RGB888.
[[nodiscard]] std::vector<std::uint8_t> yuyvToRgb(const hal::Frame& frame);

/// Laid out against the display's real geometry (ADR-0001): the 800x480 panel
/// and a 1080p bench monitor both come out right.
struct Layout {
    Rect preview;
    Rect panel;
    Rect capture;  ///< primary button
    Rect finish;   ///< closes the session
    std::int32_t textScale = 2;
};
[[nodiscard]] Layout computeLayout(const hal::DisplayInfo& info, std::int32_t camW,
                                   std::int32_t camH);
[[nodiscard]] bool inside(const Rect& r, std::int32_t x, std::int32_t y);

/// Word-wraps `text` to `maxWidth` pixels from (x, y); returns the y below it.
std::int32_t drawWrapped(renderer::Renderer& r, std::int32_t x, std::int32_t y,
                         std::int32_t maxWidth, const std::string& text, Color colour,
                         std::int32_t scale);

/// What the side panel shows while previewing.
struct PanelState {
    std::string sessionId;
    std::optional<session::Guidance> guidance;  ///< nullopt before a session exists
    std::string status;                         ///< last capture outcome, one line
    Color statusColour = kMuted;
    bool busy = false;
};

void drawPreviewScreen(renderer::Renderer& r, const Layout& l, const std::vector<std::uint8_t>& rgb,
                       std::int32_t camW, std::int32_t camH, const PanelState& panel);

/// The finish screen: what the session concluded and what it exported.
void drawSessionSummary(renderer::Renderer& r, const session::ObjectSession& s,
                        const session::FinishResult& result, session::CloseReason reason);

/// Operator-facing reason for a refused scene: the analyzer refuses rather
/// than guess, and the operator is told what to change.
[[nodiscard]] std::string guidanceFor(vision::AnalyzeError error);

struct CaptureOutcome {
    std::optional<observation::EngineeringObservation> record;  ///< always set when saved
    bool measured = false;
    std::optional<apps::CardImage> thumbnail;
    std::optional<vision::ScoutAnalysis> analysis;
    std::string refusal;  ///< guidance when the scene was refused
    std::string status;   ///< one line for the panel
    Color statusColour = kText;
};

/// One capture: frame -> analyzer + classifier -> all-or-nothing record.
[[nodiscard]] CaptureOutcome captureOnce(hal::ICamera& camera, observation::CaptureService& service,
                                         const vision::CalibrationSpec& spec,
                                         const std::string& device, const std::string& identity);

/// What the session keeps of a capture: claims by name, plus the outline and
/// image the finish-time export needs.
[[nodiscard]] session::Capture sessionCapture(const CaptureOutcome& outcome,
                                              const session::ObjectProfile& profile);

/// Development camera: the validation battery's M8 case (20 mm square, 8 x 40
/// mm shaft, 0.25 mm/px) served as YUYV, the webcam's own format. Pose can be
/// changed between captures to mimic "shift or rotate the part"; pose -1
/// removes the part, for the refused-scene path. Records name it "synthetic".
class SceneCamera final : public hal::ICamera {
   public:
    SceneCamera();
    void setPose(int pose);

    [[nodiscard]] std::vector<hal::CameraMode> supportedModes() const override { return {kMode}; }
    hal::Status open(const hal::CameraMode&) override;
    hal::Status close() override;
    [[nodiscard]] bool isOpen() const noexcept override { return open_; }
    hal::Status setControls(const hal::CameraControls&) override { return {}; }
    hal::Result<hal::Frame> capture(std::chrono::milliseconds) override;
    hal::Status startStream(std::function<void(const hal::Frame&)> onFrame) override;
    hal::Status stopStream() override { return {}; }

    static constexpr hal::CameraMode kMode{640, 480, hal::PixelFormat::YUYV, 30.0f};

   private:
    hal::testing::SyntheticScene scene_;
    bool open_ = false;
};

}  // namespace platypus::kiosk
