// Renderer tests against a fake display — demonstrates that the HAL
// interfaces make every service testable without hardware.
#include <platypus/renderer/Renderer.hpp>

#include <cassert>
#include <cstdio>
#include <vector>

namespace {

using namespace platypus;

class FakeDisplay final : public hal::IDisplay {
public:
    hal::DisplayInfo info() const noexcept override { return {8, 8, 16}; }
    hal::Status setBacklight(float) override { return {}; }
    hal::Status present(std::span<const std::byte> pixels) override {
        lastFrame_.assign(pixels.begin(), pixels.end());
        ++presentCount_;
        return {};
    }
    hal::Status onTouch(std::function<void(const hal::TouchEvent&)>) override { return {}; }
    hal::Status onButton(std::function<void(const hal::ButtonEvent&)>) override { return {}; }

    std::vector<std::byte> lastFrame_;
    int presentCount_ = 0;
};

}  // namespace

void test_renderer() {
    auto display = std::make_shared<FakeDisplay>();
    renderer::Renderer r(display);

    r.clear({255, 0, 0});
    assert(r.present().ok());
    assert(display->presentCount_ == 1);
    assert(display->lastFrame_.size() == 8u * 8u * 2u);

    // Red in RGB565 is 0xF800 (little-endian: 0x00 0xF8).
    assert(display->lastFrame_[0] == std::byte{0x00});
    assert(display->lastFrame_[1] == std::byte{0xF8});

    std::puts("test_renderer: OK");
}

void test_app_registry();
void test_mcu_framing();

int main() {
    test_app_registry();
    test_mcu_framing();
    test_renderer();
    std::puts("All tests passed.");
    return 0;
}
