// PlatypusOS — ICamera backend for V4L2 UVC cameras (UNO Q Linux side).
//
// The smallest maintainable still-capture path: open a /dev/video* node,
// negotiate a format, memory-map a small buffer ring, stream, and dequeue
// frames. Not a general multimedia framework — no format conversion, no
// processing; frames carry whatever bytes the driver produced (MJPEG stays
// MJPEG, YUYV stays YUYV) and consumers decide what to do with them.
//
// Linux-only: excluded from non-UNIX builds by CMake. Threading: capture()
// runs on the caller's thread; startStream() owns one reader thread and
// invokes the callback on it (handlers must be brief, per ICamera contract).
#pragma once

#include <platypus/hal/ICamera.hpp>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace platypus::unoq {

class V4l2Camera final : public hal::ICamera {
public:
    /// devicePath e.g. "/dev/video0".
    explicit V4l2Camera(std::string devicePath);
    ~V4l2Camera() override;

    V4l2Camera(const V4l2Camera&) = delete;
    V4l2Camera& operator=(const V4l2Camera&) = delete;

    /// Driver + card identity ("uvcvideo: USB 2.0 Camera"), best-effort;
    /// empty until the device has been probed. Feeds observation metadata.
    [[nodiscard]] std::string deviceIdentity() const;

    [[nodiscard]] std::vector<hal::CameraMode> supportedModes() const override;

    hal::Status open(const hal::CameraMode& mode) override;
    hal::Status close() override;
    [[nodiscard]] bool isOpen() const noexcept override;

    hal::Status setControls(const hal::CameraControls& controls) override;

    hal::Result<hal::Frame> capture(std::chrono::milliseconds timeout) override;

    hal::Status startStream(std::function<void(const hal::Frame&)> onFrame) override;
    hal::Status stopStream() override;

private:
    struct MappedBuffer {
        void* start = nullptr;
        std::size_t length = 0;
    };

    hal::Result<hal::Frame> dequeueFrame(std::chrono::milliseconds timeout);
    void releaseBuffers();

    std::string devicePath_;
    int fd_ = -1;
    hal::CameraMode mode_{};
    std::vector<MappedBuffer> buffers_;
    bool streaming_ = false;

    std::thread streamThread_;
    std::atomic<bool> streamRunning_{false};
};

}  // namespace platypus::unoq
