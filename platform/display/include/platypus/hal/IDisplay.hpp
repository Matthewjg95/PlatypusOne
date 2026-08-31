// PlatypusOS HAL — display + input abstraction.
//
// The display is a dumb framebuffer sink; all drawing intelligence lives in
// services/renderer. Touch/button input is surfaced here because on handheld
// hardware they arrive through the same panel driver.
#pragma once

#include <platypus/hal/Result.hpp>

#include <cstdint>
#include <functional>
#include <span>

namespace platypus::hal {

struct DisplayInfo {
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint8_t bitsPerPixel = 16;  ///< RGB565 default for SPI panels
};

struct TouchEvent {
    enum class Type : std::uint8_t { Down, Move, Up } type;
    std::uint16_t x = 0;
    std::uint16_t y = 0;
};

struct ButtonEvent {
    std::uint8_t id = 0;
    bool pressed = false;
};

/// Rectangle within the full framebuffer that changed since the last
/// successful present. Pixels passed to presentRegion remain laid out as a
/// full framebuffer; the driver may transmit only this region.
struct DisplayRegion {
    std::uint16_t x = 0;
    std::uint16_t y = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
};

class IDisplay {
   public:
    virtual ~IDisplay() = default;

    [[nodiscard]] virtual DisplayInfo info() const noexcept = 0;

    virtual Status setBacklight(float brightness) = 0;  ///< 0..1

    /// Present one full frame. `pixels` size must equal
    /// width * height * bitsPerPixel / 8; format matches info().
    virtual Status present(std::span<const std::byte> pixels) = 0;

    /// Present a changed region from a full-frame pixel buffer. Drivers that
    /// do not support partial updates retain correct behavior through this
    /// full-frame fallback.
    virtual Status presentRegion(std::span<const std::byte> pixels, const DisplayRegion& region) {
        (void)region;
        return present(pixels);
    }

    virtual Status onTouch(std::function<void(const TouchEvent&)> handler) = 0;
    virtual Status onButton(std::function<void(const ButtonEvent&)> handler) = 0;
};

}  // namespace platypus::hal
