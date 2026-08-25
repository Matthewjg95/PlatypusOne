// PlatypusOS launcher — the home screen.
//
// The launcher is itself an IApp: it lists installed apps from the registry
// and asks the shell to switch to whichever the user selects. It gets no
// special privileges beyond a read-only view of the registry.
#pragma once

#include <platypus/appfw/AppRegistry.hpp>
#include <platypus/appfw/IApp.hpp>

namespace platypus::launcher {

class LauncherApp final : public appfw::IApp {
public:
    explicit LauncherApp(const appfw::AppRegistry& registry);

    [[nodiscard]] const appfw::AppManifest& manifest() const noexcept override;

    void onStart(appfw::AppContext& ctx) override;
    void onStop() override;
    void onFrame(appfw::AppContext& ctx, std::chrono::milliseconds dt) override;
    void onTouch(const hal::TouchEvent& event) override;

private:
    const appfw::AppRegistry& registry_;
    appfw::AppManifest manifest_;
    int selectedIndex_ = -1;   ///< set by onTouch, consumed by onFrame
    int rowHeight_ = 32;       ///< recomputed from panel height each onFrame
};

}  // namespace platypus::launcher
