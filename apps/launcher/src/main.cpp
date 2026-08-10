// PlatypusOS — composition root.
//
// This is the ONLY translation unit that knows concrete types. It builds the
// board, wires services, registers apps, and runs the shell loop. Everything
// below this file speaks only in interfaces.
#include "HostSimBoard.hpp"
#include "LauncherApp.hpp"

#include <platypus/appfw/AppContext.hpp>
#include <platypus/apps/SettingsApp.hpp>
#include <platypus/appfw/AppRegistry.hpp>
#include <platypus/filesystem/ProjectStore.hpp>
#include <platypus/renderer/Renderer.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <thread>

namespace {
std::atomic<bool> g_running{true};  // signal-handler flag; sole permitted "global"
void handleSignal(int) { g_running = false; }
}  // namespace

int main() {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    // --- Hardware -----------------------------------------------------------
    // TODO(board-bringup): select UnoQBoard vs HostSimBoard from build config.
    platypus::sim::HostSimBoard board;

    // --- Services -----------------------------------------------------------
    platypus::renderer::Renderer renderer(board.display());
    platypus::filesystem::ProjectStore projects(board.storage());

    // --- App registry -------------------------------------------------------
    platypus::appfw::AppRegistry registry;
    // Built-in apps register here. shadowscan/viewer/measurement/inspection/
    // documentation follow the same pattern as settings (see apps/settings).
    registry.add(platypus::apps::SettingsApp().manifest(),
                 &platypus::apps::SettingsApp::create);

    // The launcher needs the registry, so it is constructed directly rather
    // than through a factory.
    platypus::launcher::LauncherApp launcher(registry);

    // --- Shell loop ---------------------------------------------------------
    std::string pendingLaunch;
    platypus::appfw::AppContext ctx{
        board, renderer, projects,
        [&pendingLaunch](const std::string& id) { pendingLaunch = id; }};

    platypus::appfw::IApp* active = &launcher;
    std::unique_ptr<platypus::appfw::IApp> activeOwned;
    active->onStart(ctx);

    // Route input to whichever app is active. `active` is captured by
    // reference so app switches retarget input automatically.
    if (auto display = board.display()) {
        display->onTouch([&active](const platypus::hal::TouchEvent& e) { active->onTouch(e); });
        display->onButton([&active](const platypus::hal::ButtonEvent& e) { active->onButton(e); });
    }

    auto last = std::chrono::steady_clock::now();
    while (g_running) {
        const auto now = std::chrono::steady_clock::now();
        const auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - last);
        last = now;

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
