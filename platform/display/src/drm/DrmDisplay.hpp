// PlatypusOS — IDisplay on a local Linux KMS display (the UNO Q's own panel).
//
// Drives whatever the kernel exposes through DRM: the MIPI-DSI panel on the
// UNO Media Carrier in the product, or a DisplayPort/HDMI monitor through a
// USB-C hub on the bench. Nothing above the HAL can tell which (ADR-0001);
// ShadowScan, Scout and the launcher lay out against info(), never a literal.
//
// Mechanism: legacy KMS through raw ioctls on /dev/dri/cardN — one dumb buffer
// scanned out by one CRTC, no libdrm dependency. The renderer's RGB565 frame is
// converted to XRGB8888 on present(), because XRGB8888 is the one scanout
// format every KMS driver supports; betting on a panel's RGB565 plane is not
// worth one 800x480 conversion per frame.
//
// Single-buffered: present() writes the scanout buffer directly, so a frame can
// tear mid-update. Acceptable for a UI that redraws dirty regions; revisit with
// a page flip if ShadowScan's live view needs it.
//
// Ownership: programming a CRTC needs DRM master. If a display server (lightdm
// + Xorg on the stock UNO Q image) holds it, open() fails with Error::Busy —
// the HAL's "resource held by another client" — and diagnostic() says so.
//
// Input: touch comes from the panel's touch controller and buttons from
// gpio-keys, both as evdev devices, read on one thread that invokes the
// registered handlers directly (same contract as LinkedDisplay: composition
// roots post into appfw::EventQueue, so apps still run on the UI thread).
//
// Linux-only. Portable logic lives in KmsHelpers.hpp and is host-tested.
#pragma once

#include <platypus/hal/IDisplay.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace platypus::drm {

/// Button ids for the UNO Q's two gpio-keys buttons (reported by the kernel as
/// KEY_VOLUMEUP / KEY_VOLUMEDOWN). 254/255 are the encoder's synthetic ids.
inline constexpr std::uint8_t kBoardButtonA = 1;  ///< KEY_VOLUMEUP
inline constexpr std::uint8_t kBoardButtonB = 2;  ///< KEY_VOLUMEDOWN

struct DrmDisplayConfig {
    std::string devicePath = "/dev/dri/card0";
    /// DRM_MODE_CONNECTOR_* to prefer when several are connected (default DSI).
    std::uint32_t preferredConnectorType = 16;
    /// evdev device name for board buttons; empty disables button input.
    std::string buttonDeviceName = "gpio-keys";
    /// Look for a touchscreen (EV_ABS ABS_X + BTN_TOUCH) among evdev devices.
    bool enableTouch = true;
};

/// What open() found and chose, for bring-up tools and logs.
struct DrmSelection {
    std::uint32_t connectorId = 0;
    std::uint32_t connectorType = 0;
    std::uint32_t crtcId = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint32_t refreshHz = 0;
    std::string touchDevice;   ///< empty if none found
    std::string buttonDevice;  ///< empty if none found or disabled
};

class DrmDisplay final : public hal::IDisplay {
   public:
    explicit DrmDisplay(DrmDisplayConfig config = {});
    ~DrmDisplay() override;
    DrmDisplay(const DrmDisplay&) = delete;
    DrmDisplay& operator=(const DrmDisplay&) = delete;

    /// Finds a connected connector, programs its preferred mode with a blank
    /// scanout buffer, and starts input. Idempotent failure: on error nothing
    /// stays allocated and diagnostic() explains what went wrong.
    [[nodiscard]] hal::Status open();
    void close();

    [[nodiscard]] hal::DisplayInfo info() const noexcept override;
    hal::Status setBacklight(float brightness) override;
    hal::Status present(std::span<const std::byte> pixels) override;
    hal::Status presentRegion(std::span<const std::byte> pixels,
                              const hal::DisplayRegion& region) override;
    hal::Status onTouch(std::function<void(const hal::TouchEvent&)> handler) override;
    hal::Status onButton(std::function<void(const hal::ButtonEvent&)> handler) override;

    [[nodiscard]] const DrmSelection& selection() const noexcept { return selection_; }
    [[nodiscard]] const std::string& diagnostic() const noexcept { return diagnostic_; }

   private:
    hal::Status fail(hal::Error error, std::string why);
    hal::Status programDisplay();
    void startInput();
    void inputLoop();
    void releaseScanout();

    DrmDisplayConfig config_;
    DrmSelection selection_;
    std::string diagnostic_;

    int fd_ = -1;
    std::uint32_t fbId_ = 0;
    std::uint32_t dumbHandle_ = 0;
    std::uint32_t pitch_ = 0;
    std::size_t mapSize_ = 0;
    std::byte* map_ = nullptr;

    // CRTC state before open(), restored by close() so a display server or
    // console gets its picture back.
    struct SavedCrtc;
    std::unique_ptr<SavedCrtc> savedCrtc_;

    int touchFd_ = -1;
    int buttonFd_ = -1;
    int wakeFd_ = -1;  ///< eventfd that stops the input thread
    std::int32_t touchMinX_ = 0, touchMaxX_ = 0, touchMinY_ = 0, touchMaxY_ = 0;
    std::thread inputThread_;
    std::atomic<bool> running_{false};

    std::mutex handlerMutex_;
    std::function<void(const hal::TouchEvent&)> touchHandler_;
    std::function<void(const hal::ButtonEvent&)> buttonHandler_;
};

}  // namespace platypus::drm
