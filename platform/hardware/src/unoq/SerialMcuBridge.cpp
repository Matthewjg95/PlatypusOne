#include "SerialMcuBridge.hpp"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace platypus::unoq {

using hal::Error;
using hal::Result;
using hal::Status;
namespace mcu = hal::mcu;

SerialMcuBridge::SerialMcuBridge(std::string devicePath, unsigned baud)
    : devicePath_(std::move(devicePath)), baud_(baud) {}

SerialMcuBridge::~SerialMcuBridge() { close(); }

Status SerialMcuBridge::open() {
    if (fd_ >= 0) return {};

    fd_ = ::open(devicePath_.c_str(), O_RDWR | O_NOCTTY);
    if (fd_ < 0) return Error::IoFailure;

    termios tio{};
    if (::tcgetattr(fd_, &tio) == 0) {
        cfmakeraw(&tio);
        const speed_t speed = baud_ == 115200 ? B115200 : B115200;  // TODO: full baud table
        cfsetispeed(&tio, speed);
        cfsetospeed(&tio, speed);
        tio.c_cc[VMIN] = 1;
        tio.c_cc[VTIME] = 1;
        ::tcsetattr(fd_, TCSANOW, &tio);  // best effort; RPMsg endpoints reject this
    }

    running_ = true;
    reader_ = std::thread(&SerialMcuBridge::readLoop, this);
    return {};
}

void SerialMcuBridge::close() {
    running_ = false;
    if (fd_ >= 0) {
        ::close(fd_);  // unblocks the reader's read()
        fd_ = -1;
    }
    if (reader_.joinable()) reader_.join();
}

void SerialMcuBridge::readLoop() {
    mcu::Decoder decoder;
    std::byte buf[256];
    while (running_) {
        const ssize_t n = ::read(fd_, buf, sizeof(buf));
        if (n <= 0) {
            if (errno == EINTR) continue;
            break;  // device gone or closed
        }
        for (ssize_t i = 0; i < n; ++i) {
            auto msg = decoder.feed(buf[i]);
            if (!msg) continue;

            std::function<void(std::span<const std::byte>)> handler;
            {
                std::lock_guard lock(mutex_);
                replies_[msg->topic] = msg->payload;
                if (const auto it = subscribers_.find(msg->topic); it != subscribers_.end())
                    handler = it->second;
            }
            replyCv_.notify_all();
            if (handler) handler(msg->payload);  // driver thread; handlers must be brief
        }
    }
}

Status SerialMcuBridge::send(std::uint16_t topic, std::span<const std::byte> payload) {
    if (fd_ < 0) return Error::NotInitialized;
    if (payload.size() > mcu::kMaxPayload) return Error::InvalidArgument;

    const auto frame = mcu::encode(topic, payload);
    std::lock_guard lock(writeMutex_);
    std::size_t written = 0;
    while (written < frame.size()) {
        const ssize_t n = ::write(fd_, frame.data() + written, frame.size() - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            return Error::IoFailure;
        }
        written += static_cast<std::size_t>(n);
    }
    return {};
}

Result<std::vector<std::byte>> SerialMcuBridge::request(
    std::uint16_t requestTopic, std::span<const std::byte> payload,
    std::uint16_t replyTopic, std::chrono::milliseconds timeout) {
    {
        std::lock_guard lock(mutex_);
        replies_.erase(replyTopic);
    }
    if (const auto s = send(requestTopic, payload); !s) return s.error();

    std::unique_lock lock(mutex_);
    const bool got = replyCv_.wait_for(lock, timeout, [&] {
        return replies_.count(replyTopic) != 0;
    });
    if (!got) return Error::Timeout;
    auto payload_out = std::move(replies_[replyTopic]);
    replies_.erase(replyTopic);
    return payload_out;
}

Status SerialMcuBridge::pinMode(std::uint8_t pin, hal::PinMode mode) {
    const std::byte msg[] = {std::byte{pin}, std::byte{static_cast<std::uint8_t>(mode)}};
    return send(mcu::topics::kPinMode, msg);
}

Status SerialMcuBridge::digitalWrite(std::uint8_t pin, bool level) {
    const std::byte msg[] = {std::byte{pin}, std::byte{level ? 1u : 0u}};
    return send(mcu::topics::kGpioSet, msg);
}

Result<bool> SerialMcuBridge::digitalRead(std::uint8_t pin) {
    const std::byte msg[] = {std::byte{pin}};
    auto reply = request(mcu::topics::kGpioRead, msg, mcu::topics::kGpioReadReply,
                         std::chrono::milliseconds(100));
    if (!reply) return reply.error();
    if (reply.value().size() != 2) return Error::IoFailure;
    return std::to_integer<std::uint8_t>(reply.value()[1]) != 0;
}

Result<std::uint16_t> SerialMcuBridge::analogRead(std::uint8_t pin) {
    const std::byte msg[] = {std::byte{pin}};
    auto reply = request(mcu::topics::kAnalogRead, msg, mcu::topics::kAnalogReadReply,
                         std::chrono::milliseconds(100));
    if (!reply) return reply.error();
    if (reply.value().size() != 3) return Error::IoFailure;
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint16_t>(reply.value()[1]) |
        (std::to_integer<std::uint16_t>(reply.value()[2]) << 8));
}

Status SerialMcuBridge::pwmWrite(std::uint8_t pin, float duty) {
    if (duty < 0.0f || duty > 1.0f) return Error::InvalidArgument;
    const auto scaled = static_cast<std::uint16_t>(duty * 0xFFFF);
    const std::byte msg[] = {std::byte{pin},
                             std::byte{static_cast<std::uint8_t>(scaled & 0xFF)},
                             std::byte{static_cast<std::uint8_t>(scaled >> 8)}};
    return send(mcu::topics::kPwmWrite, msg);
}

Status SerialMcuBridge::publish(std::uint16_t topic, std::span<const std::byte> payload) {
    if (topic < mcu::topics::kUserBase) return Error::InvalidArgument;
    return send(topic, payload);
}

Status SerialMcuBridge::subscribe(std::uint16_t topic,
                                  std::function<void(std::span<const std::byte>)> handler) {
    std::lock_guard lock(mutex_);
    subscribers_[topic] = std::move(handler);
    return {};
}

Status SerialMcuBridge::ping(std::chrono::milliseconds timeout) {
    auto reply = request(mcu::topics::kPing, {}, mcu::topics::kPong, timeout);
    return reply ? Status{} : Status{reply.error()};
}

}  // namespace platypus::unoq
