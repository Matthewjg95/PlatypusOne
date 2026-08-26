// PlatypusOS HAL — presentation-link wire framing (portable, header-only).
//
// Shared by LinkedDisplay, display-client firmware, and host-side tests.
// Full protocol: docs/protocols/presentation.md
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace platypus::hal::link {

inline constexpr std::byte kSync{0xB5};
inline constexpr std::size_t kMaxPayload = 65'536;
inline constexpr std::size_t kHeaderSize = 8;  // sync + len + topic + flags
inline constexpr std::uint8_t kReplyExpected = 0x01;
inline constexpr std::uint8_t kAllowedFlags = kReplyExpected;

/// CRC-16/CCITT-FALSE: polynomial 0x1021, init 0xFFFF, no reflection.
[[nodiscard]] constexpr std::uint16_t crc16(std::span<const std::byte> data,
                                            std::uint16_t crc = 0xFFFF) noexcept {
    for (const std::byte byte : data) {
        crc ^= static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(byte)) << 8;
        for (int bit = 0; bit < 8; ++bit) {
            crc = static_cast<std::uint16_t>((crc & 0x8000) != 0 ? (crc << 1) ^ 0x1021 : crc << 1);
        }
    }
    return crc;
}

struct Message {
    std::uint16_t topic = 0;
    std::uint8_t flags = 0;
    std::vector<std::byte> payload;
};

/// Serializes one message. Invalid reserved flags and oversized payloads are
/// rejected before a transport can put a malformed frame on the wire.
[[nodiscard]] inline std::optional<std::vector<std::byte>> encode(
    std::uint16_t topic, std::span<const std::byte> payload, std::uint8_t flags = 0) {
    if (payload.size() > kMaxPayload || (flags & ~kAllowedFlags) != 0) return std::nullopt;

    std::vector<std::byte> frame;
    frame.reserve(kHeaderSize + payload.size() + 2);
    frame.push_back(kSync);
    const auto length = static_cast<std::uint32_t>(payload.size());
    for (int shift = 0; shift < 32; shift += 8) {
        frame.push_back(std::byte{static_cast<std::uint8_t>(length >> shift)});
    }
    frame.push_back(std::byte{static_cast<std::uint8_t>(topic)});
    frame.push_back(std::byte{static_cast<std::uint8_t>(topic >> 8)});
    frame.push_back(std::byte{flags});
    frame.insert(frame.end(), payload.begin(), payload.end());

    const auto checksum = crc16({frame.data() + 1, frame.size() - 1});
    frame.push_back(std::byte{static_cast<std::uint8_t>(checksum)});
    frame.push_back(std::byte{static_cast<std::uint8_t>(checksum >> 8)});
    return frame;
}

/// Incremental parser for a reliable ordered byte stream. Corrupt or oversized
/// frames are dropped; scanning resumes at the next sync byte.
class Decoder {
   public:
    [[nodiscard]] std::optional<Message> feed(std::byte byte) {
        if (!inFrame_) {
            if (byte == kSync) {
                inFrame_ = true;
                buffer_.clear();
                expectedSize_.reset();
            }
            return std::nullopt;
        }

        buffer_.push_back(byte);
        if (!expectedSize_ && buffer_.size() == kHeaderSize - 1) {
            const auto length = readLength();
            if (length > kMaxPayload || (flags() & ~kAllowedFlags) != 0) {
                reset();
                return std::nullopt;
            }
            expectedSize_ = kHeaderSize - 1 + length + 2;
        }

        if (!expectedSize_ || buffer_.size() < *expectedSize_) return std::nullopt;

        const auto contentSize = buffer_.size() - 2;
        const auto received = static_cast<std::uint16_t>(
            std::to_integer<std::uint8_t>(buffer_[contentSize]) |
            (std::to_integer<std::uint16_t>(buffer_[contentSize + 1]) << 8));
        if (received != crc16({buffer_.data(), contentSize})) {
            reset();
            return std::nullopt;
        }

        Message message;
        message.topic =
            static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(buffer_[4]) |
                                       (std::to_integer<std::uint16_t>(buffer_[5]) << 8));
        message.flags = flags();
        message.payload.assign(buffer_.begin() + (kHeaderSize - 1),
                               buffer_.begin() + static_cast<std::ptrdiff_t>(contentSize));
        reset();
        return message;
    }

   private:
    [[nodiscard]] std::size_t readLength() const noexcept {
        std::uint32_t length = 0;
        for (int index = 0; index < 4; ++index) {
            length |= std::to_integer<std::uint32_t>(buffer_[index]) << (index * 8);
        }
        return length;
    }

    [[nodiscard]] std::uint8_t flags() const noexcept {
        return std::to_integer<std::uint8_t>(buffer_[6]);
    }

    void reset() {
        inFrame_ = false;
        buffer_.clear();
        expectedSize_.reset();
    }

    bool inFrame_ = false;
    std::vector<std::byte> buffer_;
    std::optional<std::size_t> expectedSize_;
};

namespace topics {
inline constexpr std::uint16_t kHello = 0x0001;
inline constexpr std::uint16_t kHelloReply = 0x0002;
inline constexpr std::uint16_t kPing = 0x0003;
inline constexpr std::uint16_t kPong = 0x0004;
inline constexpr std::uint16_t kBye = 0x0005;
inline constexpr std::uint16_t kFrameBegin = 0x0010;
inline constexpr std::uint16_t kTile = 0x0011;
inline constexpr std::uint16_t kFrameEnd = 0x0012;
inline constexpr std::uint16_t kFrameAck = 0x0013;
inline constexpr std::uint16_t kBacklight = 0x0014;
inline constexpr std::uint16_t kTouch = 0x0030;
inline constexpr std::uint16_t kButton = 0x0031;
inline constexpr std::uint16_t kEncoder = 0x0032;
inline constexpr std::uint16_t kUserBase = 0x4000;
}  // namespace topics

}  // namespace platypus::hal::link
