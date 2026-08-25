#include "LauncherApp.hpp"

#include <algorithm>

namespace platypus::launcher {

namespace {
constexpr renderer::Color kBackground{16, 24, 32};
constexpr renderer::Color kRowFill{32, 48, 64};
constexpr renderer::Color kText{220, 230, 240};
}  // namespace

LauncherApp::LauncherApp(const appfw::AppRegistry& registry)
    : registry_(registry),
      manifest_{"one.platypus.launcher", "Launcher", "0.1.0", false, false} {}

const appfw::AppManifest& LauncherApp::manifest() const noexcept { return manifest_; }

void LauncherApp::onStart(appfw::AppContext&) { selectedIndex_ = -1; }
void LauncherApp::onStop() {}

void LauncherApp::onTouch(const hal::TouchEvent& event) {
    if (event.type == hal::TouchEvent::Type::Up)
        selectedIndex_ = event.y / rowHeight_;
}

void LauncherApp::onFrame(appfw::AppContext& ctx, std::chrono::milliseconds) {
    const auto& apps = registry_.manifests();

    // Rows scale with the panel instead of assuming one (ADR-0001: geometry is
    // discovered, never assumed). A tenth of the panel height, floored at a
    // 32 px touch target — which leaves the 320x240 layout exactly as it was.
    rowHeight_ = std::max(32, static_cast<int>(ctx.renderer.displayInfo().height) / 10);

    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(apps.size())) {
        const auto id = apps[static_cast<std::size_t>(selectedIndex_)].id;
        selectedIndex_ = -1;
        if (id != manifest_.id) {
            ctx.requestLaunch(id);
            return;
        }
    }

    auto& r = ctx.renderer;
    r.clear(kBackground);
    int y = 0;
    for (const auto& app : apps) {
        r.fillRect({2, y + 2, r.displayInfo().width - 4, rowHeight_ - 4}, kRowFill);
        r.drawText(10, y + rowHeight_ / 2 - 4, app.name, kText);
        y += rowHeight_;
    }
    r.present();
}

}  // namespace platypus::launcher
