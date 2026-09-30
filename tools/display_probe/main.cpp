// display_probe — bring-up tool for the UNO Q's local display.
//
//   display_probe                  what DRM exposes and what DrmDisplay chose
//   display_probe --pattern [S]    colour quadrants for S seconds (default 10)
//   display_probe --input [S]      echo touch and board-button events for S seconds
//   display_probe --prefer dp|dsi  which connector wins when both are connected
//
// The pattern is TEST_CHECKLISTS §3: red, green, blue and white quadrants (a
// swapped pair means a byte- or channel-order bug), inside a 2 px yellow frame
// (a missing edge means the mode or pitch is wrong, not the panel).
//
// Driving the display needs DRM master. On the stock image lightdm/Xorg holds
// it, and this tool says so; stop the display server first:
//   sudo systemctl stop lightdm
// On exit the CRTC is handed back to whatever it showed before.
#include "drm/DrmDisplay.hpp"
#include "drm/KmsHelpers.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace platypus;

void fill(std::vector<std::uint16_t>& fb, std::uint16_t w, std::uint16_t x0, std::uint16_t y0,
          std::uint16_t x1, std::uint16_t y1, std::uint16_t colour) {
    for (std::uint32_t y = y0; y < y1; ++y)
        for (std::uint32_t x = x0; x < x1; ++x)
            fb[y * w + x] = colour;
}

std::vector<std::uint16_t> testPattern(std::uint16_t w, std::uint16_t h) {
    std::vector<std::uint16_t> fb(std::size_t{w} * h);
    const auto hw = static_cast<std::uint16_t>(w / 2);
    const auto hh = static_cast<std::uint16_t>(h / 2);
    fill(fb, w, 0, 0, hw, hh, 0xF800);  // red
    fill(fb, w, hw, 0, w, hh, 0x07E0);  // green
    fill(fb, w, 0, hh, hw, h, 0x001F);  // blue
    fill(fb, w, hw, hh, w, h, 0xFFFF);  // white
    constexpr std::uint16_t yellow = 0xFFE0;
    constexpr std::uint16_t edge = 2;
    fill(fb, w, 0, 0, w, edge, yellow);
    fill(fb, w, 0, static_cast<std::uint16_t>(h - edge), w, h, yellow);
    fill(fb, w, 0, 0, edge, h, yellow);
    fill(fb, w, static_cast<std::uint16_t>(w - edge), 0, w, h, yellow);
    return fb;
}

int usage() {
    std::fprintf(stderr, "usage: display_probe [--pattern [S]] [--input [S]] [--prefer dp|dsi]\n");
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    int patternSeconds = 0;
    int inputSeconds = 0;
    drm::DrmDisplayConfig config;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto seconds = [&](int fallback) {
            if (i + 1 < argc && argv[i + 1][0] != '-') return std::atoi(argv[++i]);
            return fallback;
        };
        if (arg == "--pattern")
            patternSeconds = seconds(10);
        else if (arg == "--input")
            inputSeconds = seconds(30);
        else if (arg == "--prefer" && i + 1 < argc) {
            const std::string which = argv[++i];
            if (which == "dp")
                config.preferredConnectorType = drm::kConnectorDisplayPort;
            else if (which == "dsi")
                config.preferredConnectorType = drm::kConnectorDsi;
            else
                return usage();
        } else {
            return usage();
        }
    }

    drm::DrmDisplay display(config);
    if (const auto status = display.open(); !status) {
        const auto reason = hal::to_string(status.error());
        std::fprintf(stderr, "display: %.*s - %s\n", static_cast<int>(reason.size()), reason.data(),
                     display.diagnostic().c_str());
        return 1;
    }

    const auto& s = display.selection();
    const auto type = drm::connectorTypeName(s.connectorType);
    std::printf("display: %.*s connector %u, crtc %u, %ux%u @ %u Hz\n",
                static_cast<int>(type.size()), type.data(), s.connectorId, s.crtcId, s.width,
                s.height, s.refreshHz);
    std::printf("touch:   %s\n", s.touchDevice.empty() ? "none found" : s.touchDevice.c_str());
    std::printf("buttons: %s\n", s.buttonDevice.empty() ? "none found" : s.buttonDevice.c_str());
    std::fflush(stdout);

    if (patternSeconds > 0) {
        const auto fb = testPattern(s.width, s.height);
        if (const auto status = display.present(std::as_bytes(std::span(fb))); !status) {
            const auto reason = hal::to_string(status.error());
            std::fprintf(stderr, "present: %.*s\n", static_cast<int>(reason.size()), reason.data());
            return 1;
        }
        std::printf("pattern: red | green / blue | white, yellow frame - %d s\n", patternSeconds);
        std::fflush(stdout);
        if (inputSeconds == 0) std::this_thread::sleep_for(std::chrono::seconds(patternSeconds));
    }

    if (inputSeconds > 0) {
        display.onTouch([](const hal::TouchEvent& e) {
            const char* kind = e.type == hal::TouchEvent::Type::Down   ? "down"
                               : e.type == hal::TouchEvent::Type::Move ? "move"
                                                                       : "up";
            std::printf("touch %-4s %4u %4u\n", kind, e.x, e.y);
            std::fflush(stdout);
        });
        display.onButton([](const hal::ButtonEvent& e) {
            std::printf("button %u %s\n", e.id, e.pressed ? "pressed" : "released");
            std::fflush(stdout);
        });
        std::printf("input:   listening %d s\n", inputSeconds);
        std::fflush(stdout);
        std::this_thread::sleep_for(std::chrono::seconds(inputSeconds));
    }
    return 0;
}
