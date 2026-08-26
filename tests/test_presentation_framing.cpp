#include <platypus/hal/link/Framing.hpp>

#include <array>
#include <cassert>
#include <cstdio>
#include <string_view>
#include <vector>

namespace {

using namespace platypus;

std::optional<hal::link::Message> feedAll(hal::link::Decoder& decoder,
                                          std::span<const std::byte> bytes) {
    std::optional<hal::link::Message> result;
    for (const auto byte : bytes) {
        if (auto message = decoder.feed(byte)) result = std::move(message);
    }
    return result;
}

}  // namespace

void test_presentation_framing() {
    constexpr std::string_view check = "123456789";
    const auto checkBytes = std::as_bytes(std::span(check.data(), check.size()));
    assert(hal::link::crc16(checkBytes) == 0x29B1);

    const std::array payload{std::byte{0x10}, hal::link::kSync, std::byte{0x20}};
    const auto frame =
        hal::link::encode(hal::link::topics::kTile, payload, hal::link::kReplyExpected);
    assert(frame);
    assert((*frame)[0] == hal::link::kSync);
    assert((*frame)[1] == std::byte{payload.size()});
    assert((*frame)[5] == std::byte{0x11});
    assert((*frame)[6] == std::byte{0x00});
    assert((*frame)[7] == std::byte{hal::link::kReplyExpected});

    hal::link::Decoder decoder;
    const auto decoded = feedAll(decoder, *frame);
    assert(decoded);
    assert(decoded->topic == hal::link::topics::kTile);
    assert(decoded->flags == hal::link::kReplyExpected);
    assert(decoded->payload == std::vector<std::byte>(payload.begin(), payload.end()));

    // Bad CRC is discarded and the next valid frame still decodes.
    auto corrupt = *frame;
    corrupt.back() ^= std::byte{0x01};
    hal::link::Decoder resyncDecoder;
    assert(!feedAll(resyncDecoder, corrupt));
    assert(feedAll(resyncDecoder, *frame));

    // Encoder and decoder both enforce protocol bounds.
    std::vector<std::byte> maximum(hal::link::kMaxPayload);
    const auto maximumFrame = hal::link::encode(hal::link::topics::kTile, maximum);
    assert(maximumFrame);
    hal::link::Decoder maximumDecoder;
    const auto maximumDecoded = feedAll(maximumDecoder, *maximumFrame);
    assert(maximumDecoded);
    assert(maximumDecoded->payload.size() == hal::link::kMaxPayload);
    maximum.push_back(std::byte{0});
    assert(!hal::link::encode(hal::link::topics::kTile, maximum));
    assert(!hal::link::encode(hal::link::topics::kPing, {},
                              static_cast<std::uint8_t>(hal::link::kAllowedFlags | 0x80)));

    // An oversized declared length is rejected before any allocation based on it.
    const std::array oversizedHeader{
        hal::link::kSync, std::byte{0x01}, std::byte{0x00}, std::byte{0x01},
        std::byte{0x00},  std::byte{0x11}, std::byte{0x00}, std::byte{0x00},
    };
    hal::link::Decoder boundedDecoder;
    assert(!feedAll(boundedDecoder, oversizedHeader));
    assert(feedAll(boundedDecoder, *frame));

    std::puts("test_presentation_framing: OK");
}
