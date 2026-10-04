// PlatypusOS services — minimal PNG encoder.
//
// Session artifacts must open anywhere (a CAD canvas, a browser, a phone), and
// the observation store deliberately keeps raw sensor bytes. This bridges the
// two without a dependency: 8-bit grayscale or RGB, one IDAT holding a zlib
// stream of *stored* (uncompressed) deflate blocks. Larger than a compressed
// PNG, but valid everywhere and a few dozen lines. Compression can come later
// without changing callers.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace platypus::exporter {

/// Encodes tightly packed rows (channels = 1 gray, 3 RGB). Returns empty on
/// a size mismatch or unsupported channel count.
[[nodiscard]] std::vector<std::uint8_t> encodePng(std::span<const std::uint8_t> pixels,
                                                  std::uint32_t width, std::uint32_t height,
                                                  std::uint32_t channels);

}  // namespace platypus::exporter
