// LinkedDisplay contract tests: the presentation-link session, tiling, flow
// control, and input paths proven against a scripted in-memory client — the
// "host-side loopback fake" the protocol's open-items list requires. No
// hardware, no serial device; the shared Framing codec is the wire.
#include "linked/LinkedDisplay.hpp"

#include <cassert>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <vector>

namespace {

using namespace platypus;
namespace link = hal::link;

/// In-memory duplex transport with a scripted client brain: every frame the
/// host writes is decoded synchronously and handed to onHostFrame, which may
/// queue reply bytes for the host's receive side.
class FakeTransport final : public link::ITransport {
   public:
    std::function<void(const link::Message&)> onHostFrame;

    hal::Status write(std::span<const std::byte> data) override {
        std::vector<link::Message> decoded;
        {
            const std::scoped_lock lock(mutex_);
            for (const auto b : data)
                if (auto message = clientDecoder_.feed(b)) decoded.push_back(std::move(*message));
        }
        // Dispatch outside the lock: the brain usually calls sendToHost().
        for (auto& message : decoded) {
            hostFrames.push_back(message);
            if (onHostFrame) onHostFrame(message);
        }
        return {};
    }

    hal::Result<std::size_t> read(std::span<std::byte> buffer,
                                  std::chrono::milliseconds timeout) override {
        std::unique_lock lock(mutex_);
        cv_.wait_for(lock, timeout, [&] { return !inbox_.empty(); });
        std::size_t count = 0;
        while (count < buffer.size() && !inbox_.empty()) {
            buffer[count++] = inbox_.front();
            inbox_.pop_front();
        }
        return count;
    }

    void sendToHost(std::uint16_t topic, std::span<const std::byte> payload) {
        const auto frame = link::encode(topic, payload);
        assert(frame);
        const std::scoped_lock lock(mutex_);
        inbox_.insert(inbox_.end(), frame->begin(), frame->end());
        cv_.notify_all();
    }

    std::vector<link::Message> hostFrames;  ///< everything the host sent, in order

   private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::byte> inbox_;
    link::Decoder clientDecoder_;
};

std::uint16_t u16(const std::vector<std::byte>& payload, std::size_t offset) {
    return static_cast<std::uint16_t>(std::to_integer<std::uint16_t>(payload[offset]) |
                                      (std::to_integer<std::uint16_t>(payload[offset + 1]) << 8));
}

std::vector<std::byte> helloReply(std::uint16_t width, std::uint16_t height,
                                  std::uint8_t capabilities, std::uint32_t maxPayload) {
    std::vector<std::byte> reply(28, std::byte{0});
    reply[0] = std::byte{1};  // protoVersion = 1
    reply[2] = std::byte{static_cast<std::uint8_t>(width & 0xFF)};
    reply[3] = std::byte{static_cast<std::uint8_t>(width >> 8)};
    reply[4] = std::byte{static_cast<std::uint8_t>(height & 0xFF)};
    reply[5] = std::byte{static_cast<std::uint8_t>(height >> 8)};
    reply[6] = std::byte{0};  // RGB565
    reply[7] = std::byte{capabilities};
    for (int i = 0; i < 4; ++i)
        reply[8 + static_cast<std::size_t>(i)] =
            std::byte{static_cast<std::uint8_t>(maxPayload >> (8 * i))};
    reply[12] = std::byte{'f'};
    return reply;
}

struct Client {
    FakeTransport* transport = nullptr;
    bool answerHello = true;
    bool sendAcks = true;
    std::uint8_t ackStatus = 0;

