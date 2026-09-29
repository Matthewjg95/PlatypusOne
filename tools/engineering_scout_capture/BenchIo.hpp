// Linux-only bench I/O for the capture harness: the board's GPIO button as a
// physical capture trigger, and its user LED as operator feedback.
//
// The UNO Q exposes its two GPIO buttons through the kernel's gpio-keys driver
// as an ordinary evdev device (they report KEY_VOLUMEUP / KEY_VOLUMEDOWN), and
// its RGB user LED through /sys/class/leds. Both are group-readable/writable by
// the `arduino` user (groups `input` and `gpiod`), so the harness needs no
// elevated privileges.
//
// This is deliberately a bench facility, not a HAL interface. A physical
// trigger that belongs to the product is an MCU concern — an STM32 button event
// arriving over the mcu-bridge, which keeps deterministic I/O on the MCU side
// where PlatypusOne's architecture puts it. This header exists so the capture
// loop can be exercised end to end with the hardware actually on the bench.

#pragma once

#ifdef __linux__

#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace platypus::bench {

/// Path of the first evdev node whose device name matches `name`, if any.
/// The event node's number is not stable across boots or hotplug, so the
/// device is always resolved by name rather than hard-coded to event0.
inline std::optional<std::string> findEventDevice(std::string_view name) {
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator("/sys/class/input", ec)) {
        const auto base = entry.path().filename().string();
        if (base.rfind("event", 0) != 0) continue;
        std::ifstream in(entry.path() / "device" / "name");
        std::string deviceName;
        if (!std::getline(in, deviceName)) continue;
        if (deviceName == name) return "/dev/input/" + base;
    }
    return std::nullopt;
}

/// Blocking reader for one evdev keyboard-class device.
class ButtonTrigger {
   public:
    ButtonTrigger() = default;
    ButtonTrigger(const ButtonTrigger&) = delete;
    ButtonTrigger& operator=(const ButtonTrigger&) = delete;
    ~ButtonTrigger() {
        if (fd_ >= 0) ::close(fd_);
    }

    /// Opens `path` for reading. Returns false with `error()` set on failure.
    bool open(const std::string& path) {
        fd_ = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd_ < 0) {
            error_ = std::strerror(errno);
            return false;
        }
        path_ = path;
        return true;
    }

    /// Blocks until a key is pressed, and returns its code. Key releases and
    /// auto-repeats are skipped so one physical press yields one capture.
    /// Returns nullopt only if the device disappears (unplug, or a read error).
    std::optional<int> waitForPress() {
        input_event event{};
        while (true) {
            const auto got = ::read(fd_, &event, sizeof(event));
            if (got != static_cast<ssize_t>(sizeof(event))) {
                if (got < 0 && errno == EINTR) continue;  // signal during read
                error_ = got < 0 ? std::strerror(errno) : "short read from evdev";
                return std::nullopt;
            }
            if (event.type == EV_KEY && event.value == 1) return event.code;
        }
    }

    const std::string& path() const { return path_; }
    const std::string& error() const { return error_; }

   private:
    int fd_ = -1;
    std::string path_;
    std::string error_;
};

/// One of the board's user LEDs, used as capture feedback. Every operation is
/// best-effort: the LED is a convenience, and a bench run must never fail
/// because a sysfs node moved or is not writable.
class StatusLed {
   public:
    explicit StatusLed(std::string colour) : path_("/sys/class/leds/" + colour + ":user/") {}

    bool available() const {
        std::error_code ec;
        return std::filesystem::exists(path_ + "brightness", ec);
    }

    void on() const { write(maxBrightness()); }
    void off() const { write(0); }

   private:
    int maxBrightness() const {
        std::ifstream in(path_ + "max_brightness");
        int value = 1;
        if (in >> value && value > 0) return value;
        return 1;
    }

    void write(int value) const {
        std::ofstream out(path_ + "brightness");
        if (out) out << value << '\n';
    }

    std::string path_;
};

}  // namespace platypus::bench

#endif  // __linux__
