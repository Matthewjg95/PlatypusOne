// PlatypusOS — Win32 simulation display (host development only).
//
// Renders an RGB565 framebuffer into a native window and maps mouse input to
// touch events. Panel geometry is injected, not fixed, so the simulator can
// follow whichever prototype display is current (ADR-0001); the window scale
// is chosen to keep that geometry on a typical developer monitor. Zero
// third-party dependencies (GDI only). Compiled only on _WIN32; POSIX hosts
// fall back to the headless SimDisplay until an SDL/X11 backend exists.
#pragma once
#ifdef _WIN32

#include <platypus/hal/IDisplay.hpp>

#include <memory>

namespace platypus::sim {

class Win32SimDisplay final : public hal::IDisplay {
public:
    explicit Win32SimDisplay(hal::DisplayInfo geometry);
    ~Win32SimDisplay() override;

    Win32SimDisplay(const Win32SimDisplay&) = delete;
    Win32SimDisplay& operator=(const Win32SimDisplay&) = delete;

    hal::DisplayInfo info() const noexcept override;
    hal::Status setBacklight(float brightness) override;
    hal::Status present(std::span<const std::byte> pixels) override;
    hal::Status onTouch(std::function<void(const hal::TouchEvent&)> handler) override;
    hal::Status onButton(std::function<void(const hal::ButtonEvent&)> handler) override;

private:
    struct Impl;                    ///< hides <windows.h> from consumers
    std::unique_ptr<Impl> impl_;
};

}  // namespace platypus::sim

#endif  // _WIN32
