// The kiosk's depth view: live camera, the VL53L8CX zone grid, and the grid
// laid over the camera image. A demo and bench instrument, not a measurement:
// the overlay assumes co-axial sensors with datasheet fields of view, and the
// screen says so.
#include "KioskCore.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace platypus::kiosk {

namespace {

namespace tof = hal::tof;

constexpr double kPi = 3.14159265358979323846;

std::string number(const char* fmt, double v) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), fmt, v);
    return buffer;
}

/// Black on bright cells, white on dark ones.
Color textOn(Color c) {
    return (299 * c.r + 587 * c.g + 114 * c.b) / 1000 > 128 ? Color{0, 0, 0} : Color{255, 255, 255};
}

void centredText(renderer::Renderer& r, const Rect& box, const std::string& text, Color c,
                 std::int32_t scale) {
    const auto w = renderer::Renderer::textWidth(text, scale);
    r.drawText(box.x + (box.w - w) / 2, box.y + (box.h - renderer::Renderer::textHeight(scale)) / 2,
               text, c, scale);
}

/// The square heat map inside the ToF pane.
Rect gridRect(const Rect& pane, std::int32_t side) {
    const std::int32_t cell = std::min(pane.w, pane.h) / std::max(1, side);
    const std::int32_t size = cell * side;
    return {pane.x + (pane.w - size) / 2, pane.y + (pane.h - size) / 2, size, size};
}

}  // namespace

DepthLayout computeDepthLayout(const hal::DisplayInfo& info) {
    return computeDepthLayout(Rect{0, 0, info.width, info.height}, true);
}

DepthLayout computeDepthLayout(const Rect& area, bool withBack) {
    const std::int32_t w = area.w, h = area.h;
    const std::int32_t ref = area.y + area.h;  // the display height, for text size
    const std::int32_t m = std::max(8, ref / 30);
    DepthLayout l;
    l.area = area;
    l.textScale = std::max(1, ref / 240);
    const std::int32_t s = l.textScale;
    const std::int32_t labelY = m + renderer::Renderer::textHeight(s) + m / 2;
    const std::int32_t paneY = labelY + renderer::Renderer::textHeight(s) + 4;
    // Camera panes 4:3 either side of a square grid, filling the width:
    // 2 * (4/3) t + t = available, so t = 3 * available / 11.
    const std::int32_t t = std::min(3 * (w - 4 * m) / 11, h / 2);
    const std::int32_t c = t * 4 / 3;
    const std::int32_t x0 = (w - (2 * c + t + 2 * m)) / 2;
    l.camera = {x0, paneY, c, t};
    l.tof = {x0 + c + m, paneY, t, t};
    l.fused = {x0 + c + t + 2 * m, paneY, c, t};
    l.legend = {l.tof.x, paneY + t + m / 2, l.tof.w, std::max(6, h / 48)};
    l.infoY = l.legend.y + l.legend.h + renderer::Renderer::textHeight(s) + m;
    const std::int32_t bh = ref / 9;
    const std::int32_t bw = c * 3 / 4;
    if (withBack) l.back = {w - m - bw, h - m - bh, bw, bh};
    // Everything above was laid out from the area's corner.
    for (Rect* r : {&l.camera, &l.tof, &l.fused, &l.legend, &l.back}) {
        if (r->w == 0) continue;
        r->x += area.x;
        r->y += area.y;
    }
    l.infoY += area.y;
    return l;
}

Footprint tofFootprint(const FieldsOfView& fov) {
    const auto half = [](double deg) { return std::tan(deg * kPi / 360.0); };
    return {std::clamp(half(fov.tofDeg) / half(fov.cameraHDeg), 0.0, 1.0),
            std::clamp(half(fov.tofDeg) / half(fov.cameraVDeg), 0.0, 1.0)};
}

