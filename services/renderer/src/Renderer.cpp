#include "platypus/renderer/Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace platypus::renderer {

Renderer::Renderer(std::shared_ptr<hal::IDisplay> display)
    : display_(std::move(display)),
      info_(display_->info()),
      framebuffer_(static_cast<std::size_t>(info_.width) * info_.height, 0) {}

hal::DisplayInfo Renderer::displayInfo() const noexcept { return info_; }

void Renderer::setPixel(std::int32_t x, std::int32_t y, std::uint16_t rgb565) {
    if (x < 0 || y < 0 || x >= info_.width || y >= info_.height) return;
    framebuffer_[static_cast<std::size_t>(y) * info_.width + static_cast<std::size_t>(x)] = rgb565;
}

void Renderer::clear(Color color) {
    std::fill(framebuffer_.begin(), framebuffer_.end(), color.toRgb565());
}

void Renderer::fillRect(const Rect& rect, Color color) {
    const auto c = color.toRgb565();
    const auto x0 = std::max<std::int32_t>(rect.x, 0);
    const auto y0 = std::max<std::int32_t>(rect.y, 0);
    const auto x1 = std::min<std::int32_t>(rect.x + rect.w, info_.width);
    const auto y1 = std::min<std::int32_t>(rect.y + rect.h, info_.height);
    for (auto y = y0; y < y1; ++y)
        for (auto x = x0; x < x1; ++x)
            framebuffer_[static_cast<std::size_t>(y) * info_.width + static_cast<std::size_t>(x)] = c;
}

void Renderer::drawRect(const Rect& rect, Color color) {
    drawLine(rect.x, rect.y, rect.x + rect.w - 1, rect.y, color);
    drawLine(rect.x, rect.y + rect.h - 1, rect.x + rect.w - 1, rect.y + rect.h - 1, color);
    drawLine(rect.x, rect.y, rect.x, rect.y + rect.h - 1, color);
    drawLine(rect.x + rect.w - 1, rect.y, rect.x + rect.w - 1, rect.y + rect.h - 1, color);
}

void Renderer::drawLine(std::int32_t x0, std::int32_t y0,
                        std::int32_t x1, std::int32_t y1, Color color) {
    // Bresenham
    const auto c = color.toRgb565();
    const auto dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
    const auto sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    auto err = dx + dy;
    for (;;) {
        setPixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        const auto e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Renderer::drawText(std::int32_t x, std::int32_t y,
                        std::string_view text, Color color) {
    // Placeholder glyph rendering: 5x7 filled box per character until the
    // bitmap font lands (see ROADMAP: renderer/font-atlas).
    auto cx = x;
    for (const char ch : text) {
        if (ch != ' ') fillRect({cx, y, 5, 7}, color);
        cx += 6;
    }
}

hal::Status Renderer::present() {
    return display_->present(std::as_bytes(std::span(framebuffer_)));
}

}  // namespace platypus::renderer
