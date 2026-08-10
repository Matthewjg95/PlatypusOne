// PlatypusOS HAL — error handling primitives.
//
// The HAL never throws across module boundaries. All fallible operations
// return Result<T> (a thin std::expected-style wrapper) so callers on both
// the Linux MPU side and MCU bridge side share one error model.
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace platypus::hal {

enum class Error : std::uint8_t {
    None = 0,
    NotSupported,     ///< Capability absent on this hardware revision.
    NotInitialized,   ///< open()/init() has not completed successfully.
    Busy,             ///< Resource held by another client.
    Timeout,          ///< Bounded wait expired (real-time paths must bound waits).
    IoFailure,        ///< Bus/driver level failure.
    InvalidArgument,
    OutOfRange,
    HardwareFault,    ///< Device reported an unrecoverable fault.
};

constexpr std::string_view to_string(Error e) noexcept {
    switch (e) {
        case Error::None:            return "None";
        case Error::NotSupported:    return "NotSupported";
        case Error::NotInitialized:  return "NotInitialized";
        case Error::Busy:            return "Busy";
        case Error::Timeout:         return "Timeout";
        case Error::IoFailure:       return "IoFailure";
        case Error::InvalidArgument: return "InvalidArgument";
        case Error::OutOfRange:      return "OutOfRange";
        case Error::HardwareFault:   return "HardwareFault";
    }
    return "Unknown";
}

/// Minimal expected-like result. Kept dependency-free so the header can be
/// consumed by firmware toolchains without full libstdc++.
template <typename T>
class Result {
public:
    Result(T value) : value_(std::move(value)), error_(Error::None) {}
    Result(Error error) : error_(error) {}

    [[nodiscard]] bool ok() const noexcept { return error_ == Error::None; }
    explicit operator bool() const noexcept { return ok(); }

    [[nodiscard]] Error error() const noexcept { return error_; }
    [[nodiscard]] T& value() { return *value_; }
    [[nodiscard]] const T& value() const { return *value_; }

private:
    std::optional<T> value_;
    Error error_;
};

/// Result for void-returning operations.
class Status {
public:
    Status() : error_(Error::None) {}
    Status(Error error) : error_(error) {}

    [[nodiscard]] bool ok() const noexcept { return error_ == Error::None; }
    explicit operator bool() const noexcept { return ok(); }
    [[nodiscard]] Error error() const noexcept { return error_; }

private:
    Error error_;
};

}  // namespace platypus::hal