std::optional<DepthRange> depthRange(const tof::Frame& frame) {
    double lo = 1e9, hi = -1e9;
    for (const auto& z : frame.zones) {
        if (!tof::isValid(z.status)) continue;
        lo = std::min(lo, static_cast<double>(z.distanceMm));
        hi = std::max(hi, static_cast<double>(z.distanceMm));
    }
    if (lo > hi) return std::nullopt;
    if (hi - lo < 40.0) {
        const double mid = 0.5 * (lo + hi);
        lo = mid - 20.0;
        hi = mid + 20.0;
    }
    return DepthRange{lo, hi};
}

Color depthColour(double mm, const DepthRange& range) {
    // viridis stops, near (t = 0) to far (t = 1)
    static constexpr std::array<Color, 5> kStops{
        {{253, 231, 37}, {94, 201, 98}, {33, 145, 140}, {59, 82, 139}, {68, 1, 84}}};
    const double span = std::max(1e-6, range.farMm - range.nearMm);
    const double t = std::clamp((mm - range.nearMm) / span, 0.0, 1.0) * (kStops.size() - 1);
    const auto i = std::min(static_cast<std::size_t>(t), kStops.size() - 2);
    const double f = t - static_cast<double>(i);
    const auto mix = [&](std::uint8_t a, std::uint8_t b) {
        return static_cast<std::uint8_t>(std::lround(a + (b - a) * f));
    };
    return {mix(kStops[i].r, kStops[i + 1].r), mix(kStops[i].g, kStops[i + 1].g),
            mix(kStops[i].b, kStops[i + 1].b)};
}

std::vector<std::uint8_t> fuseDepth(const std::vector<std::uint8_t>& rgb, std::int32_t camW,
                                    std::int32_t camH, const tof::Frame& frame,
                                    tof::Orientation orientation, const Footprint& footprint,
                                    const DepthRange& range) {
    std::vector<std::uint8_t> out = rgb;
    if (out.size() < static_cast<std::size_t>(camW) * camH * 3 || frame.side <= 0 ||
        frame.zones.size() != static_cast<std::size_t>(frame.side * frame.side))
        return out;
    const std::int32_t fw = static_cast<std::int32_t>(std::lround(footprint.w * camW));
    const std::int32_t fh = static_cast<std::int32_t>(std::lround(footprint.h * camH));
    const std::int32_t x0 = (camW - fw) / 2, y0 = (camH - fh) / 2;
    if (fw <= 0 || fh <= 0) return out;
    // Borders thick enough to survive the downscale into a ~250 px pane.
    const std::int32_t line = std::max(2, camW / 250);
    const Color border{20, 20, 20};
    for (std::int32_t y = y0; y < y0 + fh; ++y) {
        const std::int32_t row = (y - y0) * frame.side / fh;
        const bool hLine = (y - y0) * frame.side % fh < line * frame.side;
        auto* p = out.data() + (static_cast<std::size_t>(y) * camW + x0) * 3;
        for (std::int32_t x = x0; x < x0 + fw; ++x, p += 3) {
            const std::int32_t col = (x - x0) * frame.side / fw;
            const bool vLine = (x - x0) * frame.side % fw < line * frame.side;
            if (hLine || vLine || y >= y0 + fh - line || x >= x0 + fw - line) {
                p[0] = border.r;
                p[1] = border.g;
                p[2] = border.b;
                continue;
            }
            const auto& z = frame.zones[static_cast<std::size_t>(
                tof::zoneIndexAt(row, col, frame.side, orientation))];
            if (tof::isValid(z.status)) {
                const Color c = depthColour(z.distanceMm, range);
                p[0] = static_cast<std::uint8_t>((p[0] * 55 + c.r * 45) / 100);
                p[1] = static_cast<std::uint8_t>((p[1] * 55 + c.g * 45) / 100);
                p[2] = static_cast<std::uint8_t>((p[2] * 55 + c.b * 45) / 100);
            } else {
                p[0] = static_cast<std::uint8_t>(p[0] / 3);
                p[1] = static_cast<std::uint8_t>(p[1] / 3);
                p[2] = static_cast<std::uint8_t>(p[2] / 3);
            }
        }
    }
    return out;
}

