// KMS display backend — the portable logic, tested on every host.
//
// DrmDisplay itself needs a real DRM device; everything that decides what it
// does (connector, mode, pixel conversion, touch mapping) is here, where a
// mistake is a failed test instead of a black panel on the bench.
#include "drm/KmsHelpers.hpp"

#include <array>
#include <cassert>
#include <cstdio>
#include <vector>

namespace {

using namespace platypus;
using drm::ConnectorCandidate;
using drm::ModeCandidate;

void test_connector_choice() {
    // The integrated panel wins over a monitor on the hub, whatever the order.
    const std::array<ConnectorCandidate, 2> both{{
        {31, drm::kConnectorDisplayPort, true, 12},
        {32, drm::kConnectorDsi, true, 1},
    }};
    assert(drm::chooseConnector(both) == 1u);

    // No panel: the monitor is used rather than nothing.
    const std::array<ConnectorCandidate, 2> monitorOnly{{
        {31, drm::kConnectorDisplayPort, true, 12},
        {32, drm::kConnectorDsi, false, 0},
    }};
    assert(drm::chooseConnector(monitorOnly) == 0u);

    // Connected but modeless is as good as absent; nothing connected is nullopt.
    const std::array<ConnectorCandidate, 2> none{{
        {31, drm::kConnectorDisplayPort, false, 0},
        {32, drm::kConnectorDsi, true, 0},
    }};
    assert(!drm::chooseConnector(none));
    assert(!drm::chooseConnector({}));

    // The bench fixture as seen on 2026-09-28: one DP connector, disconnected.
    const std::array<ConnectorCandidate, 1> tonight{{{31, drm::kConnectorDisplayPort, false, 0}}};
    assert(!drm::chooseConnector(tonight));

    assert(drm::connectorTypeName(drm::kConnectorDsi) == "DSI");
    assert(drm::connectorTypeName(drm::kConnectorDisplayPort) == "DP");
}

void test_mode_choice() {
    // The preferred flag beats size: a monitor's native mode, not its largest.
    const std::array<ModeCandidate, 3> monitor{{
        {1920, 1080, 60, false},
        {1280, 720, 60, true},
        {640, 480, 60, false},
    }};
    assert(drm::chooseMode(monitor) == 1u);

    // Without one, the largest; at equal size, the higher refresh.
    const std::array<ModeCandidate, 3> unflagged{{
        {1280, 720, 30, false},
        {1920, 1080, 30, false},
        {1920, 1080, 60, false},
    }};
    assert(drm::chooseMode(unflagged) == 2u);

    // The Waveshare 5inch DSI LCD reports its single 800x480 mode.
    const std::array<ModeCandidate, 1> panel{{{800, 480, 60, true}}};
    assert(drm::chooseMode(panel) == 0u);

    const std::array<ModeCandidate, 1> degenerate{{{0, 480, 60, true}}};
    assert(!drm::chooseMode(degenerate));
}

void test_pixel_expansion() {
    // Full scale stays full scale: bit replication, not a plain shift.
    assert(drm::rgb565ToXrgb8888(0xF800) == 0xFFFF0000u);  // red
    assert(drm::rgb565ToXrgb8888(0x07E0) == 0xFF00FF00u);  // green
    assert(drm::rgb565ToXrgb8888(0x001F) == 0xFF0000FFu);  // blue
    assert(drm::rgb565ToXrgb8888(0xFFFF) == 0xFFFFFFFFu);  // white
    assert(drm::rgb565ToXrgb8888(0x0000) == 0xFF000000u);  // black, X still set
}

void test_blit_respects_region_and_pitch() {
    constexpr std::uint16_t w = 4, h = 3;
    // Source: every pixel red except (2,1), which is blue — little-endian RGB565.
    std::vector<std::byte> src(w * h * 2);
    for (std::size_t i = 0; i < w * h; ++i) {
        const std::uint16_t px = (i == 1 * w + 2) ? 0x001F : 0xF800;
        src[i * 2] = static_cast<std::byte>(px & 0xFF);
        src[i * 2 + 1] = static_cast<std::byte>(px >> 8);
    }
    // Scanout rows padded to 32 bytes, as real dumb buffers are.
    constexpr std::size_t pitch = 32;
    std::vector<std::byte> dst(pitch * h, std::byte{0xAA});

    drm::blitRgb565ToXrgb8888(src, w, h, dst.data(), pitch, {1, 1, 2, 1});

    const auto at = [&](std::size_t x, std::size_t y, std::size_t c) {
        return dst[y * pitch + x * 4 + c];
    };
    // (1,1) red: bytes B, G, R, X.
    assert(at(1, 1, 0) == std::byte{0x00} && at(1, 1, 1) == std::byte{0x00});
    assert(at(1, 1, 2) == std::byte{0xFF} && at(1, 1, 3) == std::byte{0xFF});
    // (2,1) blue.
    assert(at(2, 1, 0) == std::byte{0xFF} && at(2, 1, 2) == std::byte{0x00});
    // Outside the region, and the pitch padding, are untouched.
    assert(at(0, 1, 0) == std::byte{0xAA});
    assert(at(3, 1, 0) == std::byte{0xAA});
    assert(at(1, 0, 0) == std::byte{0xAA});
    assert(dst[1 * pitch + w * 4] == std::byte{0xAA});

    // A region running off the frame is clipped, never overrun.
    const auto clipped = drm::clampRegion({3, 2, 10, 10}, w, h);
    assert(clipped.width == 1 && clipped.height == 1);
    assert(drm::clampRegion({9, 0, 1, 1}, w, h).width == 0);
}

void test_touch_scaling() {
    // Raw range onto panel pixels, ends inclusive.
    assert(drm::scaleAxis(0, 0, 799, 800) == 0);
    assert(drm::scaleAxis(799, 0, 799, 800) == 799);
    assert(drm::scaleAxis(400, 0, 799, 800) == 400);
    // A controller with a different raw range still lands on the panel.
    assert(drm::scaleAxis(4095, 0, 4095, 480) == 479);
    // Overshoot at the bezel clamps instead of wrapping.
    assert(drm::scaleAxis(-5, 0, 799, 800) == 0);
    assert(drm::scaleAxis(900, 0, 799, 800) == 799);
    // A device reporting a nonsense range yields 0, not a divide-by-zero.
    assert(drm::scaleAxis(10, 5, 5, 800) == 0);
}

}  // namespace

void test_kms_helpers() {
    test_connector_choice();
    test_mode_choice();
    test_pixel_expansion();
    test_blit_respects_region_and_pitch();
    test_touch_scaling();
    std::puts("test_kms_helpers: OK");
}
