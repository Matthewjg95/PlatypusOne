#include "platypus/export/OutlineExport.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <string_view>
#include <system_error>
#include <vector>

namespace platypus::exporter {

namespace {

/// Locale-independent fixed-point formatting: a comma decimal separator in a
/// DXF or JSON file silently corrupts every coordinate.
void appendNumber(std::string& out, double value, int decimals) {
    char buffer[64];
    const auto result =
        std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed, decimals);
    if (result.ec == std::errc{}) out.append(buffer, result.ptr);
}

std::vector<const std::vector<geometry::Vec2>*> loopsOf(const geometry::Outline2& outline) {
    std::vector<const std::vector<geometry::Vec2>*> loops;
    if (outline.outer.size() >= 3) loops.push_back(&outline.outer);
    for (const auto& hole : outline.holes)
        if (hole.size() >= 3) loops.push_back(&hole);
    return loops;
}

/// One DXF group: code line, then value line.
void group(std::string& out, int code, std::string_view value) {
    out += std::to_string(code);
    out += '\n';
    out += value;
    out += '\n';
}

void groupNumber(std::string& out, int code, double value) {
    out += std::to_string(code);
    out += '\n';
    appendNumber(out, value, 4);
    out += '\n';
}

}  // namespace

std::string outlineForgeJson(const geometry::Outline2& outlinePx, double mmPerPx) {
    std::string out = R"({"format":"shadowscan-outline","units":"px","scale_mm_per_unit":)";
    appendNumber(out, mmPerPx, 6);
    out += R"(,"outlines":[)";
    bool firstLoop = true;
    for (const auto* loop : loopsOf(outlinePx)) {
        if (!firstLoop) out += ',';
        firstLoop = false;
        out += '[';
        for (std::size_t i = 0; i < loop->size(); ++i) {
            if (i) out += ',';
            out += '[';
            appendNumber(out, (*loop)[i].x, 2);
            out += ',';
            appendNumber(out, (*loop)[i].y, 2);
            out += ']';
        }
        out += ']';
    }
    out += "]}\n";
    return out;
}

std::string outlineDxf(const geometry::Outline2& outlinePx, double mmPerPx) {
    // Origin at the bounding-box minimum, y flipped: image rows grow downward,
    // CAD y grows upward, so the part is not mirrored when imported.
    float minX = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    for (const auto* loop : loopsOf(outlinePx))
        for (const auto& p : *loop) {
            minX = std::min(minX, p.x);
            maxY = std::max(maxY, p.y);
        }

    std::string out;
    group(out, 0, "SECTION");
    group(out, 2, "HEADER");
    group(out, 9, "$ACADVER");
    group(out, 1, "AC1009");  // R12: the most widely importable DXF
    group(out, 9, "$INSUNITS");
    group(out, 70, "4");  // millimetres (read by KiCad; ignored harmlessly by R12 readers)
    group(out, 0, "ENDSEC");
    group(out, 0, "SECTION");
    group(out, 2, "ENTITIES");

    bool outer = true;
    for (const auto* loop : loopsOf(outlinePx)) {
        const std::string_view layer = outer ? "OUTLINE" : "HOLES";
        outer = false;
        group(out, 0, "POLYLINE");
        group(out, 8, layer);
        group(out, 66, "1");  // vertices follow
        group(out, 70, "1");  // closed
        groupNumber(out, 10, 0.0);
        groupNumber(out, 20, 0.0);
        groupNumber(out, 30, 0.0);
        for (const auto& p : *loop) {
            group(out, 0, "VERTEX");
            group(out, 8, layer);
            groupNumber(out, 10, (p.x - minX) * mmPerPx);
            groupNumber(out, 20, (maxY - p.y) * mmPerPx);
            groupNumber(out, 30, 0.0);
        }
        group(out, 0, "SEQEND");
        group(out, 8, layer);
    }
    group(out, 0, "ENDSEC");
    group(out, 0, "EOF");
    return out;
}

}  // namespace platypus::exporter