void drawDepthScreen(renderer::Renderer& r, const DepthLayout& l,
                     const std::vector<std::uint8_t>& rgb, std::int32_t camW, std::int32_t camH,
                     const DepthState& st) {
    const std::int32_t s = l.textScale;
    const std::int32_t small = std::max(1, s - 1);
    const std::int32_t m = std::max(8, static_cast<std::int32_t>(r.displayInfo().height) / 30);
    r.clear(kBackground);

    // Title, and the stream's state where the eye lands first.
    const std::int32_t ax = l.area.x, ay = l.area.y;
    r.drawText(ax + m, ay + m, "DEPTH VIEW", kAccent, s);
    std::string live;
    Color liveColour = kWarn;
    if (!st.connected) {
        live = "TOF NOT CONNECTED";
    } else if (!st.frame) {
        live = "TOF STARTING";
        liveColour = kAccent;
    } else if (st.stale) {
        live = "TOF STALE";
    } else {
        live = "TOF LIVE " + number("%.1f", st.hz) + " HZ";
        liveColour = kGood;
    }
    r.drawText(ax + l.area.w - m - renderer::Renderer::textWidth(live, s), ay + m, live, liveColour,
               s);

    const std::int32_t labelY = l.camera.y - renderer::Renderer::textHeight(s) - 4;
    r.drawText(l.camera.x, labelY, "CAMERA", kMuted, s);
    r.drawText(l.tof.x, labelY, "TOF MM " + std::string(tof::to_string(st.orientation)), kMuted, s);
    r.drawText(l.fused.x, labelY, "CAMERA + TOF", kMuted, s);

    // 1. camera
    if (!rgb.empty())
        r.drawImage(l.camera, rgb, camW, camH, 3);
    else
        centredText(r, l.camera, "NO CAMERA", kWarn, s);
    r.drawRect(l.camera, kMuted);

    // 2. ToF grid
    const auto range = st.frame ? depthRange(*st.frame) : std::nullopt;
    if (st.frame && st.frame->side > 0) {
        const auto& f = *st.frame;
        const Rect g = gridRect(l.tof, f.side);
        const std::int32_t cell = g.w / f.side;
        for (std::int32_t row = 0; row < f.side; ++row) {
            for (std::int32_t col = 0; col < f.side; ++col) {
                const auto& z = f.zones[static_cast<std::size_t>(
                    tof::zoneIndexAt(row, col, f.side, st.orientation))];
                const Rect c{g.x + col * cell, g.y + row * cell, cell - 1, cell - 1};
                if (tof::isValid(z.status) && range) {
                    const Color fill = depthColour(z.distanceMm, *range);
                    r.fillRect(c, fill);
                    centredText(r, c, std::to_string(z.distanceMm), textOn(fill), 1);
                } else {
                    r.fillRect(c, kPanel);
                    centredText(r, c, std::to_string(z.status), kMuted, 1);
                }
            }
        }
        if (st.stale) r.drawRect(g, kWarn);
    } else {
        centredText(r, l.tof, st.connected ? "WAITING FOR FRAMES" : "NO TOF", kMuted, s);
    }
    r.drawRect(l.tof, kMuted);

    // Colour scale under the grid.
    if (range) {
        for (std::int32_t x = 0; x < l.legend.w; ++x) {
            const double mm =
                range->nearMm + (range->farMm - range->nearMm) * x / std::max(1, l.legend.w - 1);
            r.fillRect({l.legend.x + x, l.legend.y, 1, l.legend.h}, depthColour(mm, *range));
        }
        const std::string lo = number("%.0f", range->nearMm);
        const std::string hi = number("%.0f", range->farMm);
        const std::int32_t ty = l.legend.y + l.legend.h + 2;
        r.drawText(l.legend.x, ty, lo, kMuted, s);
        r.drawText(l.legend.x + l.legend.w - renderer::Renderer::textWidth(hi, s), ty, hi, kMuted,
                   s);
    }

    // 3. camera + ToF
    if (!rgb.empty() && st.frame && range) {
        const auto fused =
            fuseDepth(rgb, camW, camH, *st.frame, st.orientation, tofFootprint(st.fov), *range);
        r.drawImage(l.fused, fused, camW, camH, 3);
    } else if (!rgb.empty()) {
        r.drawImage(l.fused, rgb, camW, camH, 3);
    } else {
        centredText(r, l.fused, "NO CAMERA", kWarn, s);
    }
    r.drawRect(l.fused, kMuted);

    // Readings and the honest caveats.
    const bool hasBack = l.back.w > 0;
    const std::int32_t tx = ax + m;
    const std::int32_t tw = (hasBack ? l.back.x : ax + l.area.w) - tx - m;
    const std::int32_t textBottom = hasBack ? l.back.y + l.back.h : ay + l.area.h - m;
    std::int32_t y = l.infoY;
    const auto line = [&](const std::string& text, Color c, std::int32_t scale) {
        if (y < textBottom - renderer::Renderer::textHeight(scale))
            y = drawWrapped(r, tx, y, tw, text, c, scale);
    };
    if (st.frame) {
        const auto& f = *st.frame;
        std::int32_t valid = 0;
        for (const auto& z : f.zones)
            valid += tof::isValid(z.status) ? 1 : 0;
        const auto centre = tof::centralMedianMm(f);
        line(centre ? "CENTRE " + number("%.0f", *centre) + " MM" : std::string("CENTRE --"),
             centre ? kText : kWarn, s + 1);
        line("VALID " + std::to_string(valid) + "/" + std::to_string(f.zones.size()) + " | " +
                 std::to_string(f.temperatureC) + " C | FRAME " + std::to_string(f.seq),
             kText, s);
    } else if (!st.connected && !st.lastError.empty()) {
        line(st.lastError, kWarn, s);
    }
    if (!st.lastError.empty() && st.connected) line("TOF: " + st.lastError, kWarn, s);
    y += s * 3;
    line("OVERLAY: NOMINAL FOV, NOT CALIBRATED", kMuted, s);
    line("GREY = NOT VALID (ST STATUS)", kMuted, s);
    line("TAP GRID OR OVERLAY TO FLIP", kMuted, s);
    line("frames " + std::to_string(st.frames) + "  rejected " + std::to_string(st.rejects) +
             "  ToF " + number("%.0f", st.fov.tofDeg) + " deg in camera " +
             number("%.0f", st.fov.cameraHDeg) + "x" + number("%.0f", st.fov.cameraVDeg),
         kMuted, small);

    if (hasBack) {
        r.fillRect(l.back, kPanel);
        r.drawRect(l.back, kAccent);
        centredText(r, l.back, st.backLabel, kAccent, s);
    }
}

