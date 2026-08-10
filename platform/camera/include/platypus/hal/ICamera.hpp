// PlatypusOS HAL — camera abstraction.
//
// Concrete backends: V4L2/libcamera on the UNO Q Linux core, a file-replay
// backend for tests, and future MIPI/USB modules. Frames are reference
// counted and zero-copy where the backend supports DMA buffers.
#pragma once

#include <platypus/hal/Result.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace platypus::hal {

enum class PixelFormat : std::uint8_t { Unknown, Gray8, RGB888, YUYV, MJPEG, NV12 };

struct CameraMode {
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    PixelFormat format = PixelFormat::Unknown;
    float fps = 0.0f;
};

/// One captured frame. Owns (or ref-counts) its pixel data; cheap to copy.
class Frame {
public:
    Frame() = default;
    Frame(CameraMode mode, std::shared_ptr<const std::vector<std::byte>> data,
          std::chrono::steady_clock::time_point timestamp)
        : mode_(mode), data_(std::move(data)), timestamp_(timestamp) {}

    [[nodiscard]] const CameraMode& mode() const noexcept { return mode_; }
    [[nodiscard]] std::span<const std::byte> pixels() const noexcept {
        return data_ ? std::span<const std::byte>(*data_) : std::span<const std::byte>{};
    }
    [[nodiscard]] auto timestamp() const noexcept { return timestamp_; }
    [[nodiscard]] bool empty() const noexcept { return !data_ || data_->empty(); }

private:
    CameraMode mode_{};
    std::shared_ptr<const std::vector<std::byte>> data_;
    std::chrono::steady_clock::time_point timestamp_{};
};

struct CameraControls {
    std::optional<float> exposureMs;   ///< nullopt = auto exposure
    std::optional<float> gain;         ///< nullopt = auto gain
    std::optional<float> focus;        ///< 0..1, nullopt = autofocus / fixed
};

class ICamera {
public:
    virtual ~ICamera() = default;

    [[nodiscard]] virtual std::vector<CameraMode> supportedModes() const = 0;

    virtual Status open(const CameraMode& mode) = 0;
    virtual Status close() = 0;
    [[nodiscard]] virtual bool isOpen() const noexcept = 0;

    virtual Status setControls(const CameraControls& controls) = 0;

    /// Blocking single capture with bounded wait.
    virtual Result<Frame> capture(std::chrono::milliseconds timeout) = 0;

    /// Streaming capture. The callback runs on the camera's own thread;
    /// handlers must be fast and must not call back into the camera.
    virtual Status startStream(std::function<void(const Frame&)> onFrame) = 0;
    virtual Status stopStream() = 0;
};

}  // namespace platypus::hal
