// PlatypusOS — portable pieces of the Linux KMS display backend.
//
// Everything in DrmDisplay that is pure logic lives here, free of Linux
// headers, so the host test suite exercises it on every platform CI builds:
// which connector and mode to drive, how the renderer's RGB565 frame becomes a
// scanout buffer, and how raw touch readings map onto the panel.
#pragma once

#include <platypus/hal/IDisplay.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace platypus::drm {

/// DRM_MODE_CONNECTOR_* values from <drm/drm_mode.h>, restated so this header
/// stays portable. Only the ones PlatypusOne can meet are named.
inline constexpr std::uint32_t kConnectorDisplayPort = 10;
inline constexpr std::uint32_t kConnectorHdmiA = 11;
inline constexpr std::uint32_t kConnectorEdp = 14;
inline constexpr std::uint32_t kConnectorDsi = 16;

/// Kernel-style connector name ("DSI", "DP", ...) for diagnostics.
constexpr std::string_view connectorTypeName(std::uint32_t type) noexcept {
    switch (type) {
        case kConnectorDisplayPort:
            return "DP";
        case kConnectorHdmiA:
            return "HDMI-A";
        case kConnectorEdp:
            return "eDP";
        case kConnectorDsi:
            return "DSI";
        default:
            return "other";
    }
}

struct ConnectorCandidate {
    std::uint32_t id = 0;
    std::uint32_t type = 0;
    bool connected = false;
    std::size_t modeCount = 0;
};

/// Index of the connector to drive: the first connected one of `preferredType`
/// if any, else the first connected one. A connector with no modes is treated
/// as absent — there is nothing to program it with.
///
/// The default preference is DSI because an integrated panel is the product
/// display: a monitor on the USB-C hub is a bench convenience and must not win
/// when both are attached. (On the UNO Q both cannot be live at once — they
/// share one display controller — but the rule should not depend on that.)
inline std::optional<std::size_t> chooseConnector(std::span<const ConnectorCandidate> candidates,
                                                  std::uint32_t preferredType = kConnectorDsi) {
    std::optional<std::size_t> fallback;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        const auto& c = candidates[i];
        if (!c.connected || c.modeCount == 0) continue;
        if (c.type == preferredType) return i;
        if (!fallback) fallback = i;
    }
    return fallback;
}

struct ModeCandidate {
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint32_t refreshHz = 0;
    bool preferred = false;
};

/// Index of the mode to program: the connector's preferred mode, else the
/// largest by area (ties broken by refresh rate). A panel reports exactly one
/// mode; a monitor reports many and marks the native one preferred.
inline std::optional<std::size_t> chooseMode(std::span<const ModeCandidate> modes) {
    std::optional<std::size_t> best;
    for (std::size_t i = 0; i < modes.size(); ++i) {
        if (modes[i].width == 0 || modes[i].height == 0) continue;
        if (modes[i].preferred) return i;
        if (!best) {
            best = i;
            continue;
        }
        const auto& a = modes[i];
        const auto& b = modes[*best];
        const auto areaA = std::uint32_t{a.width} * a.height;
        const auto areaB = std::uint32_t{b.width} * b.height;
        if (areaA > areaB || (areaA == areaB && a.refreshHz > b.refreshHz)) best = i;
    }
    return best;
}

/// The part of `region` that lies inside a width x height frame. An empty
/// region (zero width or height) means "nothing changed" and stays empty.
constexpr hal::DisplayRegion clampRegion(hal::DisplayRegion region, std::uint16_t width,
                                         std::uint16_t height) noexcept {
    if (region.x >= width || region.y >= height) return {};
    const auto roomX = static_cast<std::uint16_t>(width - region.x);
    const auto roomY = static_cast<std::uint16_t>(height - region.y);
    region.width = std::min(region.width, roomX);
    region.height = std::min(region.height, roomY);
    return region;
}

/// Expands one little-endian RGB565 pixel to XRGB8888 (X = 0xFF), replicating
/// the high bits into the low ones so full-scale stays full-scale: 0x1F red
/// becomes 0xFF, not 0xF8.
constexpr std::uint32_t rgb565ToXrgb8888(std::uint16_t pixel) noexcept {
    const std::uint32_t r5 = (pixel >> 11) & 0x1Fu;
    const std::uint32_t g6 = (pixel >> 5) & 0x3Fu;
    const std::uint32_t b5 = pixel & 0x1Fu;
    const std::uint32_t r = (r5 << 3) | (r5 >> 2);
    const std::uint32_t g = (g6 << 2) | (g6 >> 4);
    const std::uint32_t b = (b5 << 3) | (b5 >> 2);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

/// Converts `region` of a full-frame RGB565LE buffer (`width` x `height`,
/// tightly packed) into an XRGB8888 scanout buffer whose rows are `dstPitch`
/// bytes apart. Scanout buffers are padded, so the pitch is not width * 4.
///
/// XRGB8888 is written as its little-endian bytes (B, G, R, X), which is what
/// DRM_FORMAT_XRGB8888 means regardless of host byte order.
inline void blitRgb565ToXrgb8888(std::span<const std::byte> src, std::uint16_t width,
                                 std::uint16_t height, std::byte* dst, std::size_t dstPitch,
                                 hal::DisplayRegion region) {
    region = clampRegion(region, width, height);
    for (std::uint32_t y = region.y; y < std::uint32_t{region.y} + region.height; ++y) {
        const std::byte* in = src.data() + (std::size_t{y} * width + region.x) * 2;
        std::byte* out = dst + std::size_t{y} * dstPitch + std::size_t{region.x} * 4;
        for (std::uint32_t x = 0; x < region.width; ++x, in += 2, out += 4) {
            const auto pixel =
                static_cast<std::uint16_t>(std::to_integer<std::uint16_t>(in[0]) |
                                           (std::to_integer<std::uint16_t>(in[1]) << 8));
            const auto xrgb = rgb565ToXrgb8888(pixel);
            out[0] = static_cast<std::byte>(xrgb & 0xFFu);
            out[1] = static_cast<std::byte>((xrgb >> 8) & 0xFFu);
            out[2] = static_cast<std::byte>((xrgb >> 16) & 0xFFu);
            out[3] = static_cast<std::byte>(xrgb >> 24);
        }
    }
}

/// Maps a raw absolute-axis reading in [min, max] onto [0, size - 1], clamping
/// readings outside the advertised range (controllers do overshoot at edges).
/// Orientation is not handled here: on the UNO Q the device tree tells the
/// touch driver to invert, so readings arrive already in panel orientation.
constexpr std::uint16_t scaleAxis(std::int32_t value, std::int32_t min, std::int32_t max,
                                  std::uint16_t size) noexcept {
    if (size == 0) return 0;
    if (max <= min) return 0;
    value = std::clamp(value, min, max);
    const auto span = static_cast<std::int64_t>(max) - min;
    const auto scaled = (static_cast<std::int64_t>(value) - min) * (size - 1) / span;
    return static_cast<std::uint16_t>(scaled);
}

}  // namespace platypus::drm
