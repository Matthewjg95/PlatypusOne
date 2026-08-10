// PlatypusOS HAL — board composition root.
//
// IBoard is the single factory through which all hardware is obtained.
// Nothing in the system instantiates a concrete driver directly; instead a
// board implementation (UnoQBoard, HostSimBoard, future revisions) is created
// in main() and injected downward. This is the seam that keeps every other
// module hardware-agnostic and testable.
#pragma once

#include <memory>
#include <string_view>

namespace platypus::hal {

class IDisplay;
class ICamera;
class ISensorHub;
class IAudioOutput;
class IMcuBridge;
class IStorage;

/// Aggregates every hardware capability of one physical device.
/// Accessors return nullptr when the capability is absent — callers must
/// degrade gracefully (a Platypus One without a camera still boots).
class IBoard {
public:
    virtual ~IBoard() = default;

    /// Human-readable board identifier, e.g. "arduino-uno-q-r1".
    [[nodiscard]] virtual std::string_view id() const noexcept = 0;

    [[nodiscard]] virtual std::shared_ptr<IDisplay>     display()  = 0;
    [[nodiscard]] virtual std::shared_ptr<ICamera>      camera()   = 0;
    [[nodiscard]] virtual std::shared_ptr<ISensorHub>   sensors()  = 0;
    [[nodiscard]] virtual std::shared_ptr<IAudioOutput> audio()    = 0;
    [[nodiscard]] virtual std::shared_ptr<IStorage>     storage()  = 0;

    /// Link to the STM32U585 real-time core (GPIO, PWM, ADC live there
    /// on the UNO Q). May be null on a pure host simulation.
    [[nodiscard]] virtual std::shared_ptr<IMcuBridge>   mcuBridge() = 0;
};

}  // namespace platypus::hal
