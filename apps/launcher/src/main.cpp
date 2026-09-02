// PlatypusOS — composition root.
//
// This is the ONLY translation unit that knows concrete types. It builds the
// board, wires services, registers apps, and runs the shell loop. Everything
// below this file speaks only in interfaces.
#include "HostSimBoard.hpp"
#include "LauncherApp.hpp"

#include <platypus/appfw/AppContext.hpp>
#include <platypus/appfw/AppRegistry.hpp>
#include <platypus/appfw/EventQueue.hpp>
#include <platypus/apps/EngineeringScoutApp.hpp>
#include <platypus/apps/SettingsApp.hpp>
#include <platypus/filesystem/ProjectStore.hpp>
#include <platypus/renderer/Renderer.hpp>

#ifndef _WIN32
#include "linked/LinkedDisplay.hpp"
#include "linked/SerialTransport.hpp"
#include "unoq/UnoQBoard.hpp"
#endif

#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>  // std::errc, compared against from_chars_result::ec
#include <thread>

namespace {
std::atomic<bool> g_running{true};  // signal-handler flag; sole permitted "global"
void handleSignal(int) {
    g_running = false;
}

bool parseDimension(std::string_view text, std::uint16_t& out) {
    unsigned value = 0;
    const auto* const last = text.data() + text.size();
    const auto [end, ec] = std::from_chars(text.data(), last, value);
    if (ec != std::errc{} || end != last) return false;
    if (value == 0 || value > 4096) return false;  // sanity bounds, not a policy
    out = static_cast<std::uint16_t>(value);
    return true;
}

/// Parses "WIDTHxHEIGHT" (e.g. "800x480"), returning `fallback` unchanged if
/// the spec is malformed. ADR-0001 keeps the panel dynamic, so the simulated
/// geometry is a runtime choice rather than a compiled-in constant.
platypus::hal::DisplayInfo parseGeometry(std::string_view spec,
                                         platypus::hal::DisplayInfo fallback) {
    const auto separator = spec.find('x');
    if (separator == std::string_view::npos) return fallback;

    auto parsed = fallback;
    if (!parseDimension(spec.substr(0, separator), parsed.width) ||
        !parseDimension(spec.substr(separator + 1), parsed.height))
        return fallback;
    return parsed;
}

/// Frame sink for headless runs (no window backend, no linked display): the
/// renderer needs a real IDisplay, and boards legitimately report the display
/// as absent. Accepts frames, produces no events.
class HeadlessDisplay final : public platypus::hal::IDisplay {
   public:
    explicit HeadlessDisplay(platypus::hal::DisplayInfo info) : info_(info) {}
    platypus::hal::DisplayInfo info() const noexcept override { return info_; }
    platypus::hal::Status setBacklight(float) override { return {}; }
    platypus::hal::Status present(std::span<const std::byte>) override { return {}; }
    platypus::hal::Status onTouch(std::function<void(const platypus::hal::TouchEvent&)>) override {
        return {};
    }
    platypus::hal::Status onButton(
        std::function<void(const platypus::hal::ButtonEvent&)>) override {
        return {};
    }

   private:
    platypus::hal::DisplayInfo info_;
};
}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    // --- Hardware -----------------------------------------------------------
    // --board hostsim (default) | unoq (POSIX targets only).
    // --display-link PATH connects a LinkedDisplay over serial (ADR-0001
    //   prototype path; default /dev/ttyACM0) when the board is unoq.
    // --geometry WxH sizes the simulated/headless panel. Nothing downstream
    //   sees these values: apps and the renderer read IDisplay::info().
    auto geometry = platypus::sim::kDefaultSimGeometry;
    std::string boardChoice = "hostsim";
    std::string linkDevice = "/dev/ttyACM0";
    for (int i = 1; i + 1 < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--geometry") geometry = parseGeometry(argv[i + 1], geometry);
        if (arg == "--board") boardChoice = argv[i + 1];
        if (arg == "--display-link") linkDevice = argv[i + 1];
    }
    // Declared before the board so the queue outlives every registered driver
    // callback during shutdown.
    platypus::appfw::EventQueue eventQueue;

    std::unique_ptr<platypus::hal::IBoard> board;
