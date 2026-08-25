// PlatypusOS — host simulation board.
//
// Lets the entire OS build and run on a developer workstation with no
// hardware attached: an in-memory display, temp-dir storage, and absent
// (nullptr) camera/sensors/audio. The real UnoQBoard implementation will
// live in platform/hardware/src once board bring-up starts (see ROADMAP).
#pragma once

#include <platypus/hal/IBoard.hpp>
#include <platypus/hal/IDisplay.hpp>
#include <platypus/hal/IStorage.hpp>

namespace platypus::sim {

/// Geometry the simulator falls back to when the caller names none. This is a
/// convenience default, NOT a product decision: ADR-0001 keeps the panel
/// deliberately dynamic, so nothing may treat this as "the" resolution.
inline constexpr hal::DisplayInfo kDefaultSimGeometry{320, 240, 16};

class HostSimBoard final : public hal::IBoard {
public:
    /// @param geometry panel size to simulate — pass whichever prototype
    ///        display is on the bench (see --geometry in main.cpp).
    explicit HostSimBoard(hal::DisplayInfo geometry = kDefaultSimGeometry);

    [[nodiscard]] std::string_view id() const noexcept override { return "host-sim"; }

    std::shared_ptr<hal::IDisplay>     display() override { return display_; }
    std::shared_ptr<hal::ICamera>      camera() override { return nullptr; }
    std::shared_ptr<hal::ISensorHub>   sensors() override { return nullptr; }
    std::shared_ptr<hal::IAudioOutput> audio() override { return nullptr; }
    std::shared_ptr<hal::IStorage>     storage() override { return storage_; }
    std::shared_ptr<hal::IMcuBridge>   mcuBridge() override { return nullptr; }

private:
    std::shared_ptr<hal::IDisplay> display_;
    std::shared_ptr<hal::IStorage> storage_;
};

}  // namespace platypus::sim
