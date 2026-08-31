// PlatypusOS services — immediate-mode renderer skeleton.
//
// Owns a CPU framebuffer (RGB565) and flushes it to an injected IDisplay.
// Apps draw through this API only; they never see the display driver.
// Future work: font atlas, GPU path on the MPU.
#pragma once

#include <platypus/hal/IDisplay.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace platypus::renderer {

struct Color {
    std::uint8_t r = 0, g = 0, b = 0;
    [[nodiscard]] std::uint16_t toRgb565() const noexcept {
        return static_cast<std::uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }
};

struct Rect {
    std::int32_t x = 0, y = 0, w = 0, h = 0;
};

class Renderer {
   public:
    /// The renderer does not own the display's lifetime policy; it holds a
    /// shared_ptr injected by the composition root.
    explicit Renderer(std::shared_ptr<hal::IDisplay> display);

    [[nodiscard]] hal::DisplayInfo displayInfo() const noexcept;

    void clear(Color color);
    void fillRect(const Rect& rect, Color color);
    void drawRect(const Rect& rect, Color color);
    void drawLine(std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1, Color color);

    /// Built-in 6x8 bitmap font (TODO: font atlas + scaling).
    void drawText(std::int32_t x, std::int32_t y, std::string_view text, Color color);

    /// Push the framebuffer to hardware.
    hal::Status present();

   private:
    void setPixel(std::int32_t x, std::int32_t y, std::uint16_t rgb565);
    void markDirty(const Rect& rect);

    std::shared_ptr<hal::IDisplay> display_;
    hal::DisplayInfo info_;
    std::vector<std::uint16_t> framebuffer_;
    std::optional<Rect> dirtyRect_;
};

}  // namespace platypus::renderer
