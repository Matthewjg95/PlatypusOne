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
    explicit FakeDisplay(hal::DisplayInfo info = {8, 8, 16}) : info_(info) {}

    hal::DisplayInfo info() const noexcept override { return info_; }
    hal::Status setBacklight(float) override { return {}; }
    hal::Status present(std::span<const std::byte> pixels) override {
        lastFrame_.assign(pixels.begin(), pixels.end());
        ++presentCount_;
        return {};
    }
    hal::Status presentRegion(std::span<const std::byte> pixels,
                              const hal::DisplayRegion& region) override {
        lastRegion_ = region;
        ++regionAttemptCount_;
        if (failNextRegion_) {
            failNextRegion_ = false;
            return hal::Error::IoFailure;
        }
        return present(pixels);
    }
    hal::Status onTouch(std::function<void(const hal::TouchEvent&)>) override { return {}; }
    hal::Status onButton(std::function<void(const hal::ButtonEvent&)>) override { return {}; }

    std::vector<std::byte> lastFrame_;
    int presentCount_ = 0;
    int regionAttemptCount_ = 0;
    bool failNextRegion_ = false;
    hal::DisplayRegion lastRegion_{};

   private:
    hal::DisplayInfo info_;
};

}  // namespace

void test_renderer() {
    auto display = std::make_shared<FakeDisplay>();
    renderer::Renderer r(display);

    r.clear({255, 0, 0});
    assert(r.present().ok());
    assert(display->presentCount_ == 1);
    assert(display->lastFrame_.size() == 8u * 8u * 2u);
    assert(display->lastRegion_.width == 8);
    assert(display->lastRegion_.height == 8);

    // Red in RGB565 is 0xF800 (little-endian: 0x00 0xF8).
    assert(display->lastFrame_[0] == std::byte{0x00});
    assert(display->lastFrame_[1] == std::byte{0xF8});

    // No drawing means no transport work on the next present.
    assert(r.present().ok());
    assert(display->presentCount_ == 1);

    r.fillRect({2, 3, 3, 2}, {0, 255, 0});
    assert(r.present().ok());
    assert(display->presentCount_ == 2);
    assert(display->lastRegion_.x == 2);
    assert(display->lastRegion_.y == 3);
    assert(display->lastRegion_.width == 3);
    assert(display->lastRegion_.height == 2);

    // A failed transport must retain the dirty region for retry.
    r.fillRect({1, 5, 2, 2}, {0, 0, 255});
    display->failNextRegion_ = true;
    assert(!r.present().ok());
    assert(display->presentCount_ == 2);
    assert(r.present().ok());
    assert(display->presentCount_ == 3);
    assert(display->lastRegion_.x == 1);
    assert(display->lastRegion_.y == 5);
    assert(display->lastRegion_.width == 2);
    assert(display->lastRegion_.height == 2);

    // Dirty regions clip to the display and merge across draw calls.
    r.fillRect({-2, 4, 4, 10}, {0, 0, 255});
    r.fillRect({6, -2, 4, 4}, {0, 0, 255});
    assert(r.present().ok());
    assert(display->lastRegion_.x == 0);
    assert(display->lastRegion_.y == 0);
    assert(display->lastRegion_.width == 8);
    assert(display->lastRegion_.height == 8);

    std::puts("test_renderer: OK");
}

// ADR-0001: display geometry is discovered at runtime, so the renderer must
// track whatever the panel reports — including a linked prototype display at
// 800x480 — with no resolution compiled in anywhere.
void test_renderer_geometry() {
    auto display = std::make_shared<FakeDisplay>(hal::DisplayInfo{800, 480, 16});
    renderer::Renderer r(display);

    assert(r.displayInfo().width == 800);
    assert(r.displayInfo().height == 480);

    r.clear({0, 0, 0});
    // Drawing past the old 320x240 bounds must land in the framebuffer, and
    // drawing past the real bounds must clip rather than corrupt memory.
    r.fillRect({700, 400, 200, 200}, {0, 0, 255});
    assert(r.present().ok());
    assert(display->lastFrame_.size() == 800u * 480u * 2u);

    // Blue is 0x001F; check the pixel at (799, 479), the far corner.
    const auto corner = (479u * 800u + 799u) * 2u;
    assert(display->lastFrame_[corner] == std::byte{0x1F});
    assert(display->lastFrame_[corner + 1] == std::byte{0x00});

    std::puts("test_renderer_geometry: OK");
}

void test_app_registry();
void test_event_queue();
void test_mcu_framing();
void test_presentation_framing();

int main() {
    test_app_registry();
    test_event_queue();
    test_mcu_framing();
    test_presentation_framing();
    test_renderer();
    test_renderer_geometry();
    std::puts("All tests passed.");
    return 0;
}
