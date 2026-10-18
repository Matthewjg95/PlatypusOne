// Device shell: carousel motion, splash -> home -> applet -> home, and the
// standard applet set.
#include <platypus/appfw/AppRegistry.hpp>
#include <platypus/filesystem/ProjectStore.hpp>
#include <platypus/shell/Applets.hpp>
#include <platypus/shell/Carousel.hpp>
#include <platypus/shell/Shell.hpp>

#include <cassert>
#include <chrono>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

using namespace platypus;
using namespace std::chrono_literals;
using shell::Carousel;

class ShellDisplay final : public hal::IDisplay {
   public:
    hal::DisplayInfo info() const noexcept override { return {800, 480, 16}; }
    hal::Status setBacklight(float) override { return {}; }
    hal::Status present(std::span<const std::byte> pixels) override {
        frame_.assign(pixels.begin(), pixels.end());
        ++presents_;
        return {};
    }
    hal::Status onTouch(std::function<void(const hal::TouchEvent&)>) override { return {}; }
    hal::Status onButton(std::function<void(const hal::ButtonEvent&)>) override { return {}; }

    std::vector<std::byte> frame_;
    int presents_ = 0;
};

void settle(Carousel& c) {
    for (int i = 0; i < 100; ++i)
        c.tick(16ms);
}

void test_carousel_snaps_and_taps() {
    Carousel c(4);
    c.setPitch(400);

    // A short, slow drag (15 % of a card) springs back.
    c.press(500, 200);
    c.tick(400ms);
    c.drag(440, 200);
    assert(!c.release(440, 200).tap);
    settle(c);
    assert(c.page() == 0 && c.position() == 0.0);

    // Past a fifth of a card goes to the next one.
    c.press(500, 200);
    c.tick(400ms);  // slow drag: no flick
    c.drag(400, 200);
    (void)c.release(400, 200);
    settle(c);
    assert(c.page() == 1 && c.position() == 1.0);

    // A quick flick moves a page even when short.
    c.press(500, 200);
    c.tick(100ms);
    c.drag(450, 200);
    (void)c.release(450, 200);
    assert(c.page() == 2);
    settle(c);

    // Barely moving is a tap, and does not move the strip.
    c.press(400, 200);
    c.drag(405, 203);
    const auto rel = c.release(405, 203);
    assert(rel.tap && c.page() == 2);

    // Dragging past the last card gives way, then settles back.
    c.jumpTo(3);
    c.press(500, 200);
    c.drag(-500, 200);  // 2.5 cards beyond the end
    assert(c.position() > 3.0 && c.position() <= 3.25);
    (void)c.release(-500, 200);
    settle(c);
    assert(c.page() == 3 && c.position() == 3.0);

    // Buttons step and clamp.
    c.step(1);
    assert(c.page() == 3);
    c.step(-2);
    assert(c.page() == 1);
    std::puts("test_shell_carousel: OK");
}

void test_standard_applets() {
    appfw::AppRegistry reg;
    shell::registerStandardApplets(reg);
    const auto& m = reg.manifests();
    assert(m.size() == 6);
    assert(m.front().id == shell::ids::kScout && m.back().id == shell::ids::kSystem);
    for (const auto& a : m)
        assert(a.preview == (a.id != shell::ids::kSystem));  // only System is built
    for (const auto& a : m)
        assert(reg.create(a.id) != nullptr);

    // A host that provides an applet clears its preview flag.
    appfw::AppRegistry dev;
    shell::registerStandardApplets(
        dev, {{shell::ids::kDepth, +[]() -> std::unique_ptr<appfw::IApp> {
                   return std::make_unique<shell::PreviewApplet>(appfw::AppManifest{}, std::vector<std::string>{});
               }}});
    for (const auto& a : dev.manifests())
        if (a.id == shell::ids::kDepth) assert(!a.preview);
    std::puts("test_shell_applets: OK");
}

void test_shell_flow() {
    auto display = std::make_shared<ShellDisplay>();
    renderer::Renderer r(display);
    shell::MinimalBoard board(display);
    filesystem::ProjectStore projects(nullptr);
    appfw::AppRegistry reg;
    shell::registerStandardApplets(reg);
    shell::ShellOptions opts;
    opts.splash = 1000ms;
    shell::Shell sh(reg, appfw::AppContext{board, r, projects, {}}, opts);

    assert(sh.state() == shell::Shell::State::Splash);
    sh.frame(500ms);
    assert(sh.state() == shell::Shell::State::Splash && display->presents_ == 1);
    sh.frame(600ms);
    assert(sh.state() == shell::Shell::State::Home);
    assert(sh.cards().size() == 6);

    // Button A steps through the cards, wrapping from the last to the first.
    for (int i = 0; i < 5; ++i)
        sh.onButton({1, true});
    assert(sh.carousel().page() == 5);
    sh.onButton({1, true});
    assert(sh.carousel().page() == 0);

    // Tapping the centred card opens it once the strip has settled.
    for (int i = 0; i < 60; ++i)
        sh.frame(16ms);
    const auto& card = sh.layout().card;
    const auto cx = static_cast<std::uint16_t>(card.x + card.w / 2);
    const auto cy = static_cast<std::uint16_t>(card.y + card.h / 2);
    sh.onTouch({hal::TouchEvent::Type::Down, cx, cy});
    sh.onTouch({hal::TouchEvent::Type::Up, cx, cy});
    sh.frame(16ms);
    assert(sh.state() == shell::Shell::State::App && sh.activeAppId() == shell::ids::kScout);

    // The bar's HOME returns to the carousel, on the same card.
    sh.onTouch({hal::TouchEvent::Type::Down, 10, 10});
    sh.frame(16ms);
    assert(sh.state() == shell::Shell::State::Home && sh.activeAppId().empty());
    assert(sh.carousel().page() == 0);

    // Button B opens the current card.
    sh.onButton({1, true});
    sh.onButton({2, true});
    sh.frame(16ms);
    assert(sh.activeAppId() == shell::ids::kDepth);
    std::puts("test_shell_flow: OK");
}

}  // namespace

void test_shell() {
    test_carousel_snaps_and_taps();
    test_standard_applets();
    test_shell_flow();
}
