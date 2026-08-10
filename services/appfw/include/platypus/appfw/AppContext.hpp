// PlatypusOS app framework — dependency injection context.
//
// AppContext is the only way an app reaches hardware and services. It is
// constructed once by the composition root (apps/launcher/main.cpp) and
// passed by reference; apps must not copy service pointers out beyond their
// own lifetime. No singletons, no globals.
#pragma once

#include <platypus/filesystem/ProjectStore.hpp>
#include <platypus/hal/IBoard.hpp>
#include <platypus/renderer/Renderer.hpp>

#include <functional>
#include <memory>
#include <string>

namespace platypus::appfw {

struct AppContext {
    hal::IBoard& board;                       ///< capability discovery
    renderer::Renderer& renderer;             ///< all drawing goes here
    filesystem::ProjectStore& projects;       ///< persistent user work

    /// Request that the launcher switch to another app after this callback
    /// returns. Empty id = return to launcher home.
    std::function<void(const std::string& appId)> requestLaunch;
};

}  // namespace platypus::appfw
