// The kiosk's two screens as shell applets (issue #51, stage 2): Engineering
// Scout and the depth view, drawn in the shell's content area.
//
// Portable: the device host gives them the CSI camera, the capture and
// session stores and the ToF Monitor client; tools/ui_preview and the tests
// give them SceneCamera and FakeTofSource. The host owns and opens the camera;
// applets only pull frames from it.
#pragma once

#include "KioskCore.hpp"

#include <platypus/appfw/IApp.hpp>
#include <platypus/hal/tof/ITofSource.hpp>

#include <functional>
#include <memory>
#include <string>

namespace platypus::kiosk {

struct ScoutDeps {
    hal::ICamera* camera = nullptr;  ///< opened by the host
    std::int32_t camW = 640, camH = 480;
    observation::CaptureService* captures = nullptr;
    session::SessionStore* sessions = nullptr;
    vision::CalibrationSpec spec;
    std::string device = "camera";
    std::function<std::string()> identity;  ///< e.g. the CSI exposure line, per capture
    observation::AttachFn attach;           ///< e.g. the raw sensor frame
    std::uint8_t buttonCapture = 1;         ///< board button A
    std::uint8_t buttonFinish = 2;          ///< board button B
};

/// Live preview -> CAPTURE -> evidence card -> ... -> FINISH -> summary.
class ScoutApplet final : public appfw::IApp {
   public:
    explicit ScoutApplet(ScoutDeps deps);
    [[nodiscard]] const appfw::AppManifest& manifest() const noexcept override { return manifest_; }
    void onStart(appfw::AppContext& ctx) override;
    void onStop() override {}
    void onFrame(appfw::AppContext& ctx, std::chrono::milliseconds dt) override;
    void onTouch(const hal::TouchEvent& event) override;
    void onButton(const hal::ButtonEvent& event) override;

    enum class Screen { Preview, Card, Summary };
    [[nodiscard]] Screen screen() const noexcept { return screen_; }
    [[nodiscard]] const session::ObjectSession* session() const noexcept { return current_.get(); }
    [[nodiscard]] const std::string& status() const noexcept { return status_; }

   private:
    void newSession();
    void capture(renderer::Renderer& r);
    void finish(renderer::Renderer& r);

    appfw::AppManifest manifest_;
    ScoutDeps deps_;
    Layout layout_;
    Screen screen_ = Screen::Preview;
    std::unique_ptr<session::ObjectSession> current_;
    std::vector<std::uint8_t> lastRgb_;
    std::string status_;
    Color statusColour_ = kMuted;
    bool captureRequested_ = false, finishRequested_ = false, dismissRequested_ = false;
};

struct DepthDeps {
    hal::ICamera* camera = nullptr;  ///< opened by the host
    std::int32_t camW = 640, camH = 480;
    hal::tof::ITofSource* tof = nullptr;
    FieldsOfView fov;
    hal::tof::Orientation orientation = hal::tof::Orientation::Raw;
};

/// Camera, ToF grid and overlay. Holds the ToF stream only while open.
class DepthApplet final : public appfw::IApp {
   public:
    explicit DepthApplet(DepthDeps deps);
    [[nodiscard]] const appfw::AppManifest& manifest() const noexcept override { return manifest_; }
    void onStart(appfw::AppContext& ctx) override;
    void onStop() override;
    void onFrame(appfw::AppContext& ctx, std::chrono::milliseconds dt) override;
    void onTouch(const hal::TouchEvent& event) override;

    [[nodiscard]] hal::tof::Orientation orientation() const noexcept { return deps_.orientation; }

   private:
    appfw::AppManifest manifest_;
    DepthDeps deps_;
    DepthLayout layout_;
    std::vector<std::uint8_t> lastRgb_;
    bool flipRequested_ = false;
};

}  // namespace platypus::kiosk
