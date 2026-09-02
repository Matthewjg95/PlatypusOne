// PlatypusOS display client — M5Stack Tab5 (temporary prototype panel).
//
// The dumb half of docs/protocols/presentation.md: receive pixel tiles over
// USB CDC serial, blit them, report touch. No widget tree, no app knowledge —
// swapping this panel for the final integrated display changes nothing above
// the host's HAL (ADR-0001).
//
// Shares the presentation-link codec verbatim:
//   arduino-cli compile --fqbn m5stack:esp32:m5stack_tab5 firmware/tab5_display_client \
//     --build-property "compiler.cpp.extra_flags=-I<repo>/platform/display/include -std=gnu++2a"
// (Exact FQBN from `arduino-cli board listall` after installing the M5Stack
// core; the codec needs C++20 — the ESP32-P4 toolchain provides it.)
#include <M5Unified.h>

#include <platypus/hal/link/Framing.hpp>

namespace link = platypus::hal::link;

namespace {

constexpr std::uint16_t kProtocolVersion = 1;
constexpr std::uint32_t kMaxPayload = 16384;  // bounds per-frame latency on CDC
constexpr std::uint8_t kCapTouch = 1u << 0;
constexpr std::uint8_t kCapBacklight = 1u << 3;

link::Decoder decoder;
std::uint16_t openFrameId = 0;
bool frameOpen = false;

void sendFrame(std::uint16_t topic, const std::uint8_t* payload, std::size_t count) {
    const auto frame =
        link::encode(topic, {reinterpret_cast<const std::byte*>(payload), count});
    if (frame) Serial.write(reinterpret_cast<const std::uint8_t*>(frame->data()), frame->size());
}

void putU16(std::uint8_t* out, std::uint16_t value) {
    out[0] = static_cast<std::uint8_t>(value & 0xFF);
    out[1] = static_cast<std::uint8_t>(value >> 8);
}

std::uint16_t getU16(const std::byte* data) {
    return static_cast<std::uint16_t>(std::to_integer<std::uint16_t>(data[0]) |
                                      (std::to_integer<std::uint16_t>(data[1]) << 8));
}

void sendHelloReply() {
    std::uint8_t reply[28] = {};
    putU16(reply, kProtocolVersion);
    putU16(reply + 2, static_cast<std::uint16_t>(M5.Display.width()));
    putU16(reply + 4, static_cast<std::uint16_t>(M5.Display.height()));
    reply[6] = 0;  // RGB565 little-endian
    reply[7] = kCapTouch | kCapBacklight;
    reply[8] = static_cast<std::uint8_t>(kMaxPayload & 0xFF);
    reply[9] = static_cast<std::uint8_t>((kMaxPayload >> 8) & 0xFF);
    reply[10] = static_cast<std::uint8_t>((kMaxPayload >> 16) & 0xFF);
    reply[11] = static_cast<std::uint8_t>((kMaxPayload >> 24) & 0xFF);
    const char* name = "m5stack-tab5";
    for (std::size_t i = 0; name[i] != '\0'; ++i) reply[12 + i] = name[i];
    sendFrame(link::topics::kHelloReply, reply, sizeof(reply));
}

void sendAck(std::uint16_t frameId, std::uint8_t status) {
    std::uint8_t ack[3];
    putU16(ack, frameId);
    ack[2] = status;
    sendFrame(link::topics::kFrameAck, ack, sizeof(ack));
}

void handleTile(const link::Message& message) {
    if (message.payload.size() < 12) return;
    const auto* p = message.payload.data();
    const auto frameId = getU16(p);
    if (!frameOpen || frameId != openFrameId) return;  // stale tile: discard
    const auto x = getU16(p + 2);
    const auto y = getU16(p + 4);
    const auto w = getU16(p + 6);
    const auto h = getU16(p + 8);
    const auto encoding = std::to_integer<std::uint8_t>(p[10]);
    if (encoding != 0) return;  // raw only in this client (no RLE16 capability)
    if (x + w > M5.Display.width() || y + h > M5.Display.height() ||
        message.payload.size() != 12u + static_cast<std::size_t>(w) * h * 2u) {
        sendAck(frameId, 1);  // protocol error: drop the frame
        frameOpen = false;
        return;
    }
    // RGB565 little-endian wire order; verify byte order on the bench and
    // flip setSwapBytes if colors come out wrong (checklist step 5).
    M5.Display.setSwapBytes(false);
    M5.Display.pushImage(x, y, w, h, reinterpret_cast<const std::uint16_t*>(p + 12));
}

void handleMessage(const link::Message& message) {
    const auto* p = message.payload.data();
    switch (message.topic) {
        case link::topics::kHello:
            sendHelloReply();
            break;
        case link::topics::kPing:
            sendFrame(link::topics::kPong,
                      reinterpret_cast<const std::uint8_t*>(p), message.payload.size());
            break;
        case link::topics::kFrameBegin:
            if (message.payload.size() >= 4) {
                openFrameId = getU16(p);
                frameOpen = true;
            }
            break;
        case link::topics::kTile:
            handleTile(message);
            break;
        case link::topics::kFrameEnd:
            if (frameOpen && message.payload.size() >= 2 && getU16(p) == openFrameId) {
                sendAck(openFrameId, 0);
                frameOpen = false;
            }
            break;
        case link::topics::kBacklight:
            if (message.payload.size() >= 2)
                M5.Display.setBrightness(static_cast<std::uint8_t>(getU16(p) >> 8));
            break;
        default:
            break;  // unknown topics ignored, never errors
    }
}

void pumpTouch() {
    M5.update();
    const auto detail = M5.Touch.getDetail();
    std::uint8_t event[5];
    if (detail.wasPressed()) {
        event[0] = 0;  // Down
    } else if (detail.isPressed() && detail.wasDragged()) {
        event[0] = 1;  // Move
    } else if (detail.wasReleased()) {
        event[0] = 2;  // Up
    } else {
        return;
    }
    putU16(event + 1, static_cast<std::uint16_t>(detail.x));
    putU16(event + 3, static_cast<std::uint16_t>(detail.y));
    sendFrame(link::topics::kTouch, event, sizeof(event));
}

}  // namespace

void setup() {
    auto config = M5.config();
    M5.begin(config);
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.drawString("PlatypusOS link: waiting for host...", 16, 16);
    Serial.begin(115200);
}

void loop() {
    while (Serial.available() > 0) {
        if (auto message =
                decoder.feed(std::byte{static_cast<std::uint8_t>(Serial.read())}))
            handleMessage(*message);
    }
    pumpTouch();
}
