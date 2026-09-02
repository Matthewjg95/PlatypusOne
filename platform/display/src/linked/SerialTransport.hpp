// PlatypusOS — presentation-link transport over a POSIX serial device
// (USB CDC first: /dev/ttyACM0 per docs/protocols/presentation.md §3).
//
// Linux-only: excluded from non-UNIX builds by CMake. Raw byte pipe — the
// link's framing does all message work.
#pragma once

#include <platypus/hal/link/Transport.hpp>

#include <string>

namespace platypus::linked {

class SerialTransport final : public hal::link::ITransport {
   public:
    explicit SerialTransport(std::string devicePath);
    ~SerialTransport() override;

    SerialTransport(const SerialTransport&) = delete;
    SerialTransport& operator=(const SerialTransport&) = delete;

    /// Opens and configures the device (raw mode, 115200-8N1; USB CDC ignores
    /// the baud but setting it keeps real UARTs honest).
    hal::Status open();
    [[nodiscard]] bool isOpen() const noexcept { return fd_ >= 0; }

    hal::Status write(std::span<const std::byte> data) override;
    hal::Result<std::size_t> read(std::span<std::byte> buffer,
                                  std::chrono::milliseconds timeout) override;

   private:
    std::string devicePath_;
    int fd_ = -1;
};

}  // namespace platypus::linked
