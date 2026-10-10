// PlatypusOS HAL — raw Bayer handling for CSI sensors (portable).
//
// The UNO Q's camss delivers IMX219 frames as MIPI-packed RAW10 (V4L2
// 'pRAA'): four 10-bit pixels in five bytes (four high bytes, then one byte
// of 2-bit low parts). Measurement wants raw data with known exposure and
// gain, not an uncalibrated ISP, so the CSI camera reads raw frames and turns
// them into images here.
//
// binnedRgb() collapses each 2x2 colour-filter cell into one RGB pixel: half
// resolution, no interpolation, so no demosaic artefacts on the silhouette
// edges Scout measures. Gains are applied after black-level subtraction; an
// optional gamma makes the preview readable without changing which pixels
// are dark or light (it is monotonic, so thresholding is unaffected in order).
//
// AutoExposure is a small deterministic controller over the sensor's own
// exposure (lines) and analogue gain (IMX219 code, gain = 256 / (256 - code)),
// preferring exposure before gain. Every frame's settings stay recordable.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace platypus::hal::bayer {

/// Colour order of the top-left 2x2 cell.
enum class Cfa : std::uint8_t { RGGB, GRBG, GBRG, BGGR };

struct Raw10Layout {
    std::uint32_t width = 0;         ///< pixels, multiple of 4
    std::uint32_t height = 0;        ///< rows, multiple of 2
    std::uint32_t bytesPerLine = 0;  ///< >= width * 5 / 4 (driver may pad)
};

/// Unpacks MIPI RAW10 into one 10-bit value per pixel, row-major.
/// Returns empty if the buffer is too small for the layout.
[[nodiscard]] std::vector<std::uint16_t> unpackRaw10(std::span<const std::byte> packed,
                                                     const Raw10Layout& layout);

/// Packs 10-bit values as MIPI RAW10 (tests and fixtures).
[[nodiscard]] std::vector<std::byte> packRaw10(std::span<const std::uint16_t> pixels,
                                               const Raw10Layout& layout);

struct RgbParams {
    Cfa cfa = Cfa::RGGB;
    std::uint16_t blackLevel = 64;  ///< IMX219 10-bit pedestal
    std::uint16_t whiteLevel = 1023;
    std::array<float, 3> gains{1.0f, 1.0f, 1.0f};  ///< R, G, B white balance
    bool gamma = true;                             ///< sRGB-like 1/2.2 for viewing
};

/// Per-channel means and the green 90th percentile, as fractions of the
/// usable range (white - black). Feeds white balance and auto-exposure.
struct CellStats {
    std::array<double, 3> mean{};  ///< R, G, B
    double greenP90 = 0.0;
};

/// One RGB888 pixel per 2x2 cell: (width/2) x (height/2) x 3 bytes.
/// Also fills `stats` (pre-gain, linear) when given.
[[nodiscard]] std::vector<std::uint8_t> binnedRgb(std::span<const std::uint16_t> raw,
                                                  std::uint32_t width, std::uint32_t height,
                                                  const RgbParams& params,
                                                  CellStats* stats = nullptr);

/// Gray-world white-balance gains (green = 1) from cell means, clamped.
[[nodiscard]] std::array<float, 3> grayWorldGains(const CellStats& stats);

struct Exposure {
    std::uint32_t lines = 0;     ///< sensor exposure, in line periods
    std::uint32_t gainCode = 0;  ///< IMX219 analogue gain register value
    std::uint32_t digitalCode = 256;  ///< IMX219 digital gain, Q8 (256 = 1x)
};

struct ExposureLimits {
    std::uint32_t minLines = 4;
    std::uint32_t maxLines = 1703;  ///< at the binned mode's default frame length
    std::uint32_t maxGainCode = 232;
    /// Digital gain is the last resort once lines and analogue gain are
    /// spent; it lifts the signal and the noise together. 256 disables it.
    std::uint32_t maxDigitalCode = 256;
};

[[nodiscard]] double imx219Gain(std::uint32_t gainCode);
[[nodiscard]] std::uint32_t imx219GainCode(double gain, std::uint32_t maxCode);

/// Steers the green 90th percentile toward `target` (fraction of range),
/// spending exposure lines first, then analogue gain, then digital gain.
class AutoExposure {
   public:
    explicit AutoExposure(ExposureLimits limits = {}, double target = 0.6)
        : limits_(limits), target_(target) {}

    /// Next settings from the frame captured with `current`.
    [[nodiscard]] Exposure next(const CellStats& stats, const Exposure& current) const;

   private:
    ExposureLimits limits_;
    double target_;
};

}  // namespace platypus::hal::bayer
