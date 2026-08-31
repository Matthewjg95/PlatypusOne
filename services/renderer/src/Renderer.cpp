#include "platypus/renderer/Renderer.hpp"

#include "Font5x7.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace platypus::renderer {

Renderer::Renderer(std::shared_ptr<hal::IDisplay> display)
    : display_(std::move(display)),
      info_(display_->info()),
      framebuffer_(static_cast<std::size_t>(info_.width) * info_.height, 0),
      dirtyRect_(Rect{0, 0, info_.width, info_.height}) {}

hal::DisplayInfo Renderer::displayInfo() const noexcept {
    return info_;
}

void Renderer::setPixel(std::int32_t x, std::int32_t y, std::uint16_t rgb565) {
    if (x < 0 || y < 0 || x >= info_.width || y >= info_.height) return;
    const auto index = static_cast<std::size_t>(y) * info_.width + static_cast<std::size_t>(x);
    if (framebuffer_[index] == rgb565) return;
    framebuffer_[index] = rgb565;
    markDirty({x, y, 1, 1});
}

void Renderer::markDirty(const Rect& rect) {
    const auto x0 = std::max<std::int32_t>(rect.x, 0);
    const auto y0 = std::max<std::int32_t>(rect.y, 0);
    const auto x1 = std::min<std::int32_t>(rect.x + rect.w, info_.width);
    const auto y1 = std::min<std::int32_t>(rect.y + rect.h, info_.height);
    if (x0 >= x1 || y0 >= y1) return;

    if (!dirtyRect_) {
        dirtyRect_ = Rect{x0, y0, x1 - x0, y1 - y0};
        return;
    }
    const auto dirtyX1 = dirtyRect_->x + dirtyRect_->w;
    const auto dirtyY1 = dirtyRect_->y + dirtyRect_->h;
    const auto mergedX0 = std::min(dirtyRect_->x, x0);
    const auto mergedY0 = std::min(dirtyRect_->y, y0);
    const auto mergedX1 = std::max(dirtyX1, x1);
    const auto mergedY1 = std::max(dirtyY1, y1);
    dirtyRect_ = Rect{mergedX0, mergedY0, mergedX1 - mergedX0, mergedY1 - mergedY0};
}

void Renderer::clear(Color color) {
    std::fill(framebuffer_.begin(), framebuffer_.end(), color.toRgb565());
    markDirty({0, 0, info_.width, info_.height});
}

void Renderer::fillRect(const Rect& rect, Color color) {
    const auto c = color.toRgb565();
    const auto x0 = std::max<std::int32_t>(rect.x, 0);
    const auto y0 = std::max<std::int32_t>(rect.y, 0);
    const auto x1 = std::min<std::int32_t>(rect.x + rect.w, info_.width);
    const auto y1 = std::min<std::int32_t>(rect.y + rect.h, info_.height);
    for (auto y = y0; y < y1; ++y)
        for (auto x = x0; x < x1; ++x)
            framebuffer_[static_cast<std::size_t>(y) * info_.width + static_cast<std::size_t>(x)] =
                c;
    markDirty({x0, y0, x1 - x0, y1 - y0});
}

void Renderer::drawRect(const Rect& rect, Color color) {
    drawLine(rect.x, rect.y, rect.x + rect.w - 1, rect.y, color);
    drawLine(rect.x, rect.y + rect.h - 1, rect.x + rect.w - 1, rect.y + rect.h - 1, color);
    drawLine(rect.x, rect.y, rect.x, rect.y + rect.h - 1, color);
    drawLine(rect.x + rect.w - 1, rect.y, rect.x + rect.w - 1, rect.y + rect.h - 1, color);
}

void Renderer::drawLine(std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1,
                        Color color) {
    // Bresenham
    const auto c = color.toRgb565();
    const auto dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
    const auto sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    auto err = dx + dy;
    for (;;) {
        setPixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        const auto e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void Renderer::drawText(std::int32_t x, std::int32_t y, std::string_view text, Color color,
                        std::int32_t scale) {
    if (scale < 1) return;
    const auto c = color.toRgb565();
    auto cx = x;
    for (const char ch : text) {
        const auto& glyph = font::glyphFor(ch);
        for (std::int32_t col = 0; col < font::kGlyphWidth; ++col) {
            const auto bits = glyph[static_cast<std::size_t>(col)];
            for (std::int32_t row = 0; row < font::kGlyphHeight; ++row) {
                if ((bits & (1u << row)) == 0) continue;
                if (scale == 1) {
                    setPixel(cx + col, y + row, c);
                } else {
                    fillRect({cx + col * scale, y + row * scale, scale, scale}, color);
                }
            }
        }
        cx += font::kGlyphAdvance * scale;
    }
}

std::int32_t Renderer::textWidth(std::string_view text, std::int32_t scale) noexcept {
    if (scale < 1) return 0;
    return static_cast<std::int32_t>(text.size()) * font::kGlyphAdvance * scale;
}

hal::Status Renderer::present() {
    if (!dirtyRect_) return {};

    const hal::DisplayRegion region{
        static_cast<std::uint16_t>(dirtyRect_->x),
        static_cast<std::uint16_t>(dirtyRect_->y),
        static_cast<std::uint16_t>(dirtyRect_->w),
        static_cast<std::uint16_t>(dirtyRect_->h),
    };
    auto status = display_->presentRegion(std::as_bytes(std::span(framebuffer_)), region);
    if (status.ok()) dirtyRect_.reset();
    return status;
}

}  // namespace platypus::renderer
