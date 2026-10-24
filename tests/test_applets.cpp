// Scout and Depth as shell applets: real captures through the shell, and the
// ToF held only while the depth view is open.
#include "KioskApplets.hpp"

#include <platypus/appfw/AppRegistry.hpp>
#include <platypus/filesystem/ProjectStore.hpp>
#include <platypus/hal/tof/ITofSource.hpp>
#include <platypus/shell/Applets.hpp>
#include <platypus/shell/Shell.hpp>

#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>

namespace {

using namespace platypus;
using namespace std::chrono_literals;

class AppletDisplay final : public hal::IDisplay {
   public:
    hal::DisplayInfo info() const noexcept override { return {800, 480, 16}; }
    hal::Status setBacklight(float) override { return {}; }
    hal::Status present(std::span<const std::byte>) override { return {}; }
    hal::Status onTouch(std::function<void(const hal::TouchEvent&)>) override { return {}; }
    hal::Status onButton(std::function<void(const hal::ButtonEvent&)>) override { return {}; }
};

void test_scout_and_depth_through_the_shell() {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "platypus_test_applets";
    std::error_code ec;
    fs::remove_all(root, ec);

    auto display = std::make_shared<AppletDisplay>();
    renderer::Renderer r(display);
    shell::MinimalBoard board(display);
    filesystem::ProjectStore projects(nullptr);
    kiosk::SceneCamera camera;
    (void)camera.open(kiosk::SceneCamera::kMode);
    observation::CaptureService captures(root / "observations");
    session::SessionStore sessions(root / "sessions");
    hal::tof::FakeTofSource tof;

    appfw::AppRegistry reg;
    shell::registerStandardApplets(reg,
                                   {{shell::ids::kScout,
                                     [&]() -> std::unique_ptr<appfw::IApp> {
                                         kiosk::ScoutDeps d;
                                         d.camera = &camera;
                                         d.captures = &captures;
                                         d.sessions = &sessions;
                                         d.device = "synthetic";
                                         return std::make_unique<kiosk::ScoutApplet>(std::move(d));
                                     }},
                                    {shell::ids::kDepth, [&]() -> std::unique_ptr<appfw::IApp> {
                                         kiosk::DepthDeps d;
                                         d.camera = &camera;
                                         d.tof = &tof;
                                         return std::make_unique<kiosk::DepthApplet>(std::move(d));
                                     }}});
    for (const auto& m : reg.manifests())
        if (m.id == shell::ids::kScout || m.id == shell::ids::kDepth) assert(!m.preview);

    shell::Shell sh(reg, appfw::AppContext{board, r, projects, {}});
    sh.skipSplash();

    // Scout: button A captures; the record lands on disk with its evidence.
    sh.open(shell::ids::kScout);
    sh.frame(16ms);
    sh.onButton({1, true});
    sh.frame(16ms);
    assert(fs::exists(root / "observations" / "scan-0001" / "observation.json"));
    assert(fs::exists(root / "sessions" / "sess-0001"));

    // Depth: the ToF is held only while the view is open.
    assert(!tof.running());
    sh.open(shell::ids::kDepth);
    assert(tof.running());
    sh.frame(16ms);
    sh.onTouch({hal::TouchEvent::Type::Down, 10, 10});  // the bar's HOME
    sh.frame(16ms);
    assert(sh.state() == shell::Shell::State::Home && !tof.running());

    fs::remove_all(root, ec);
    std::puts("test_applets_through_shell: OK");
}

}  // namespace

void test_applets() {
    test_scout_and_depth_through_the_shell();
}
