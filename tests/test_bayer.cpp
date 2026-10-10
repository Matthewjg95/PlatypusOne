// Raw Bayer handling for the CSI camera: RAW10 packing round trip, 2x2
// binning per colour-filter order, white balance, and the auto-exposure
// controller converging on a simulated linear sensor.
#include <platypus/hal/Bayer.hpp>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

using namespace platypus::hal::bayer;

void test_raw10_round_trip() {
    const Raw10Layout layout{8, 2, 12};  // 8 px -> 10 bytes, padded to 12
    std::vector<std::uint16_t> pixels = {0,  1,   2,   3,   1020, 1021, 1022, 1023,
                                         64, 512, 700, 333, 5,    6,    7,    8};
    const auto packed = packRaw10(pixels, layout);
    assert(packed.size() == 24);
    // First group: high bytes 0,0,0,0 and low byte 0b11'10'01'00.
    assert(std::to_integer<int>(packed[4]) == 0xE4);
    const auto unpacked = unpackRaw10(packed, layout);
    assert(unpacked == pixels);
    // Too small a buffer is refused, not read past.
    assert(unpackRaw10(std::span(packed).first(10), layout).empty());
}

/// A flat colour scene as one RGGB-ordered raw frame.
std::vector<std::uint16_t> flatScene(std::uint32_t w, std::uint32_t h, std::uint16_t r,
                                     std::uint16_t g, std::uint16_t b, Cfa cfa) {
    std::vector<std::uint16_t> raw(static_cast<std::size_t>(w) * h);
    for (std::uint32_t y = 0; y < h; ++y)
        for (std::uint32_t x = 0; x < w; ++x) {
            const int pos = static_cast<int>((y % 2) * 2 + (x % 2));
            int colour = 1;  // 0 R, 1 G, 2 B
            switch (cfa) {
                case Cfa::RGGB:
                    colour = pos == 0 ? 0 : pos == 3 ? 2 : 1;
                    break;
                case Cfa::BGGR:
                    colour = pos == 0 ? 2 : pos == 3 ? 0 : 1;
                    break;
                case Cfa::GRBG:
                    colour = pos == 1 ? 0 : pos == 2 ? 2 : 1;
                    break;
                case Cfa::GBRG:
                    colour = pos == 2 ? 0 : pos == 1 ? 2 : 1;
                    break;
            }
            raw[static_cast<std::size_t>(y) * w + x] = colour == 0 ? r : colour == 1 ? g : b;
        }
    return raw;
}

void test_binning_respects_cfa() {
    for (const Cfa cfa : {Cfa::RGGB, Cfa::GRBG, Cfa::GBRG, Cfa::BGGR}) {
        // Red-dominant scene: R near white, G mid, B at black.
        const auto raw = flatScene(8, 4, 1023, 543, 64, cfa);
        RgbParams p;
        p.cfa = cfa;
        p.gamma = false;
        CellStats stats;
        const auto rgb = binnedRgb(raw, 8, 4, p, &stats);
        assert(rgb.size() == 4u * 2u * 3u);
        assert(rgb[0] == 255);                     // red at full scale
        assert(std::abs(int(rgb[1]) - 127) <= 1);  // (543-64)/959 ~ 0.5
        assert(rgb[2] == 0);                       // blue at black level
        assert(std::abs(stats.mean[0] - 1.0) < 1e-9);
        assert(std::abs(stats.mean[2]) < 1e-9);
    }
}

void test_gray_world_balances_a_tint() {
    // A neutral grey seen through a green-heavy sensor.
    const auto raw = flatScene(16, 16, 300, 600, 250, Cfa::RGGB);
    CellStats stats;
    (void)binnedRgb(raw, 16, 16, RgbParams{}, &stats);
    const auto gains = grayWorldGains(stats);
    RgbParams p;
    p.gains = gains;
    p.gamma = false;
    const auto rgb = binnedRgb(raw, 16, 16, p);
    assert(std::abs(int(rgb[0]) - int(rgb[1])) <= 1);
    assert(std::abs(int(rgb[2]) - int(rgb[1])) <= 1);
}

void test_gain_codes() {
    assert(std::abs(imx219Gain(0) - 1.0) < 1e-12);
    assert(std::abs(imx219Gain(128) - 2.0) < 1e-12);
    assert(imx219GainCode(2.0, 232) == 128);
    assert(imx219GainCode(100.0, 232) == 232);  // clamped to the sensor's maximum
    assert(imx219GainCode(0.5, 232) == 0);
}

void test_auto_exposure_converges() {
    // Simulated linear sensor: green p90 = scene * lines * gain, clipped.
    const ExposureLimits limits{4, 1703, 232};
    const AutoExposure ae(limits, 0.6);
    for (const double scene : {0.0005, 0.00002, 0.01}) {
        Exposure e{1600, 0};
        for (int i = 0; i < 40; ++i) {
            CellStats s;
            s.greenP90 = std::min(1.0, scene * e.lines * imx219Gain(e.gainCode));
            e = ae.next(s, e);
            assert(e.lines >= limits.minLines && e.lines <= limits.maxLines);
            assert(e.gainCode <= limits.maxGainCode);
        }
        const double level = std::min(1.0, scene * e.lines * imx219Gain(e.gainCode));
        const bool saturatedDark = e.lines == limits.maxLines && e.gainCode == limits.maxGainCode;
        assert(saturatedDark || std::abs(level - 0.6) < 0.08);
        // Exposure is used before gain: gain only rises once lines are maxed.
        if (e.gainCode > 0) assert(e.lines == limits.maxLines);
    }
}

}  // namespace

void test_bayer() {
    test_raw10_round_trip();
    test_binning_respects_cfa();
    test_gray_world_balances_a_tint();
    test_gain_codes();
    test_auto_exposure_converges();
    std::puts("test_bayer: OK");
}
