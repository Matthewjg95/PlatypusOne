#include "platypus/export/Png.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace platypus::exporter {

namespace {

std::array<std::uint32_t, 256> makeCrcTable() {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t n = 0; n < 256; ++n) {
        std::uint32_t c = n;
        for (int k = 0; k < 8; ++k)
            c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        table[n] = c;
    }
    return table;
}

std::uint32_t crc32(std::span<const std::uint8_t> bytes) {
    static const auto table = makeCrcTable();
    std::uint32_t c = 0xFFFFFFFFu;
    for (const auto b : bytes)
        c = table[(c ^ b) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

void putU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v >> 24));
    out.push_back(static_cast<std::uint8_t>(v >> 16));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v));
}

/// Appends a chunk: length, type, data, CRC over type + data.
void chunk(std::vector<std::uint8_t>& out, std::string_view type,
           const std::vector<std::uint8_t>& data) {
    putU32(out, static_cast<std::uint32_t>(data.size()));
    const std::size_t typeAt = out.size();
    out.insert(out.end(), type.begin(), type.end());
    out.insert(out.end(), data.begin(), data.end());
    putU32(out, crc32({out.data() + typeAt, out.size() - typeAt}));
}

}  // namespace

std::vector<std::uint8_t> encodePng(std::span<const std::uint8_t> pixels, std::uint32_t width,
                                    std::uint32_t height, std::uint32_t channels) {
    if ((channels != 1 && channels != 3) || width == 0 || height == 0) return {};
    const std::size_t stride = static_cast<std::size_t>(width) * channels;
    if (pixels.size() != stride * height) return {};

    // Raw scanlines, each prefixed with filter type 0 (none).
    std::vector<std::uint8_t> raw;
    raw.reserve((stride + 1) * height);
    for (std::uint32_t y = 0; y < height; ++y) {
        raw.push_back(0);
        const auto row = pixels.subspan(static_cast<std::size_t>(y) * stride, stride);
        raw.insert(raw.end(), row.begin(), row.end());
    }

    // zlib: header, stored deflate blocks of at most 65535 bytes, Adler-32.
    std::vector<std::uint8_t> z{0x78, 0x01};
    std::uint32_t a = 1, b = 0;
    for (const auto byte : raw) {
        a = (a + byte) % 65521u;
        b = (b + a) % 65521u;
    }
    for (std::size_t at = 0; at < raw.size(); at += 65535) {
        const std::size_t len = std::min<std::size_t>(65535, raw.size() - at);
        const bool last = at + len >= raw.size();
        z.push_back(last ? 1 : 0);  // BFINAL, BTYPE = 00 (stored)
        const auto l = static_cast<std::uint16_t>(len);
        const auto nl = static_cast<std::uint16_t>(~l);
        z.push_back(static_cast<std::uint8_t>(l & 0xFF));
        z.push_back(static_cast<std::uint8_t>(l >> 8));
        z.push_back(static_cast<std::uint8_t>(nl & 0xFF));
        z.push_back(static_cast<std::uint8_t>(nl >> 8));
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(at),
                 raw.begin() + static_cast<std::ptrdiff_t>(at + len));
        if (last) break;
    }
    putU32(z, (b << 16) | a);

    std::vector<std::uint8_t> png{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<std::uint8_t> ihdr;
    putU32(ihdr, width);
    putU32(ihdr, height);
    ihdr.push_back(8);                                                 // bit depth
    ihdr.push_back(static_cast<std::uint8_t>(channels == 3 ? 2 : 0));  // RGB or gray
    ihdr.push_back(0);                                                 // deflate
    ihdr.push_back(0);                                                 // adaptive filtering
    ihdr.push_back(0);                                                 // no interlace
    chunk(png, "IHDR", ihdr);
    chunk(png, "IDAT", z);
    chunk(png, "IEND", {});
    return png;
}

}  // namespace platypus::exporter
