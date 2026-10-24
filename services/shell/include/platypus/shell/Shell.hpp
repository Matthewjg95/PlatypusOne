// PlatypusOS shell — splash, home carousel, and the applet lifecycle.
//
// Modelled on the Project Platypus Tab5 shell (platypus-lab src/shell.h): one
// applet runs at a time; home is a swipeable strip of applet cards; the shell
// owns a thin bar with HOME so every applet has the same way out. Applets are
// appfw::IApp; the shell gives each an AppContext whose contentArea excludes
// the bar, and presents once per frame after the applet has drawn.
//
// Input arrives on the UI thread (post events through appfw::EventQueue from
// driver threads). On home, board button A steps to the next card and B opens
// it; inside an applet the buttons belong to the applet and the bar's HOME
// is the way out.
#pragma once

#include <platypus/appfw/AppContext.hpp>
#include <platypus/appfw/AppRegistry.hpp>
#include <platypus/appfw/IApp.hpp>
#include <platypus/hal/IBoard.hpp>
#include <platypus/shell/Carousel.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace platypus::shell {

struct ShellOptions {
    std::string version = "0.1.0";
    std::chrono::milliseconds splash{1600};
    std::uint8_t buttonNext = 1;  ///< board button A (DrmDisplay kBoardButtonA)
    std::uint8_t buttonOpen = 2;  ///< board button B
};

/// Geometry of the home screen and the shell bar, from the display's size.
struct ShellLayout {
    renderer::Rect bar;      ///< shell bar while an applet runs
    renderer::Rect home;     ///< HOME button inside the bar
    renderer::Rect content;  ///< what the applet may draw
    renderer::Rect card;     ///< the centred card on the home carousel
    std::int32_t pitch = 0;  ///< card centre to card centre
    std::int32_t dotsY = 0;
    std::int32_t textScale = 2;
};
[[nodiscard]] ShellLayout computeShellLayout(const hal::DisplayInfo& info);

class Shell {
   public:
    enum class State { Splash, Home, App };

    /// `context` carries board, renderer and project store from the
    /// composition root; the shell fills in requestLaunch and contentArea.
    Shell(const appfw::AppRegistry& registry, appfw::AppContext context, ShellOptions options = {});
    ~Shell();

    void onTouch(const hal::TouchEvent& event);
    void onButton(const hal::ButtonEvent& event);
    /// Advances time, runs the active applet, draws, presents.
    void frame(std::chrono::milliseconds dt);

    void open(std::string_view appId);
    void home();
    /// Skips the rest of the splash (the device is ready sooner).
    void skipSplash();

    [[nodiscard]] State state() const noexcept { return state_; }
    [[nodiscard]] const Carousel& carousel() const noexcept { return carousel_; }
    [[nodiscard]] std::string activeAppId() const;
    [[nodiscard]] const std::vector<appfw::AppManifest>& cards() const noexcept { return cards_; }
    [[nodiscard]] const ShellLayout& layout() const noexcept { return layout_; }

   private:
    void drawSplash();
    void drawHome();
    void drawBar();
    void stopActive();

    const appfw::AppRegistry& registry_;
    appfw::AppContext context_;
    ShellOptions options_;
    ShellLayout layout_;
    std::vector<appfw::AppManifest> cards_;
    Carousel carousel_;
    State state_ = State::Splash;
    std::chrono::milliseconds splashElapsed_{0};
    std::unique_ptr<appfw::IApp> active_;
    std::string pendingLaunch_;
    bool pendingHome_ = false;
};

/// Draws the icon for a manifest's icon key, fitted to `box`.
void drawAppIcon(renderer::Renderer& r, const renderer::Rect& box, std::string_view key);

/// Board for hosts without hardware (ui_preview, tests): the display it is
/// given, nothing else.
class MinimalBoard final : public hal::IBoard {
   public:
    explicit MinimalBoard(std::shared_ptr<hal::IDisplay> display) : display_(std::move(display)) {}
    [[nodiscard]] std::string_view id() const noexcept override { return "minimal-host"; }
    [[nodiscard]] std::shared_ptr<hal::IDisplay> display() override { return display_; }
    [[nodiscard]] std::shared_ptr<hal::ICamera> camera() override { return nullptr; }
    [[nodiscard]] std::shared_ptr<hal::ISensorHub> sensors() override { return nullptr; }
    [[nodiscard]] std::shared_ptr<hal::IAudioOutput> audio() override { return nullptr; }
    [[nodiscard]] std::shared_ptr<hal::IStorage> storage() override { return nullptr; }
    [[nodiscard]] std::shared_ptr<hal::IMcuBridge> mcuBridge() override { return nullptr; }

   private:
    std::shared_ptr<hal::IDisplay> display_;
};

}  // namespace platypus::shell