void drawCaptureReview(renderer::Renderer& r, const Rect& area, const CaptureOutcome& out,
                       tof::Orientation orientation, const FieldsOfView& fov) {
    const std::int32_t ref = area.y + area.h;
    const std::int32_t s = std::max(1, ref / 240);
    const std::int32_t m = std::max(8, ref / 30);
    const std::int32_t x0 = area.x + m, y0 = area.y + m;
    r.clear(kBackground);

    // What this capture is, in the colour of what it achieved.
    const std::string head = out.record ? out.status : "capture failed: " + out.status;
    r.drawText(x0, y0, head, out.measured ? kGood : kWarn, s);

    const std::int32_t top = y0 + renderer::Renderer::textHeight(s) + m / 2;
    const std::int32_t pw = std::min(area.w * 56 / 100 - m, (area.h * 58 / 100) * 4 / 3);
    const std::int32_t ph = pw * 3 / 4;
    const Rect photo{x0, top, pw, ph};
    const std::int32_t gs = std::min(area.x + area.w - m - (photo.x + pw + m), ph);
    const Rect gridBox{photo.x + pw + m, top, gs, gs};

    const auto range = out.tofFrame ? depthRange(*out.tofFrame) : std::nullopt;
    if (out.thumbnail && !out.thumbnail->pixels.empty()) {
        const auto& t = *out.thumbnail;
        if (out.tofFrame && range)
            r.drawImage(photo,
                        fuseDepth(t.pixels, t.width, t.height, *out.tofFrame, orientation,
                                  tofFootprint(fov), *range),
                        t.width, t.height, 3);
        else
            r.drawImage(photo, t.pixels, t.width, t.height, 3);
    } else {
        centredText(r, photo, "NO IMAGE", kWarn, s);
    }
    r.drawRect(photo, kMuted);

    // The ToF grid, and its centre reading large beneath it.
    if (out.tofFrame && out.tofFrame->side > 0) {
        const auto& f = *out.tofFrame;
        const Rect g = gridRect(gridBox, f.side);
        const std::int32_t cell = g.w / f.side;
        for (std::int32_t row = 0; row < f.side; ++row)
            for (std::int32_t col = 0; col < f.side; ++col) {
                const auto& z = f.zones[static_cast<std::size_t>(
                    tof::zoneIndexAt(row, col, f.side, orientation))];
                const Rect c{g.x + col * cell, g.y + row * cell, cell - 1, cell - 1};
                if (tof::isValid(z.status) && range)
                    r.fillRect(c, depthColour(z.distanceMm, *range));
                else
                    r.fillRect(c, kPanel);
            }
    } else {
        r.drawRect(gridBox, kMuted);
        centredText(r, gridBox, "NO TOF", kMuted, s);
    }
    const auto centre = out.tofFrame ? tof::centralMedianMm(*out.tofFrame) : std::nullopt;
    const std::string big = centre ? "TOF " + number("%.0f", *centre) + " MM" : "TOF --";
    const std::int32_t bigY = gridBox.y + gridBox.h + m / 2;
    r.drawText(gridBox.x, bigY, big, out.tofOk ? kText : kWarn, s + 1);

    // What the analysis got, the capture's quality, and the ToF's state,
    // below both the photo and the large ToF reading.
    std::int32_t y = std::max(top + ph, bigY + renderer::Renderer::textHeight(s + 1)) + m / 2;
    const std::int32_t tw = area.x + area.w - m - x0;
    const auto line = [&](const std::string& text, Color c) {
        if (y < area.y + area.h - renderer::Renderer::textHeight(s))
            y = drawWrapped(r, x0, y, tw, text, c, s);
    };
    if (out.analysis) {
        line("L " + number("%.2f", out.analysis->subjectLengthMm) + " MM   W " +
                 number("%.2f", out.analysis->subjectWidthMm) + " MM   tilt ~" +
                 number("%.0f", out.analysis->cameraTiltDeg) + " deg",
             kText);
    } else if (out.inspection) {
        const auto& in = *out.inspection;
        std::string t;
        if (in.subject) {
            t = "part " + number("%.0f", in.subject->lengthPx) + " x " +
                number("%.0f", in.subject->widthPx) + " px";
            if (in.bestEffortMmPerPixel > 0.0)
                t += " = ~" + number("%.1f", in.subject->lengthPx * in.bestEffortMmPerPixel) +
                     " x " + number("%.1f", in.subject->widthPx * in.bestEffortMmPerPixel) +
                     " mm (conf " + number("%.2f", in.bestEffortConfidence) + ")";
        } else {
            t = "no part found";
        }
        line(t, kText);
        if (!in.failedChecks.empty()) line(in.failedChecks.front(), kMuted);
    }
    if (out.inspection) {
        const auto& q = out.inspection->quality;
        line("contrast " + number("%.2f", q.contrast) + "  median " + number("%.0f", q.lumaP50) +
                 "  sharp " + number("%.1f", q.sharpnessIndex) + "  clipped " +
                 number("%.1f", 100.0 * q.clippedFraction) + "%",
             kMuted);
    }
    if (!out.tofStatus.empty()) line(out.tofStatus, out.tofOk ? kMuted : kWarn);
    const std::string tap = "TAP TO CONTINUE";
    r.drawText(area.x + area.w - m - renderer::Renderer::textWidth(tap, s),
               area.y + area.h - m - renderer::Renderer::textHeight(s), tap, kAccent, s);
}

}  // namespace platypus::kiosk
