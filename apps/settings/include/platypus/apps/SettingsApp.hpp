// PlatypusOS app — Settings.
//
// Reference implementation of the IApp contract; the simplest possible app.
// Displays device info and (future) brightness/volume/storage management.
#pragma once

#include <platypus/appfw/IApp.hpp>

namespace platypus::apps {

class SettingsApp final : public appfw::IApp {
public:
    SettingsApp();

    /// Factory used to register with AppRegistry.
    static std::unique_ptr<appfw::IApp> create();

    [[nodiscard]] const appfw::AppManifest& manifest() const noexcept override;

    void onStart(appfw::AppContext& ctx) override;
    void onStop() override;
    void onFrame(appfw::AppContext& ctx, std::chrono::milliseconds dt) override;
    void onButton(const hal::ButtonEvent& event) override;

private:
    appfw::AppManifest manifest_;
    bool exitRequested_ = false;
};

}  // namespace platypus::apps
