// PlatypusOS HAL — generic sensor abstraction.
//
// Designed for expansion: new sensor kinds are added by registering a driver
// with ISensorHub, not by editing any consumer. Readings are self-describing
// (kind + unit + channel names) so apps like `inspection` can display sensors
// that did not exist when the app shipped.
#pragma once

#include <platypus/hal/Result.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace platypus::hal {

/// Open enumeration: values above User are reserved for plugin sensors.
enum class SensorKind : std::uint16_t {
    Unknown = 0,
    Accelerometer,
    Gyroscope,
    Magnetometer,
    Temperature,
    Humidity,
    Pressure,
    Distance,     ///< ToF / ultrasonic rangefinders
    Light,
    User = 0x8000,
};

struct SensorDescriptor {
    std::string id;                  ///< stable unique id, e.g. "tof0"
    SensorKind kind = SensorKind::Unknown;
    std::string unit;                ///< SI unit string, e.g. "m/s^2"
    std::vector<std::string> channels;  ///< e.g. {"x","y","z"}
    float minRate = 0.0f, maxRate = 0.0f;  ///< Hz
};

struct SensorSample {
    std::string sensorId;
    std::chrono::steady_clock::time_point timestamp;
    std::vector<float> values;       ///< one per channel, descriptor order
};

class ISensor {
public:
    virtual ~ISensor() = default;

    [[nodiscard]] virtual const SensorDescriptor& descriptor() const noexcept = 0;

    virtual Status start(float rateHz) = 0;
    virtual Status stop() = 0;

    virtual Result<SensorSample> read() = 0;
    virtual Status onSample(std::function<void(const SensorSample&)> handler) = 0;
};

/// Registry + discovery. Hot-pluggable: drivers may attach/detach at runtime.
class ISensorHub {
public:
    virtual ~ISensorHub() = default;

    [[nodiscard]] virtual std::vector<SensorDescriptor> enumerate() const = 0;
    [[nodiscard]] virtual std::shared_ptr<ISensor> get(std::string_view id) = 0;

    virtual Status registerSensor(std::shared_ptr<ISensor> sensor) = 0;
    virtual Status unregisterSensor(std::string_view id) = 0;

    /// Fires on attach/detach so UI can update live.
    virtual Status onTopologyChanged(std::function<void()> handler) = 0;
};

}  // namespace platypus::hal