    void install(FakeTransport& t, std::uint8_t capabilities = 0x0B,
                 std::uint32_t maxPayload = 1024) {
        transport = &t;
        t.onHostFrame = [this, capabilities, maxPayload](const link::Message& message) {
            if (message.topic == link::topics::kHello && answerHello) {
                const auto reply = helloReply(64, 32, capabilities, maxPayload);
                transport->sendToHost(link::topics::kHelloReply, reply);
            } else if (message.topic == link::topics::kFrameEnd && sendAcks) {
                std::vector<std::byte> ack{message.payload[0], message.payload[1],
                                           std::byte{ackStatus}};
                transport->sendToHost(link::topics::kFrameAck, ack);
            }
        };
    }
};

linked::LinkedDisplayConfig testConfig() {
    linked::LinkedDisplayConfig config;
    config.helloTimeout = std::chrono::milliseconds(250);
    config.ackTimeout = std::chrono::milliseconds(100);
    return config;
}

std::vector<std::byte> checkerFrame(std::uint16_t width, std::uint16_t height) {
    std::vector<std::byte> pixels(static_cast<std::size_t>(width) * height * 2);
    for (std::size_t i = 0; i < pixels.size(); ++i)
        pixels[i] = std::byte{static_cast<std::uint8_t>(i * 31)};
    return pixels;
}

void test_session_and_info() {
    auto transport = std::make_unique<FakeTransport>();
    Client client;
    client.install(*transport);
    linked::LinkedDisplay display(std::move(transport), testConfig());
    assert(display.connect().ok());
    assert(display.connected());
    assert(display.info().width == 64 && display.info().height == 32);
    assert(display.info().bitsPerPixel == 16);
}

void test_connect_timeout_and_bad_reply() {
    {
        auto transport = std::make_unique<FakeTransport>();
        Client client;
        client.install(*transport);
        client.answerHello = false;
        linked::LinkedDisplay display(std::move(transport), testConfig());
        assert(display.connect().error() == hal::Error::Timeout);
    }
    {
        auto transport = std::make_unique<FakeTransport>();
        auto* raw = transport.get();
        raw->onHostFrame = [raw](const link::Message& message) {
            if (message.topic != link::topics::kHello) return;
            auto reply = helloReply(64, 32, 0x03, 1024);
            reply[6] = std::byte{9};  // unknown pixel format
            raw->sendToHost(link::topics::kHelloReply, reply);
        };
        linked::LinkedDisplay display(std::move(transport), testConfig());
        assert(display.connect().error() == hal::Error::NotSupported);
    }
}

void test_tiling_and_pixels() {
    auto transport = std::make_unique<FakeTransport>();
    auto* raw = transport.get();
    Client client;
    client.install(*transport);  // maxPayload 1024
    linked::LinkedDisplay display(std::move(transport), testConfig());
    assert(display.connect().ok());

    const auto pixels = checkerFrame(64, 32);
    // Region 48 wide → rowBytes 96; rowsPerTile = (1024-12)/96 = 10; 20 rows → 2 tiles.
    assert(display.presentRegion(pixels, {8, 4, 48, 20}).ok());

    std::vector<link::Message> frames;
    for (const auto& message : raw->hostFrames)
        if (message.topic == link::topics::kFrameBegin || message.topic == link::topics::kTile ||
            message.topic == link::topics::kFrameEnd)
            frames.push_back(message);
    assert(frames.size() == 4);  // begin, 2 tiles, end
    assert(frames[0].topic == link::topics::kFrameBegin);
    assert(u16(frames[0].payload, 2) == 2);  // tileCount

    const auto& tile = frames[1];
    assert(u16(tile.payload, 2) == 8);                             // x
    assert(u16(tile.payload, 4) == 4);                             // y
    assert(u16(tile.payload, 6) == 48);                            // w
    assert(u16(tile.payload, 8) == 10);                            // rows
    assert(std::to_integer<std::uint8_t>(tile.payload[10]) == 0);  // raw encoding
    assert(tile.payload.size() == 12 + 48u * 10u * 2u);
    // First row of the tile equals framebuffer row 4, columns 8..56.
    const std::size_t sourceOffset = 4u * 64u * 2u + 8u * 2u;
    for (std::size_t i = 0; i < 96; ++i)
        assert(tile.payload[12 + i] == pixels[sourceOffset + i]);

    const auto& second = frames[2];
    assert(u16(second.payload, 4) == 14);  // y advanced by rowsPerTile
    assert(u16(second.payload, 8) == 10);  // remaining rows

    // Full-frame present() also succeeds (64 rows*... 32 rows → 4 tiles at 10 rows/tile?
    // rowBytes 128 → rowsPerTile 7; ceil(32/7)=5 tiles — just assert it works).
    assert(display.present(pixels).ok());
}

void test_ack_failures() {
    {
        auto transport = std::make_unique<FakeTransport>();
        Client client;
        client.install(*transport);
        client.sendAcks = false;
        linked::LinkedDisplay display(std::move(transport), testConfig());
        assert(display.connect().ok());
        const auto pixels = checkerFrame(64, 32);
        assert(display.present(pixels).error() == hal::Error::Timeout);
    }
    {
        auto transport = std::make_unique<FakeTransport>();
        Client client;
        client.install(*transport);
        client.ackStatus = 1;  // client dropped the frame
        linked::LinkedDisplay display(std::move(transport), testConfig());
        assert(display.connect().ok());
        const auto pixels = checkerFrame(64, 32);
        assert(display.present(pixels).error() == hal::Error::IoFailure);
    }
}

void test_input_events() {
    auto transport = std::make_unique<FakeTransport>();
    auto* raw = transport.get();
    Client client;
    client.install(*transport);
    linked::LinkedDisplay display(std::move(transport), testConfig());
    assert(display.connect().ok());

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<hal::TouchEvent> touches;
    std::vector<hal::ButtonEvent> buttons;
    assert(display
               .onTouch([&](const hal::TouchEvent& e) {
                   const std::scoped_lock lock(mutex);
                   touches.push_back(e);
                   cv.notify_all();
               })
               .ok());
    assert(display
               .onButton([&](const hal::ButtonEvent& e) {
                   const std::scoped_lock lock(mutex);
                   buttons.push_back(e);
                   cv.notify_all();
               })
               .ok());

    raw->sendToHost(link::topics::kTouch,
                    std::vector<std::byte>{std::byte{0}, std::byte{10}, std::byte{0}, std::byte{20},
                                           std::byte{0}});
    raw->sendToHost(link::topics::kButton, std::vector<std::byte>{std::byte{3}, std::byte{1}});
    // Encoder: delta -2 → two counter-clockwise synthetic presses.
    raw->sendToHost(link::topics::kEncoder,
                    std::vector<std::byte>{std::byte{0xFE}, std::byte{0xFF}, std::byte{0}});

    std::unique_lock lock(mutex);
    const bool delivered = cv.wait_for(lock, std::chrono::seconds(2),
                                       [&] { return touches.size() == 1 && buttons.size() == 3; });
    assert(delivered);
    assert(touches[0].type == hal::TouchEvent::Type::Down);
    assert(touches[0].x == 10 && touches[0].y == 20);
    assert(buttons[0].id == 3 && buttons[0].pressed);
    assert(buttons[1].id == linked::kEncoderCounterClockwiseButton);
    assert(buttons[2].id == linked::kEncoderCounterClockwiseButton);
}

void test_backlight_capability() {
    {
        auto transport = std::make_unique<FakeTransport>();
        Client client;
        client.install(*transport, 0x0B);  // touch|buttons|backlight
        linked::LinkedDisplay display(std::move(transport), testConfig());
        assert(display.connect().ok());
        assert(display.setBacklight(0.5f).ok());
    }
    {
        auto transport = std::make_unique<FakeTransport>();
        Client client;
        client.install(*transport, 0x03);  // no backlight bit
        linked::LinkedDisplay display(std::move(transport), testConfig());
        assert(display.connect().ok());
        assert(display.setBacklight(0.5f).error() == hal::Error::NotSupported);
    }
}

}  // namespace

void test_linked_display() {
    test_session_and_info();
    test_connect_timeout_and_bad_reply();
    test_tiling_and_pixels();
    test_ack_failures();
    test_input_events();
    test_backlight_capability();
    std::puts("test_linked_display: OK");
}
