#include <platypus/hal/Bayer.hpp>

#include <algorithm>
#include <cmath>

namespace platypus::hal::bayer {

std::vector<std::uint16_t> unpackRaw10(std::span<const std::byte> packed,
                                       const Raw10Layout& layout) {
    const std::uint32_t w = layout.width, h = layout.height;
    if (w % 4 != 0 || layout.bytesPerLine < w / 4 * 5 ||
        packed.size() < static_cast<std::size_t>(layout.bytesPerLine) * h)
        return {};
    std::vector<std::uint16_t> out(static_cast<std::size_t>(w) * h);
    for (std::uint32_t y = 0; y < h; ++y) {
        const auto* row = packed.data() + static_cast<std::size_t>(y) * layout.bytesPerLine;
        auto* dst = out.data() + static_cast<std::size_t>(y) * w;
        for (std::uint32_t g = 0; g < w / 4; ++g) {
            const auto* p = row + g * 5;
            const auto low = std::to_integer<std::uint16_t>(p[4]);
            for (std::uint32_t k = 0; k < 4; ++k)
                dst[g * 4 + k] = static_cast<std::uint16_t>(
                    (std::to_integer<std::uint16_t>(p[k]) << 2) | ((low >> (2 * k)) & 0x3));
        }
    }
    return out;
}

std::vector<std::byte> packRaw10(std::span<const std::uint16_t> pixels, const Raw10Layout& layout) {
    const std::uint32_t w = layout.width, h = layout.height;
    std::vector<std::byte> out(static_cast<std::size_t>(layout.bytesPerLine) * h, std::byte{0});
    if (w % 4 != 0 || pixels.size() < static_cast<std::size_t>(w) * h) return {};
    for (std::uint32_t y = 0; y < h; ++y) {
        auto* row = out.data() + static_cast<std::size_t>(y) * layout.bytesPerLine;
        const auto* src = pixels.data() + static_cast<std::size_t>(y) * w;
        for (std::uint32_t g = 0; g < w / 4; ++g) {
            std::uint8_t low = 0;
            for (std::uint32_t k = 0; k < 4; ++k) {
                const std::uint16_t v = src[g * 4 + k] & 0x3ff;
                row[g * 5 + k] = static_cast<std::byte>(v >> 2);
                low = static_cast<std::uint8_t>(low | ((v & 0x3) << (2 * k)));
            }
            row[g * 5 + 4] = static_cast<std::byte>(low);
        }
    }
    return out;
}

namespace {

/// Position of R, G (either), G (other) and B inside the 2x2 cell, as
/// (dy * 2 + dx).
struct CellMap {
    int r, g1, g2, b;
};

CellMap cellMap(Cfa cfa) {
    switch (cfa) {
        case Cfa::RGGB:
            return {0, 1, 2, 3};
        case Cfa::GRBG:
            return {1, 0, 3, 2};
        case Cfa::GBRG:
            return {2, 0, 3, 1};
        case Cfa::BGGR:
            return {3, 1, 2, 0};
    }
    return {0, 1, 2, 3};
}

std::array<std::uint8_t, 4096> gammaLut(bool gamma) {
    std::array<std::uint8_t, 4096> lut{};
    for (std::size_t i = 0; i < lut.size(); ++i) {
        const double x = static_cast<double>(i) / (lut.size() - 1);
        lut[i] =
            static_cast<std::uint8_t>(std::lround(255.0 * (gamma ? std::pow(x, 1.0 / 2.2) : x)));
    }
    return lut;
}

}  // namespace

std::vector<std::uint8_t> binnedRgb(std::span<const std::uint16_t> raw, std::uint32_t width,
                                    std::uint32_t height, const RgbParams& params,
                                    CellStats* stats) {
    const std::uint32_t ow = width / 2, oh = height / 2;
    if (ow == 0 || oh == 0 || raw.size() < static_cast<std::size_t>(width) * height) return {};
    const CellMap m = cellMap(params.cfa);
    const double range = std::max(1, params.whiteLevel - params.blackLevel);
    static const auto lutGamma = gammaLut(true);
    static const auto lutLinear = gammaLut(false);
    const auto& lut = params.gamma ? lutGamma : lutLinear;

    std::vector<std::uint8_t> out(static_cast<std::size_t>(ow) * oh * 3);
    std::array<double, 3> sum{};
    std::array<std::uint32_t, 256> greenHist{};
    for (std::uint32_t y = 0; y < oh; ++y) {
        const auto* r0 = raw.data() + static_cast<std::size_t>(2 * y) * width;
        const auto* r1 = r0 + width;
        auto* dst = out.data() + static_cast<std::size_t>(y) * ow * 3;
        for (std::uint32_t x = 0; x < ow; ++x) {
            const std::uint16_t cell[4] = {r0[2 * x], r0[2 * x + 1], r1[2 * x], r1[2 * x + 1]};
            const auto lin = [&](int i) {
                return std::max(0.0, static_cast<double>(cell[i]) - params.blackLevel) / range;
            };
            const double rr = lin(m.r), gg = 0.5 * (lin(m.g1) + lin(m.g2)), bb = lin(m.b);
            sum[0] += rr;
            sum[1] += gg;
            sum[2] += bb;
            ++greenHist[static_cast<std::size_t>(std::min(255.0, gg * 255.0))];
            const double v[3] = {rr * params.gains[0], gg * params.gains[1], bb * params.gains[2]};
            for (int c = 0; c < 3; ++c)
                dst[x * 3 + c] = lut[static_cast<std::size_t>(std::clamp(v[c], 0.0, 1.0) * 4095.0)];
        }
    }
    if (stats) {
        const double n = static_cast<double>(ow) * oh;
        for (int c = 0; c < 3; ++c)
            stats->mean[c] = sum[c] / n;
        const auto want = static_cast<std::uint64_t>(0.9 * n);
        std::uint64_t acc = 0;
        std::size_t bin = 0;
        for (; bin < greenHist.size(); ++bin) {
            acc += greenHist[bin];
            if (acc >= want) break;
        }
        stats->greenP90 = (static_cast<double>(bin) + 0.5) / 255.0;
    }
    return out;
}

std::array<float, 3> grayWorldGains(const CellStats& stats) {
    const auto gain = [&](double c) {
        if (c <= 1e-6) return 1.0f;
        return static_cast<float>(std::clamp(stats.mean[1] / c, 0.25, 4.0));
    };
    return {gain(stats.mean[0]), 1.0f, gain(stats.mean[2])};
}

double imx219Gain(std::uint32_t gainCode) {
    return 256.0 / (256.0 - std::min<std::uint32_t>(gainCode, 255));
}

std::uint32_t imx219GainCode(double gain, std::uint32_t maxCode) {
    if (gain <= 1.0) return 0;
    const double code = 256.0 - 256.0 / gain;
    return std::min(maxCode, static_cast<std::uint32_t>(std::lround(std::max(0.0, code))));
}

Exposure AutoExposure::next(const CellStats& stats, const Exposure& current) const {
    // Brightness is proportional to lines x gain. Aim the green 90th
    // percentile at the target; damp the step so a single bright frame
    // cannot swing the next one wildly.
    const double level = std::max(stats.greenP90, 1.0 / 255.0);
    const double ratio = std::clamp(target_ / level, 0.5, 2.0);
    if (std::abs(ratio - 1.0) < 0.08) return current;  // dead band: hold steady
    const double total =
        std::max(1.0, static_cast<double>(current.lines)) * imx219Gain(current.gainCode) * ratio;

    Exposure out;
    const double lines = std::clamp(total, static_cast<double>(limits_.minLines),
                                    static_cast<double>(limits_.maxLines));
    out.lines = static_cast<std::uint32_t>(std::lround(lines));
    out.gainCode = imx219GainCode(total / lines, limits_.maxGainCode);
    return out;
}

}  // namespace platypus::hal::bayer
