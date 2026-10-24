// PlatypusOS shell — the standard applet set and the applets the shell owns.
//
// The home carousel shows these cards in this order. Applets that need the
// device (camera, ToF) or are not built yet register a PreviewApplet on hosts
// where they cannot run, so the card and the screen say so instead of
// pretending (issue #51 tracks each one).
#pragma once

#include <platypus/appfw/AppRegistry.hpp>
#include <platypus/appfw/IApp.hpp>

#include <string>
#include <vector>

namespace platypus::shell {

namespace ids {
inline constexpr const char* kScout = "one.platypus.scout";
inline constexpr const char* kDepth = "one.platypus.depth";
inline constexpr const char* kShadowScan = "one.platypus.shadowscan";
inline constexpr const char* kRoomScan = "one.platypus.roomscan";
inline constexpr const char* kViewer3d = "one.platypus.viewer3d";
inline constexpr const char* kSystem = "one.platypus.system";
}  // namespace ids

/// The six standard cards, in carousel order. `preview` is true for every
/// applet not built yet; a device host clears it for the ones it provides.
[[nodiscard]] std::vector<appfw::AppManifest> standardManifests();

/// What an unavailable applet will do, and where the work is tracked.
[[nodiscard]] std::vector<std::string> previewNotes(const std::string& appId);

/// Says what the applet will be, what it needs, and that it is not built.
class PreviewApplet final : public appfw::IApp {
   public:
    PreviewApplet(appfw::AppManifest manifest, std::vector<std::string> notes);
    [[nodiscard]] const appfw::AppManifest& manifest() const noexcept override { return manifest_; }
    void onStart(appfw::AppContext&) override {}
    void onStop() override {}
    void onFrame(appfw::AppContext& ctx, std::chrono::milliseconds dt) override;

   private:
    appfw::AppManifest manifest_;
    std::vector<std::string> notes_;
};

inline constexpr const char* kShellVersion = "0.1.0";

/// Device status from the board in the context: shell version, display,
/// board, camera, and the hardware the shell does not read yet (#50).
class SystemApplet final : public appfw::IApp {
   public:
    explicit SystemApplet(appfw::AppManifest manifest);
    [[nodiscard]] const appfw::AppManifest& manifest() const noexcept override { return manifest_; }
    void onStart(appfw::AppContext&) override {}
    void onStop() override {}
    void onFrame(appfw::AppContext& ctx, std::chrono::milliseconds dt) override;

   private:
    appfw::AppManifest manifest_;
};

/// Registers every standard card: the given factories where a host provides
/// the real applet, a PreviewApplet otherwise, and the SystemApplet.
struct ProvidedApplet {
    std::string id;
    appfw::AppFactory factory;
};
void registerStandardApplets(appfw::AppRegistry& registry,
                             const std::vector<ProvidedApplet>& provided = {});

}  // namespace platypus::shell
