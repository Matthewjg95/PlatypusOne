// PlatypusOS app framework — the application contract.
//
// PlatypusOS is not one program; every feature is an app implementing IApp.
// Apps receive all capabilities through AppContext (dependency injection) and
// own no hardware directly. This is also the future plugin ABI: out-of-tree
// apps will export a factory returning std::unique_ptr<IApp>.
#pragma once

#include "AppContext.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace platypus::appfw {

struct AppManifest {
    std::string id;       ///< reverse-dns unique id, e.g. "one.platypus.shadowscan"
    std::string name;     ///< display name
    std::string version;  ///< semver
    bool requiresCamera = false;
    bool requiresSensors = false;
    std::string tagline;   ///< one line under the name on the home carousel
    std::string icon;      ///< icon key the shell knows how to draw
    bool preview = false;  ///< not built yet: the card and the app say so
};

/// Lifecycle:  onStart -> (onFrame | onTouch | onButton)* -> onStop
/// App callbacks execute on the UI thread. Driver threads hand input through
/// EventQueue; they never call an app directly. Apps must return from every
/// callback quickly; long work belongs on a worker thread the app owns and
/// joins in onStop.
class IApp {
   public:
    virtual ~IApp() = default;

    [[nodiscard]] virtual const AppManifest& manifest() const noexcept = 0;

    virtual void onStart(AppContext& ctx) = 0;
    virtual void onStop() = 0;

    /// Called once per UI tick with elapsed time; draw via ctx.renderer.
    virtual void onFrame(AppContext& ctx, std::chrono::milliseconds dt) = 0;

    virtual void onTouch(const hal::TouchEvent& event) { (void)event; }
    virtual void onButton(const hal::ButtonEvent& event) { (void)event; }
};

/// Factory for an app. A std::function so a composition root can register
/// apps that capture the services it owns (camera, capture store, sensors);
/// a future dlopen'd plugin's exported factory is wrapped into one.
using AppFactory = std::function<std::unique_ptr<IApp>()>;

}  // namespace platypus::appfw
