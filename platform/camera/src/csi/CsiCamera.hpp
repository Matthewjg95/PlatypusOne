// PlatypusOS — ICamera backend for an IMX219 on the UNO Q's CSI path (camss).
//
// Reads raw SRGGB10 (MIPI-packed 'pRAA') straight from the camss RDI video
// node, so nothing uncalibrated sits between the sensor and the analyzer:
// libcamera's simple pipeline on this board has no IMX219 tuning and does not
// expose a raw stream. open() configures the media graph itself (sensor ->
// csiphyN -> csid0 -> vfe0_rdi0), equivalent to the media-ctl commands in
// docs/hardware/CSI_CAMERA_BENCH.md.
//
// Frames are RGB888 at half resolution (each 2x2 Bayer cell -> one pixel,
// 1640x1232 -> 820x616), gray-world balanced and gamma-encoded like a webcam's
// output. Auto-exposure drives the sensor's own exposure/gain controls, and
// the values used are reported by deviceIdentity() so records can carry them.
//
// Linux-only. Threading as V4l2Camera: capture() on the caller's thread;
// startStream() owns one reader thread.
#pragma once

#include <platypus/hal/Bayer.hpp>
#include <platypus/hal/ICamera.hpp>

#include <array>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace platypus::unoq {

class CsiCamera final : public hal::ICamera {
   public:
    /// Raw sensor mode read from camss (the IMX219's 2x2-binned full-FOV mode).
    static constexpr std::uint32_t kRawWidth = 1640;
    static constexpr std::uint32_t kRawHeight = 1232;
    static constexpr hal::CameraMode kMode{kRawWidth / 2, kRawHeight / 2, hal::PixelFormat::RGB888,
                                           30.0f};

    CsiCamera();
    ~CsiCamera() override;
    CsiCamera(const CsiCamera&) = delete;
    CsiCamera& operator=(const CsiCamera&) = delete;

    /// True when camss has a bound IMX219 sensor entity (cheap; no streaming).
    [[nodiscard]] static bool sensorPresent();

    /// "imx219 3-0010 csi raw10 1640x1232->820x616 exp=3415 gain=12 dgain=256
    /// line_ns=9759", or the last error while closed. Exposure and gains are
    /// those of the latest frame; exposure time is exp x line_ns.
    [[nodiscard]] std::string deviceIdentity() const;

    [[nodiscard]] std::vector<hal::CameraMode> supportedModes() const override { return {kMode}; }
    hal::Status open(const hal::CameraMode& mode) override;
    hal::Status close() override;
    [[nodiscard]] bool isOpen() const noexcept override { return videoFd_ >= 0 && streaming_; }
    hal::Status setControls(const hal::CameraControls& controls) override;
    hal::Result<hal::Frame> capture(std::chrono::milliseconds timeout) override;
    hal::Status startStream(std::function<void(const hal::Frame&)> onFrame) override;
    hal::Status stopStream() override;

   private:
    struct MappedBuffer {
        void* start = nullptr;
        std::size_t length = 0;
    };

    hal::Status configureGraph();
    hal::Result<hal::Frame> dequeueFrame(std::chrono::milliseconds timeout);
    void applyExposure(const hal::bayer::Exposure& e);
    void releaseBuffers();

    int mediaFd_ = -1;
    int videoFd_ = -1;
    int sensorFd_ = -1;
    std::string sensorName_;
    std::string videoPath_;
    std::uint32_t bytesPerLine_ = 0;
    bool mplane_ = false;  ///< camss video nodes may be multi-planar
    std::vector<MappedBuffer> buffers_;
    bool streaming_ = false;

    hal::bayer::ExposureLimits limits_{};
    hal::bayer::Exposure exposure_{1600, 0};
    std::uint32_t lineTimeNs_ = 0;  ///< sensor line period, recorded with each capture
    std::array<float, 3> wbGains_{1.0f, 1.0f, 1.0f};
    bool autoExposure_ = true;
    mutable std::mutex stateMutex_;
    std::string lastError_;

    std::thread streamThread_;
    std::atomic<bool> streamRunning_{false};
};

}  // namespace platypus::unoq
