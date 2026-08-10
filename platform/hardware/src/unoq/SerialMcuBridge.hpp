// PlatypusOS — IMcuBridge over a serial character device (POSIX).
//
// On the UNO Q the Linux MPU reaches the STM32U585 through an RPMsg/serial
// endpoint; this driver speaks the framing protocol from
// platypus/hal/mcu/Framing.hpp over any tty-like device. Linux-only: the
// target is excluded from Windows host builds by CMake.
#pragma once

#include <platypus/hal/IMcuBridge.hpp>
#include <platypus/hal/mcu/Framing.hpp>

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace platypus::unoq {

class SerialMcuBridge final : public hal::IMcuBridge {
public:
    /// devicePath e.g. "/dev/ttyRPMSG0". Baud only applies to real UARTs.
    explicit SerialMcuBridge(std::string devicePath, unsigned baud = 115200);
    ~SerialMcuBridge() override;

    SerialMcuBridge(const SerialMcuBridge&) = delete;
    SerialMcuBridge& operator=(const SerialMcuBridge&) = delete;

    hal::Status open();
    void close();

    hal::Status pinMode(std::uint8_t pin, hal::PinMode mode) override;
    hal::Status digitalWrite(std::uint8_t pin, bool level) override;
    hal::Result<bool> digitalRead(std::uint8_t pin) override;
    hal::Result<std::uint16_t> analogRead(std::uint8_t pin) override;
    hal::Status pwmWrite(std::uint8_t pin, float duty) override;

    hal::Status publish(std::uint16_t topic, std::span<const std::byte> payload) override;
    hal::Status subscribe(std::uint16_t topic,
                          std::function<void(std::span<const std::byte>)> handler) override;

    hal::Status ping(std::chrono::milliseconds timeout) override;

private:
    hal::Status send(std::uint16_t topic, std::span<const std::byte> payload);
    /// Sends a request and blocks (bounded) for the paired reply topic.
    hal::Result<std::vector<std::byte>> request(std::uint16_t requestTopic,
                                                std::span<const std::byte> payload,
                                                std::uint16_t replyTopic,
                                                std::chrono::milliseconds timeout);
    void readLoop();

    std::string devicePath_;
    unsigned baud_;
    int fd_ = -1;

    std::thread reader_;
    std::atomic<bool> running_{false};

    std::mutex mutex_;                       ///< guards subscribers_ + pending replies
    std::condition_variable replyCv_;
    std::map<std::uint16_t, std::function<void(std::span<const std::byte>)>> subscribers_;
    std::map<std::uint16_t, std::vector<std::byte>> replies_;

    std::mutex writeMutex_;                  ///< serializes frame writes
};

}  // namespace platypus::unoq
