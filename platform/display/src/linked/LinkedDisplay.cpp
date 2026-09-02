#include "LinkedDisplay.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace platypus::linked {

namespace {

using hal::Error;
namespace link = hal::link;

constexpr std::uint16_t kProtocolVersion = 1;
constexpr std::uint8_t kCapabilityBacklight = 1u << 3;
constexpr std::size_t kHelloReplySize = 28;
constexpr std::size_t kTileHeaderSize = 12;

void putU16(std::vector<std::byte>& out, std::uint16_t value) {
    out.push_back(std::byte{static_cast<std::uint8_t>(value & 0xFF)});
    out.push_back(std::byte{static_cast<std::uint8_t>(value >> 8)});
}

std::uint16_t getU16(std::span<const std::byte> data, std::size_t offset) {
    return static_cast<std::uint16_t>(std::to_integer<std::uint16_t>(data[offset]) |
                                      (std::to_integer<std::uint16_t>(data[offset + 1]) << 8));
}

std::uint32_t getU32(std::span<const std::byte> data, std::size_t offset) {
    return static_cast<std::uint32_t>(std::to_integer<std::uint32_t>(data[offset]) |
                                      (std::to_integer<std::uint32_t>(data[offset + 1]) << 8) |
                                      (std::to_integer<std::uint32_t>(data[offset + 2]) << 16) |
                                      (std::to_integer<std::uint32_t>(data[offset + 3]) << 24));
}

}  // namespace

LinkedDisplay::LinkedDisplay(std::unique_ptr<link::ITransport> transport,
                             LinkedDisplayConfig config)
    : transport_(std::move(transport)), config_(std::move(config)) {}

LinkedDisplay::~LinkedDisplay() {
    if (running_.load()) {
        (void)sendFrame(link::topics::kBye, {});  // orderly shutdown, best effort
        running_.store(false);
    }
    if (receiveThread_.joinable()) receiveThread_.join();
}

hal::Status LinkedDisplay::sendFrame(std::uint16_t topic, std::span<const std::byte> payload) {
    const auto frame = link::encode(topic, payload);
    if (!frame) return Error::InvalidArgument;
    const std::scoped_lock lock(writeMutex_);
    return transport_->write(*frame);
}

hal::Status LinkedDisplay::connect() {
    if (running_.load()) return Error::Busy;

    // Hello: protoVersion u16 + hostName[16], NUL-padded.
    std::vector<std::byte> hello;
    putU16(hello, kProtocolVersion);
    std::array<char, 16> name{};  // zero-filled: NUL padding comes for free
    for (std::size_t i = 0; i + 1 < name.size() && i < config_.hostName.size(); ++i)
        name[i] = config_.hostName[i];
    for (const char c : name)
        hello.push_back(std::byte{static_cast<std::uint8_t>(c)});
    if (const auto status = sendFrame(link::topics::kHello, hello); !status) return status;

    // Pump reads inline until HelloReply or deadline; the receive thread only
    // starts on an established session.
    link::Decoder decoder;
    std::array<std::byte, 512> buffer;
    const auto deadline = std::chrono::steady_clock::now() + config_.helloTimeout;
    while (std::chrono::steady_clock::now() < deadline) {
        auto got = transport_->read(buffer, std::chrono::milliseconds(20));
        if (!got) return got.error();
        for (std::size_t i = 0; i < got.value(); ++i) {
            auto message = decoder.feed(buffer[i]);
            if (!message || message->topic != link::topics::kHelloReply) continue;
            if (message->payload.size() < kHelloReplySize) return Error::IoFailure;

            const std::span<const std::byte> reply(message->payload);
            const auto clientVersion = getU16(reply, 0);
            if (std::min(clientVersion, kProtocolVersion) < 1) return Error::NotSupported;
            const auto pixelFormat = std::to_integer<std::uint8_t>(reply[6]);
            if (pixelFormat != 0) return Error::NotSupported;  // v0.1: RGB565 only

            info_ = hal::DisplayInfo{getU16(reply, 2), getU16(reply, 4), 16};
            capabilities_ = std::to_integer<std::uint8_t>(reply[7]);
            maxPayload_ = std::min<std::uint32_t>(getU32(reply, 8),
                                                  static_cast<std::uint32_t>(link::kMaxPayload));
            if (info_.width == 0 || info_.height == 0 ||
                maxPayload_ <= kTileHeaderSize + info_.width * 2u)
                return Error::IoFailure;  // cannot fit even one full-width row

            running_.store(true);
            receiveThread_ = std::thread([this] { receiveLoop(); });
            return {};
        }
    }
    return Error::Timeout;
}

hal::DisplayInfo LinkedDisplay::info() const noexcept {
    return info_;
}

hal::Status LinkedDisplay::setBacklight(float brightness) {
    if (!running_.load()) return Error::NotInitialized;
    if ((capabilities_ & kCapabilityBacklight) == 0) return Error::NotSupported;
    const auto clamped = std::clamp(brightness, 0.0f, 1.0f);
    std::vector<std::byte> payload;
    putU16(payload, static_cast<std::uint16_t>(clamped * 0xFFFF));
    return sendFrame(link::topics::kBacklight, payload);
}

hal::Status LinkedDisplay::present(std::span<const std::byte> pixels) {
    return presentRegion(pixels, {0, 0, info_.width, info_.height});
}

