// PlatypusOS HAL — bridge to the STM32U585 real-time core.
//
// On the Arduino UNO Q, low-latency I/O (GPIO, PWM, ADC, timing-critical
// sensor sampling) runs on the Cortex-M33 MCU while PlatypusOS runs on the
// Linux MPU. IMcuBridge abstracts the RPC transport between the two so the
// rest of the system never touches the wire protocol.
#pragma once

#include <platypus/hal/Result.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace platypus::hal {

enum class PinMode : std::uint8_t { Input, InputPullup, Output, Analog, Pwm };

class IMcuBridge {
public:
    virtual ~IMcuBridge() = default;

    virtual Status pinMode(std::uint8_t pin, PinMode mode) = 0;
    virtual Status digitalWrite(std::uint8_t pin, bool level) = 0;
    virtual Result<bool> digitalRead(std::uint8_t pin) = 0;
    virtual Result<std::uint16_t> analogRead(std::uint8_t pin) = 0;
    virtual Status pwmWrite(std::uint8_t pin, float duty) = 0;

    /// Raw message channel for custom firmware modules on the MCU side.
    /// Topic ids are allocated in docs/protocols/mcu-bridge.md.
    virtual Status publish(std::uint16_t topic, std::span<const std::byte> payload) = 0;
    virtual Status subscribe(std::uint16_t topic,
                             std::function<void(std::span<const std::byte>)> handler) = 0;

    /// Round-trip health check with bounded wait.
    virtual Status ping(std::chrono::milliseconds timeout) = 0;
};

}  // namespace platypus::hal
