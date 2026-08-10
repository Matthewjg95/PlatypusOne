// PlatypusOS HAL — MCU bridge wire framing (portable, header-only).
//
// The byte-level protocol between the Linux MPU and the STM32U585. Kept free
// of OS dependencies so the exact same code compiles into the MCU firmware,
// the Linux bridge driver, and host-side unit tests.
//
// Frame layout (little-endian):
//   [0xA5 sync] [len u16 = payload bytes] [topic u16] [payload...] [crc8]
// crc8 (poly 0x07, init 0x00) covers len, topic and payload.
// Full spec: docs/protocols/mcu-bridge.md
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace platypus::hal::mcu {

inline constexpr std::byte kSync{0xA5};
inline constexpr std::size_t kMaxPayload = 1024;
inline constexpr std::size_t kHeaderSize = 5;  ///< sync + len + topic

/// CRC-8/ATM: polynomial 0x07, init 0x00, no reflection.
[[nodiscard]] constexpr std::uint8_t crc8(std::span<const std::byte> data,
                                          std::uint8_t crc = 0) noexcept {
    for (const std::byte b : data) {
        crc ^= std::to_integer<std::uint8_t>(b);
        for (int i = 0; i < 8; ++i)
            crc = static_cast<std::uint8_t>((crc & 0x80) ? (crc << 1) ^ 0x07 : crc << 1);
    }
    return crc;
}

struct Message {
    std::uint16_t topic = 0;
    std::vector<std::byte> payload;
};

/// Serializes one message into a wire frame.
[[nodiscard]] inline std::vector<std::byte> encode(std::uint16_t topic,
                                                   std::span<const std::byte> payload) {
    std::vector<std::byte> frame;
    frame.reserve(kHeaderSize + payload.size() + 1);
    frame.push_back(kSync);
    const auto len = static_cast<std::uint16_t>(payload.size());
    frame.push_back(std::byte{static_cast<std::uint8_t>(len & 0xFF)});
    frame.push_back(std::byte{static_cast<std::uint8_t>(len >> 8)});
    frame.push_back(std::byte{static_cast<std::uint8_t>(topic & 0xFF)});
    frame.push_back(std::byte{static_cast<std::uint8_t>(topic >> 8)});
    frame.insert(frame.end(), payload.begin(), payload.end());
    frame.push_back(std::byte{crc8({frame.data() + 1, frame.size() - 1})});
    return frame;
}

/// Incremental frame parser. Feed bytes as they arrive from the transport;
/// returns a complete message when one is fully received and CRC-valid.
/// Corrupt frames are dropped and the decoder resyncs on the next 0xA5.
class Decoder {
public:
    std::optional<Message> feed(std::byte b) {
        switch (state_) {
            case State::Sync:
                if (b == kSync) { buffer_.clear(); state_ = State::LenLo; }
                return std::nullopt;
            case State::LenLo:
                buffer_.push_back(b);
                len_ = std::to_integer<std::uint16_t>(b);
                state_ = State::LenHi;
                return std::nullopt;
            case State::LenHi:
                buffer_.push_back(b);
                len_ = static_cast<std::uint16_t>(
                    len_ | (std::to_integer<std::uint16_t>(b) << 8));
                if (len_ > kMaxPayload) { state_ = State::Sync; return std::nullopt; }
                state_ = State::TopicLo;
                return std::nullopt;
            case State::TopicLo:
                buffer_.push_back(b);
                topic_ = std::to_integer<std::uint16_t>(b);
                state_ = State::TopicHi;
                return std::nullopt;
            case State::TopicHi:
                buffer_.push_back(b);
                topic_ = static_cast<std::uint16_t>(
                    topic_ | (std::to_integer<std::uint16_t>(b) << 8));
                state_ = len_ == 0 ? State::Crc : State::Payload;
                return std::nullopt;
            case State::Payload:
                buffer_.push_back(b);
                if (buffer_.size() == kHeaderSize - 1 + len_) state_ = State::Crc;
                return std::nullopt;
            case State::Crc: {
                state_ = State::Sync;
                if (std::to_integer<std::uint8_t>(b) != crc8(buffer_)) return std::nullopt;
                Message msg;
                msg.topic = topic_;
                msg.payload.assign(buffer_.begin() + (kHeaderSize - 1), buffer_.end());
                return msg;
            }
        }
        return std::nullopt;
    }

private:
    enum class State { Sync, LenLo, LenHi, TopicLo, TopicHi, Payload, Crc };
    State state_ = State::Sync;
    std::vector<std::byte> buffer_;
    std::uint16_t len_ = 0;
    std::uint16_t topic_ = 0;
};

/// Reserved topic ids (full allocation table in docs/protocols/mcu-bridge.md).
namespace topics {
inline constexpr std::uint16_t kPing = 0x0001;
inline constexpr std::uint16_t kPong = 0x0002;
inline constexpr std::uint16_t kGpioSet = 0x0010;
inline constexpr std::uint16_t kGpioRead = 0x0011;
inline constexpr std::uint16_t kGpioReadReply = 0x0012;
inline constexpr std::uint16_t kAnalogRead = 0x0020;
inline constexpr std::uint16_t kAnalogReadReply = 0x0021;
inline constexpr std::uint16_t kPwmWrite = 0x0030;
inline constexpr std::uint16_t kPinMode = 0x0040;
inline constexpr std::uint16_t kUserBase = 0x4000;  ///< app/plugin topics start here
}  // namespace topics

}  // namespace platypus::hal::mcu