hal::Status LinkedDisplay::presentRegion(std::span<const std::byte> pixels,
                                         const hal::DisplayRegion& region) {
    if (!running_.load()) return Error::NotInitialized;
    const std::size_t stride = static_cast<std::size_t>(info_.width) * 2;
    if (pixels.size() != stride * info_.height) return Error::InvalidArgument;
    if (region.width == 0 || region.height == 0 || region.x + region.width > info_.width ||
        region.y + region.height > info_.height)
        return Error::InvalidArgument;

    // Raw-encoding tiles, split by rows so each payload fits the client's
    // maxPayload (spec §6: never assume one tile per update).
    const std::size_t rowBytes = static_cast<std::size_t>(region.width) * 2;
    const auto rowsPerTile = static_cast<std::uint16_t>(
        std::clamp<std::size_t>((maxPayload_ - kTileHeaderSize) / rowBytes, 1, region.height));
    const auto tileCount =
        static_cast<std::uint16_t>((region.height + rowsPerTile - 1) / rowsPerTile);

    const std::uint16_t frameId = nextFrameId_++;
    if (nextFrameId_ == 0) nextFrameId_ = 1;
    {
        const std::scoped_lock lock(ackMutex_);
        lastAck_.reset();
    }

    std::vector<std::byte> begin;
    putU16(begin, frameId);
    putU16(begin, tileCount);
    if (const auto status = sendFrame(link::topics::kFrameBegin, begin); !status) return status;

    std::vector<std::byte> tile;
    for (std::uint16_t row = 0; row < region.height; row += rowsPerTile) {
        const auto rows =
            static_cast<std::uint16_t>(std::min<std::uint32_t>(rowsPerTile, region.height - row));
        tile.clear();
        putU16(tile, frameId);
        putU16(tile, region.x);
        putU16(tile, static_cast<std::uint16_t>(region.y + row));
        putU16(tile, region.width);
        putU16(tile, rows);
        tile.push_back(std::byte{0});  // encoding 0 = raw RGB565
        tile.push_back(std::byte{0});  // reserved
        for (std::uint16_t r = 0; r < rows; ++r) {
            const std::size_t offset = (static_cast<std::size_t>(region.y) + row + r) * stride +
                                       static_cast<std::size_t>(region.x) * 2;
            const auto* rowStart = pixels.data() + offset;
            tile.insert(tile.end(), rowStart, rowStart + rowBytes);
        }
        if (const auto status = sendFrame(link::topics::kTile, tile); !status) return status;
    }

    std::vector<std::byte> end;
    putU16(end, frameId);
    if (const auto status = sendFrame(link::topics::kFrameEnd, end); !status) return status;
    return awaitAck(frameId);
}

hal::Status LinkedDisplay::awaitAck(std::uint16_t frameId) {
    std::unique_lock lock(ackMutex_);
    const bool acked = ackCv_.wait_for(lock, config_.ackTimeout,
                                       [&] { return lastAck_ && lastAck_->first == frameId; });
    if (!acked) return Error::Timeout;  // spec §8: drop the frame, stay responsive
    return lastAck_->second == 0 ? hal::Status{} : hal::Status{Error::IoFailure};
}

hal::Status LinkedDisplay::onTouch(std::function<void(const hal::TouchEvent&)> handler) {
    const std::scoped_lock lock(handlerMutex_);
    touchHandler_ = std::move(handler);
    return {};
}

hal::Status LinkedDisplay::onButton(std::function<void(const hal::ButtonEvent&)> handler) {
    const std::scoped_lock lock(handlerMutex_);
    buttonHandler_ = std::move(handler);
    return {};
}

void LinkedDisplay::receiveLoop() {
    link::Decoder decoder;
    std::array<std::byte, 4096> buffer;
    while (running_.load()) {
        auto got = transport_->read(buffer, std::chrono::milliseconds(50));
        if (!got) break;  // dead transport: present() will surface failures
        for (std::size_t i = 0; i < got.value(); ++i)
            if (auto message = decoder.feed(buffer[i])) handleMessage(*message);
    }
}

void LinkedDisplay::handleMessage(const link::Message& message) {
    const std::span<const std::byte> payload(message.payload);
    switch (message.topic) {
        case link::topics::kPing:  // client liveness probe: echo the nonce
            (void)sendFrame(link::topics::kPong, payload);
            break;
        case link::topics::kFrameAck:
            if (payload.size() >= 3) {
                const std::scoped_lock lock(ackMutex_);
                lastAck_ = {getU16(payload, 0), std::to_integer<std::uint8_t>(payload[2])};
                ackCv_.notify_all();
            }
            break;
        case link::topics::kTouch:
            if (payload.size() >= 5) {
                const auto type = std::to_integer<std::uint8_t>(payload[0]);
                if (type > 2) break;
                hal::TouchEvent event{static_cast<hal::TouchEvent::Type>(type), getU16(payload, 1),
                                      getU16(payload, 3)};
                const std::scoped_lock lock(handlerMutex_);
                if (touchHandler_) touchHandler_(event);
            }
            break;
        case link::topics::kButton:
            if (payload.size() >= 2) {
                hal::ButtonEvent event{std::to_integer<std::uint8_t>(payload[0]),
                                       std::to_integer<std::uint8_t>(payload[1]) != 0};
                const std::scoped_lock lock(handlerMutex_);
                if (buttonHandler_) buttonHandler_(event);
            }
            break;
        case link::topics::kEncoder:
            // No HAL encoder event yet (spec §5): translate each detent into a
            // synthetic button press so navigation works at reduced fidelity.
            if (payload.size() >= 3) {
                const auto delta = static_cast<std::int16_t>(getU16(payload, 0));
                const auto count = static_cast<std::uint16_t>(delta < 0 ? -delta : delta);
                const auto id =
                    delta >= 0 ? kEncoderClockwiseButton : kEncoderCounterClockwiseButton;
                const std::scoped_lock lock(handlerMutex_);
                if (buttonHandler_)
                    for (std::uint16_t i = 0; i < count; ++i)
                        buttonHandler_({id, true});
            }
            break;
        default:
            break;  // unknown topics are ignored, never errors (spec §4)
    }
}

}  // namespace platypus::linked
