// MCU bridge framing tests: round-trip, corruption recovery, resync.
#include <platypus/hal/mcu/Framing.hpp>

#include <cassert>
#include <cstdio>

namespace {

using namespace platypus::hal::mcu;

std::optional<Message> feedAll(Decoder& d, std::span<const std::byte> bytes) {
    std::optional<Message> last;
    for (const auto b : bytes)
        if (auto m = d.feed(b)) last = std::move(m);
    return last;
}

}  // namespace

void test_mcu_framing() {
    // Round-trip with payload.
    const std::byte payload[] = {std::byte{0x01}, std::byte{0xFF}, std::byte{0x42}};
    const auto frame = encode(0x0123, payload);
    Decoder d;
    auto msg = feedAll(d, frame);
    assert(msg);
    assert(msg->topic == 0x0123);
    assert(msg->payload.size() == 3);
    assert(msg->payload[2] == std::byte{0x42});

    // Empty payload round-trip.
    const auto pingFrame = encode(topics::kPing, {});
    msg = feedAll(d, pingFrame);
    assert(msg && msg->topic == topics::kPing && msg->payload.empty());

    // Corrupted CRC is dropped...
    auto bad = frame;
    bad.back() = std::byte{static_cast<std::uint8_t>(
        std::to_integer<std::uint8_t>(bad.back()) ^ 0xFF)};
    assert(!feedAll(d, bad));

    // ...and the decoder resyncs on the next good frame, even after noise.
    const std::byte noise[] = {std::byte{0x00}, std::byte{0xA5}, std::byte{0xFF},
                               std::byte{0xFF}};  // fake sync + oversized len
    feedAll(d, noise);
    msg = feedAll(d, frame);
    assert(msg && msg->topic == 0x0123);

    std::puts("test_mcu_framing: OK");
}
