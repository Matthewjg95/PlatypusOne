// PlatypusOS HAL testing support — deterministic synthetic scene builder.
//
// Draws dark shapes on a light Gray8 canvas and hands the result out as a
// hal::Frame, so vision code can be exercised against scenes with exactly
// known geometry. Pixel membership is a pure function of shape parameters:
// identical calls produce identical frames on every platform.
//
// Lives beside FakeCamera because it is test/development support for the
// camera seam — production code must not link it.
#pragma once

#include <platypus/hal/ICamera.hpp>

#include <cmath>
#include <memory>
#include <vector>

namespace platypus::hal::testing {

class SyntheticScene {
   public:
    explicit SyntheticScene(std::uint16_t width = 640, std::uint16_t height = 480,
                            std::uint8_t background = 235, std::uint8_t ink = 20)
        : width_(width),
          height_(height),
          ink_(ink),
          background_(background),
          pixels_(std::make_shared<std::vector<std::byte>>(static_cast<std::size_t>(width) * height,
                                                           std::byte{background})) {}

    /// Filled axis-aligned square, top-left at (x, y).
    void addSquare(std::int32_t x, std::int32_t y, std::int32_t side) {
        for (std::int32_t dy = 0; dy < side; ++dy)
            for (std::int32_t dx = 0; dx < side; ++dx)
                set(x + dx, y + dy, ink_);
    }

    /// Filled length-by-width rectangle centred at (cx, cy), rotated angleRad.
    void addRect(double cx, double cy, double length, double width, double angleRad) {
        const double c = std::cos(angleRad);
        const double s = std::sin(angleRad);
        for (std::int32_t y = 0; y < height_; ++y)
            for (std::int32_t x = 0; x < width_; ++x) {
                const double dx = x - cx;
                const double dy = y - cy;
                const double u = c * dx + s * dy;
                const double v = -s * dx + c * dy;
                if (std::abs(u) <= length / 2.0 && std::abs(v) <= width / 2.0) set(x, y, ink_);
            }
    }

    /// Filled regular hexagon by across-flats width, rotated angleRad
    /// (0 = flat-top).
    void addHexagon(double cx, double cy, double acrossFlats, double angleRad = 0.0) {
        const double half = acrossFlats / 2.0;
        const double root3 = std::sqrt(3.0);
        const double c = std::cos(angleRad);
        const double s = std::sin(angleRad);
        for (std::int32_t y = 0; y < height_; ++y)
            for (std::int32_t x = 0; x < width_; ++x) {
                const double rx = x - cx;
                const double ry = y - cy;
                const double dx = c * rx + s * ry;
                const double dy = -s * rx + c * ry;
                if (std::abs(dy) <= half && std::abs(root3 * dx + dy) <= 2.0 * half &&
                    std::abs(root3 * dx - dy) <= 2.0 * half)
                    set(x, y, ink_);
            }
    }

    /// Filled disc.
    void addDisc(double cx, double cy, double radius) { fillCircle(cx, cy, radius, ink_); }

    /// Erase a disc back to background — a bore through whatever is beneath.
    void addBore(double cx, double cy, double radius) { fillCircle(cx, cy, radius, background_); }

    [[nodiscard]] Frame frame() const {
        return Frame({static_cast<std::uint16_t>(width_), static_cast<std::uint16_t>(height_),
                      PixelFormat::Gray8, 30.0f},
                     pixels_, std::chrono::steady_clock::time_point{});
    }

   private:
    void fillCircle(double cx, double cy, double radius, std::uint8_t value) {
        for (std::int32_t y = 0; y < height_; ++y)
            for (std::int32_t x = 0; x < width_; ++x) {
                const double dx = x - cx;
                const double dy = y - cy;
                if (dx * dx + dy * dy <= radius * radius) set(x, y, value);
            }
    }

    void set(std::int32_t x, std::int32_t y, std::uint8_t value) {
        if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
        (*pixels_)[static_cast<std::size_t>(y) * width_ + static_cast<std::size_t>(x)] =
            std::byte{value};
    }

    std::int32_t width_;
    std::int32_t height_;
    std::uint8_t ink_;
    std::uint8_t background_;
    std::shared_ptr<std::vector<std::byte>> pixels_;
};

}  // namespace platypus::hal::testing
