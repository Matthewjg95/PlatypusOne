// CAD and image handoff: what leaves PlatypusOne must open in mesh2cad's
// Outline Forge, Fusion 360 / KiCad (DXF) and any image viewer (PNG), with the
// geometry exactly where it was measured.
#include <platypus/export/OutlineExport.hpp>
#include <platypus/export/Png.hpp>
#include <platypus/observation/Json.hpp>

#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace platypus;

/// A 10 x 4 px plate with one square hole, in image axes (y down).
geometry::Outline2 plate() {
    geometry::Outline2 o;
    o.outer = {{0, 0}, {10, 0}, {10, 4}, {0, 4}};
    o.holes = {{{4, 1}, {4, 3}, {6, 3}, {6, 1}}};
    return o;
}

std::size_t count(const std::string& haystack, const std::string& needle) {
    std::size_t n = 0;
    for (auto at = haystack.find(needle); at != std::string::npos;
         at = haystack.find(needle, at + 1))
        ++n;
    return n;
}

void test_outline_forge_json_matches_the_shadowscan_schema() {
    const auto text = exporter::outlineForgeJson(plate(), 0.25);
    const auto parsed = observation::json::parse(text);
    assert(parsed.ok());
    const auto& v = *parsed.value;
    assert(v.find("format")->asString() == "shadowscan-outline");
    assert(v.find("units")->asString() == "px");
    assert(v.find("scale_mm_per_unit")->asNumber() == 0.25);
    const auto& loops = v.find("outlines")->asArray();
    assert(loops.size() == 2);  // outer + hole: Outline Forge nests the hole
    const auto& first = loops[0].asArray()[1].asArray();
    assert(first[0].asNumber() == 10.0 && first[1].asNumber() == 0.0);  // pixels, unscaled
}

void test_dxf_is_millimetres_in_cad_axes() {
    const auto dxf = exporter::outlineDxf(plate(), 0.25);
    assert(dxf.find("AC1009") != std::string::npos);  // R12
    assert(count(dxf, "\nPOLYLINE\n") == 2);
    assert(count(dxf, "\nVERTEX\n") == 8);
    assert(count(dxf, "\nSEQEND\n") == 2);
    assert(count(dxf, "\nHOLES\n") > 0);
    assert(dxf.size() >= 4 && dxf.compare(dxf.size() - 4, 4, "EOF\n") == 0);

    // Parse the vertices back: image (0,0) is the top-left corner, so in CAD
    // axes (origin at the bottom-left, y up) it lands at (0, 4 px * 0.25).
    std::istringstream in(dxf);
    std::string code, value;
    std::vector<std::pair<double, double>> vertices;
    bool inVertex = false;
    double x = 0;
    while (std::getline(in, code) && std::getline(in, value)) {
        if (code == "0") inVertex = (value == "VERTEX");
        if (!inVertex) continue;
        if (code == "10") x = std::stod(value);
        if (code == "20") vertices.push_back({x, std::stod(value)});
    }
    assert(vertices.size() == 8);
    assert(vertices[0].first == 0.0 && vertices[0].second == 1.0);  // (0,0) px
    assert(vertices[2].first == 2.5 && vertices[2].second == 0.0);  // (10,4) px
    for (const auto& [vx, vy] : vertices)
        assert(vx >= 0.0 && vy >= 0.0);
}

// --- PNG, verified by an independent decoder ------------------------------

std::uint32_t be32(const std::vector<std::uint8_t>& b, std::size_t at) {
    return (std::uint32_t{b[at]} << 24) | (std::uint32_t{b[at + 1]} << 16) |
           (std::uint32_t{b[at + 2]} << 8) | b[at + 3];
}

/// Bitwise CRC-32, deliberately not the encoder's table implementation.
std::uint32_t crcBitwise(const std::uint8_t* p, std::size_t n) {
    std::uint32_t c = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < n; ++i) {
        c ^= p[i];
        for (int k = 0; k < 8; ++k)
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

void test_png_decodes_to_the_same_pixels() {
    // 300 x 250 RGB: 225,250 bytes of scanlines, so the stream needs several
    // 65535-byte stored blocks — the boundary is where encoders go wrong.
    constexpr std::uint32_t w = 300, h = 250;
    std::vector<std::uint8_t> rgb(std::size_t{w} * h * 3);
    for (std::size_t i = 0; i < rgb.size(); ++i)
        rgb[i] = static_cast<std::uint8_t>((i * 7 + i / 3) & 0xFF);
    const auto png = exporter::encodePng(rgb, w, h, 3);
    assert(!png.empty());

    const std::uint8_t signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    for (int i = 0; i < 8; ++i)
        assert(png[static_cast<std::size_t>(i)] == signature[i]);

    // Walk chunks, verifying every CRC and collecting IDAT.
    std::vector<std::uint8_t> idat;
    std::size_t at = 8;
    bool sawEnd = false;
    while (at + 12 <= png.size()) {
        const auto len = be32(png, at);
        const std::string type(png.begin() + static_cast<std::ptrdiff_t>(at + 4),
                               png.begin() + static_cast<std::ptrdiff_t>(at + 8));
        assert(crcBitwise(&png[at + 4], len + 4) == be32(png, at + 8 + len));
        if (type == "IHDR") {
            assert(be32(png, at + 8) == w && be32(png, at + 12) == h);
            assert(png[at + 16] == 8 && png[at + 17] == 2);  // 8-bit RGB
        }
        if (type == "IDAT")
            idat.insert(idat.end(), png.begin() + static_cast<std::ptrdiff_t>(at + 8),
                        png.begin() + static_cast<std::ptrdiff_t>(at + 8 + len));
        if (type == "IEND") sawEnd = true;
        at += 12 + len;
    }
    assert(sawEnd && at == png.size());

    // Inflate the stored blocks and strip the per-row filter bytes.
    assert(idat[0] == 0x78);
    std::vector<std::uint8_t> raw;
    std::size_t p = 2;
    while (true) {
        const bool final = idat[p] & 1;
        assert(((idat[p] >> 1) & 3) == 0);  // stored
        const std::size_t len = idat[p + 1] | (std::size_t{idat[p + 2]} << 8);
        const std::size_t nlen = idat[p + 3] | (std::size_t{idat[p + 4]} << 8);
        assert((len ^ nlen) == 0xFFFF);
        raw.insert(raw.end(), idat.begin() + static_cast<std::ptrdiff_t>(p + 5),
                   idat.begin() + static_cast<std::ptrdiff_t>(p + 5 + len));
        p += 5 + len;
        if (final) break;
    }
    assert(raw.size() == (std::size_t{w} * 3 + 1) * h);
    for (std::uint32_t y = 0; y < h; ++y) {
        const std::size_t row = std::size_t{y} * (w * 3 + 1);
        assert(raw[row] == 0);  // filter: none
        for (std::size_t i = 0; i < std::size_t{w} * 3; ++i)
            assert(raw[row + 1 + i] == rgb[std::size_t{y} * w * 3 + i]);
    }

    // Size mismatches and unsupported channel counts are refused.
    assert(exporter::encodePng(rgb, w, h + 1, 3).empty());
    assert(exporter::encodePng(rgb, w, h, 4).empty());
}

}  // namespace

void test_export() {
    test_outline_forge_json_matches_the_shadowscan_schema();
    test_dxf_is_millimetres_in_cad_axes();
    test_png_decodes_to_the_same_pixels();
    std::puts("test_export: OK");
}
