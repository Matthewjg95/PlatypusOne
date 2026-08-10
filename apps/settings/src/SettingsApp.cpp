#include "platypus/apps/SettingsApp.hpp"

#include <string>

namespace platypus::apps {

SettingsApp::SettingsApp()
    : manifest_{"one.platypus.settings", "Settings", "0.1.0", false, false} {}

std::unique_ptr<appfw::IApp> SettingsApp::create() {
    return std::make_unique<SettingsApp>();
}

const appfw::AppManifest& SettingsApp::manifest() const noexcept { return manifest_; }

void SettingsApp::onStart(appfw::AppContext&) { exitRequested_ = false; }
void SettingsApp::onStop() {}

void SettingsApp::onButton(const hal::ButtonEvent& event) {
    // Any button press exits back to the launcher for now.
    if (event.pressed) exitRequested_ = true;
}

void SettingsApp::onFrame(appfw::AppContext& ctx, std::chrono::milliseconds) {
    if (exitRequested_) {
        ctx.requestLaunch("");  // empty id = home
        return;
    }

    auto& r = ctx.renderer;
    r.clear({16, 24, 32});
    r.drawText(10, 10, "Settings", {220, 230, 240});
    r.drawText(10, 30, std::string("Board: ") + std::string(ctx.board.id()), {160, 170, 180});

    if (auto storage = ctx.board.storage()) {
        if (auto free = storage->bytesFree()) {
            const auto mb = free.value() / (1024 * 1024);
            r.drawText(10, 46, "Free: " + std::to_string(mb) + " MB", {160, 170, 180});
        }
    }
    r.present();
}

}  // namespace platypus::apps
