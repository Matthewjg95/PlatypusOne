// PlatypusOS HAL testing support — deterministic fake camera.
//
// Implements ICamera with a synthetic fixture frame so Scout logic, the
// evidence capture service, and unit tests run with no hardware attached
// (host-first rule). The pixel pattern is a pure function of (x, y), so a
// capture is byte-identical on every run and every platform.
//
// Header-only and dependency-free. Lives under hal/testing because it is a
// test/development double of a platform interface — production code must not
// link it in as a real camera.
#pragma once

#include <platypus/hal/ICamera.hpp>

#include <cstddef>

namespace platypus::hal::testing {

class FakeCamera final : public ICamera {
   public:
    static constexpr CameraMode kFixtureMode{640, 480, PixelFormat::RGB888, 30.0f};

    /// Failure injection for tests: refuse open(), or fail capture().
    struct Behavior {
        bool failOpen = false;
        bool failCapture = false;
    };

    explicit FakeCamera(Behavior behavior = {}) : behavior_(behavior) {}

    [[nodiscard]] std::vector<CameraMode> supportedModes() const override { return {kFixtureMode}; }

    Status open(const CameraMode& mode) override {
        if (behavior_.failOpen) return Error::HardwareFault;
        if (mode.width != kFixtureMode.width || mode.height != kFixtureMode.height ||
            mode.format != kFixtureMode.format)
            return Error::NotSupported;
        open_ = true;
        return {};
    }

    Status close() override {
        open_ = false;
        return {};
    }

    [[nodiscard]] bool isOpen() const noexcept override { return open_; }

    Status setControls(const CameraControls&) override { return {}; }

    Result<Frame> capture(std::chrono::milliseconds) override {
        if (!open_) return Error::NotInitialized;
        if (behavior_.failCapture) return Error::Timeout;
        ++captureCount_;
        return Frame(kFixtureMode, fixturePixels(),
                     std::chrono::steady_clock::time_point{
                         std::chrono::steady_clock::duration{captureCount_}});
    }

    Status startStream(std::function<void(const Frame&)> onFrame) override {
        // Synchronous single-frame "stream": enough for logic tests without
        // threads. Real streaming behavior belongs to hardware backends.
        if (!open_) return Error::NotInitialized;
        auto frame = capture(std::chrono::milliseconds(0));
        if (!frame) return frame.error();
        onFrame(frame.value());
        return {};
    }

    Status stopStream() override { return {}; }

    [[nodiscard]] int captureCount() const noexcept { return captureCount_; }

    /// The deterministic fixture: an RGB gradient with an 8px checkerboard
    /// band across the top rows (edges for future detector tests).
    static std::shared_ptr<const std::vector<std::byte>> fixturePixels() {
        auto pixels = std::make_shared<std::vector<std::byte>>();
        pixels->reserve(static_cast<std::size_t>(kFixtureMode.width) * kFixtureMode.height * 3);
        for (int y = 0; y < kFixtureMode.height; ++y) {
            for (int x = 0; x < kFixtureMode.width; ++x) {
                std::uint8_t r, g, b;
                if (y < 32) {  // checkerboard band
                    const bool onSquare = ((x / 8) + (y / 8)) % 2 == 0;
                    r = g = b = onSquare ? 255 : 0;
                } else {  // gradient body
                    r = static_cast<std::uint8_t>(x * 255 / (kFixtureMode.width - 1));
                    g = static_cast<std::uint8_t>(y * 255 / (kFixtureMode.height - 1));
                    b = static_cast<std::uint8_t>((x ^ y) & 0xFF);
                }
                pixels->push_back(std::byte{r});
                pixels->push_back(std::byte{g});
                pixels->push_back(std::byte{b});
            }
        }
        return pixels;
    }

   private:
    Behavior behavior_;
    bool open_ = false;
    int captureCount_ = 0;
};

}  // namespace platypus::hal::testing