#ifndef _WIN32
    if (boardChoice == "unoq") {
        platypus::unoq::UnoQBoard::Config config;
        auto transport = std::make_unique<platypus::linked::SerialTransport>(linkDevice);
        if (transport->open().ok()) {
            auto display = std::make_shared<platypus::linked::LinkedDisplay>(std::move(transport));
            if (display->connect().ok()) {
                std::printf("linked display: %ux%u via %s\n", display->info().width,
                            display->info().height, linkDevice.c_str());
                config.display = std::move(display);
            } else {
                std::fprintf(stderr, "linked display on %s: no session; running headless\n",
                             linkDevice.c_str());
            }
        } else {
            std::fprintf(stderr, "linked display: cannot open %s; running headless\n",
                         linkDevice.c_str());
        }
        board = std::make_unique<platypus::unoq::UnoQBoard>(std::move(config));
    }
#else
    if (boardChoice == "unoq")
        std::fprintf(stderr, "--board unoq is a POSIX target; using hostsim\n");
#endif
    if (!board) board = std::make_unique<platypus::sim::HostSimBoard>(geometry);

    // --- Services -----------------------------------------------------------
    // Boards may honestly have no display (headless bring-up); the renderer
    // still needs a sink so apps run unmodified.
    auto display = board->display();
    if (!display) display = std::make_shared<HeadlessDisplay>(geometry);
    platypus::renderer::Renderer renderer(display);
    platypus::filesystem::ProjectStore projects(board->storage());

    // --- App registry -------------------------------------------------------
    platypus::appfw::AppRegistry registry;
    // Built-in apps register here. shadowscan/viewer/measurement/inspection/
    // documentation follow the same pattern as settings (see apps/settings).
    registry.add(platypus::apps::SettingsApp().manifest(), &platypus::apps::SettingsApp::create);
    registry.add(platypus::apps::EngineeringScoutApp().manifest(),
                 &platypus::apps::EngineeringScoutApp::create);

    // The launcher needs the registry, so it is constructed directly rather
    // than through a factory.
    platypus::launcher::LauncherApp launcher(registry);

    // --- Shell loop ---------------------------------------------------------
    std::string pendingLaunch;
    platypus::appfw::AppContext ctx{
        *board, renderer, projects,
        [&pendingLaunch](const std::string& id) { pendingLaunch = id; }};

    platypus::appfw::IApp* active = &launcher;
    std::unique_ptr<platypus::appfw::IApp> activeOwned;
    active->onStart(ctx);

    // Driver callbacks may execute on transport threads. Copy events into the
    // bounded queue; the UI loop below is the only place that calls an app.
    display->onTouch(
        [&eventQueue](const platypus::hal::TouchEvent& event) { (void)eventQueue.post(event); });
    display->onButton(
        [&eventQueue](const platypus::hal::ButtonEvent& event) { (void)eventQueue.post(event); });

    auto last = std::chrono::steady_clock::now();
    while (g_running) {
        const auto now = std::chrono::steady_clock::now();
        const auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - last);
        last = now;

        (void)eventQueue.dispatchPending(*active);
        active->onFrame(ctx, dt);

        if (!pendingLaunch.empty()) {
            auto next = registry.create(pendingLaunch);
            pendingLaunch.clear();
            active->onStop();
            if (next) {
                activeOwned = std::move(next);
                active = activeOwned.get();
            } else {
                activeOwned.reset();
                active = &launcher;  // unknown id or "home": back to launcher
            }
            active->onStart(ctx);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(33));  // ~30 fps tick
    }

    active->onStop();
    std::puts("PlatypusOS shut down cleanly.");
    return 0;
}
