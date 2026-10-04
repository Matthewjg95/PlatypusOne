#include "KioskCore.hpp"

#include <platypus/ai/FastenerClassifier.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace platypus::kiosk {

namespace {

std::string fixed2(double v) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f", v);
    return buffer;
}

/// A button: filled rect with a centred label sized to fit.
void drawButton(renderer::Renderer& r, const Rect& b, const std::string& label, Color fill,
                Color text, std::int32_t maxScale) {
    r.fillRect(b, fill);
    const std::int32_t unit = renderer::Renderer::textWidth(label, 1);
    const std::int32_t scale = std::max(1, std::min(maxScale, (b.w - 8) / std::max(1, unit)));
    const auto lw = renderer::Renderer::textWidth(label, scale);
    r.drawText(b.x + (b.w - lw) / 2, b.y + (b.h - renderer::Renderer::textHeight(scale)) / 2, label,
               text, scale);
}

}  // namespace

std::vector<std::uint8_t> yuyvToRgb(const hal::Frame& frame) {
    const auto& m = frame.mode();
    const auto px = frame.pixels();
    std::vector<std::uint8_t> rgb(std::size_t{m.width} * m.height * 3);
    if (px.size() < std::size_t{m.width} * m.height * 2) return {};
    const auto clamp8 = [](int v) { return static_cast<std::uint8_t>(std::clamp(v, 0, 255)); };
    for (std::size_t i = 0, o = 0; i + 3 < px.size() && o + 5 < rgb.size(); i += 4, o += 6) {
        const int y0 = std::to_integer<int>(px[i]) - 16;
        const int u = std::to_integer<int>(px[i + 1]) - 128;
        const int y1 = std::to_integer<int>(px[i + 2]) - 16;
        const int v = std::to_integer<int>(px[i + 3]) - 128;
        for (int k = 0; k < 2; ++k) {
            const int c = 298 * (k == 0 ? y0 : y1);
            const auto at = o + 3 * static_cast<std::size_t>(k);
            rgb[at + 0] = clamp8((c + 409 * v + 128) >> 8);
            rgb[at + 1] = clamp8((c - 100 * u - 208 * v + 128) >> 8);
            rgb[at + 2] = clamp8((c + 516 * u + 128) >> 8);
        }
    }
    return rgb;
}

Layout computeLayout(const hal::DisplayInfo& info, std::int32_t camW, std::int32_t camH) {
    const std::int32_t w = info.width, h = info.height;
    const std::int32_t margin = std::max(8, h / 30);
    Layout l;
    l.textScale = std::max(1, h / 240);
    // The preview gets at most 60 % of the width: the session panel needs
    // room for readable text (~24 characters at scale 2 on the 800x480 panel).
    std::int32_t ph = h - 2 * margin;
    std::int32_t pw = ph * camW / std::max(1, camH);
    const std::int32_t maxPw = w * 60 / 100;
    if (pw > maxPw) {
        pw = maxPw;
        ph = pw * camH / std::max(1, camW);
    }
    l.preview = {margin, (h - ph) / 2, pw, ph};
    const std::int32_t px = margin + pw + margin;
    l.panel = {px, margin, w - px - margin, h - 2 * margin};
    const std::int32_t finishH = h / 9;
    const std::int32_t captureH = h / 5;
    l.finish = {l.panel.x + margin / 2, l.panel.y + l.panel.h - finishH - margin / 2,
                l.panel.w - margin, finishH};
    l.capture = {l.panel.x + margin / 2, l.finish.y - margin / 2 - captureH, l.panel.w - margin,
                 captureH};
    return l;
}

bool inside(const Rect& r, std::int32_t x, std::int32_t y) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

std::int32_t drawWrapped(renderer::Renderer& r, std::int32_t x, std::int32_t y,
                         std::int32_t maxWidth, const std::string& text, Color colour,
                         std::int32_t scale) {
    std::string line, word;
    const auto flush = [&] {
        if (line.empty()) return;
        r.drawText(x, y, line, colour, scale);
        y += renderer::Renderer::textHeight(scale) + 3 * scale;
        line.clear();
    };
    for (std::size_t i = 0; i <= text.size(); ++i) {
        const char c = i < text.size() ? text[i] : ' ';
        if (c != ' ') {
            word += c;
            continue;
        }
        if (word.empty()) continue;
        const auto candidate = line.empty() ? word : line + " " + word;
        if (renderer::Renderer::textWidth(candidate, scale) > maxWidth) flush();
        line = line.empty() ? word : line + " " + word;
        word.clear();
    }
    flush();
    return y;
}

void drawPreviewScreen(renderer::Renderer& r, const Layout& l, const std::vector<std::uint8_t>& rgb,
                       std::int32_t camW, std::int32_t camH, const PanelState& panel) {
    r.clear(kBackground);
    if (!rgb.empty()) r.drawImage(l.preview, rgb, camW, camH, 3);
    r.drawRect(l.preview, kMuted);
    if (rgb.empty() && panel.noCamera) {
        // Say it where the operator is looking: in the empty preview.
        const std::int32_t big = l.textScale + 1;
        const std::string line1 = "CAMERA NOT CONNECTED";
        const std::string line2 = "Plug the USB webcam into the hub";
        const std::int32_t h1 = renderer::Renderer::textHeight(big);
        const std::int32_t cy = l.preview.y + l.preview.h / 2 - h1;
        r.drawText(l.preview.x + (l.preview.w - renderer::Renderer::textWidth(line1, big)) / 2, cy,
                   line1, kWarn, big);
        r.drawText(
            l.preview.x + (l.preview.w - renderer::Renderer::textWidth(line2, l.textScale)) / 2,
            cy + h1 + 6 * l.textScale, line2, kText, l.textScale);
    }
    r.fillRect(l.panel, kPanel);

    const std::int32_t s = l.textScale;
    const std::int32_t small = std::max(1, s - 1);
    const std::int32_t tx = l.panel.x + 6 * s;
    const std::int32_t tw = l.panel.w - 12 * s;
    // Text stops above the buttons, whatever the content.
    const std::int32_t limit = l.capture.y - renderer::Renderer::textHeight(s) - 2 * s;
    std::int32_t y = l.panel.y + 6 * s;
    const auto section = [&](const std::string& text, Color colour, std::int32_t scale) {
        if (y < limit) y = drawWrapped(r, tx, y, tw, text, colour, scale);
    };

    section("ENGINEERING SCOUT", kAccent, s);
    if (!panel.sessionId.empty()) section("session " + panel.sessionId, kMuted, small);
    y += 3 * s;

    // The panel fits ~20 characters at readable size on the 800x480 panel, so
    // lines are kept short: full labels live on the summary screen.
    if (panel.guidance) {
        const auto& g = *panel.guidance;
        if (g.measured > 0) {
            section(std::to_string(g.accepted) + " of " + std::to_string(g.required) + " agree",
                    g.accepted >= g.required ? kGood : kText, s);
            for (const auto& m : g.summary) {
                if (m.n == 0) continue;
                const char initial =
                    m.label.empty()
                        ? '?'
                        : static_cast<char>(std::toupper(static_cast<unsigned char>(m.label[0])));
                section(std::string(1, initial) + " " + fixed2(m.mean) + " +-" + fixed2(m.spread) +
                            " " + m.unit,
                        kText, s);
            }
            y += 3 * s;
        }
        section(g.instruction, g.modelSatisfied ? kGood : kAccent, s);
        if (g.stage == session::Stage::Complete)
            for (const auto& q : g.openQuestions)
                if (!q.analyzerInBuild) section("open: " + q.field, kMuted, s);
    }
    if (!panel.status.empty()) {
        y += 3 * s;
        section(panel.status, panel.statusColour, s);
    }

    const char* captureLabel = panel.noCamera ? "NO CAMERA" : panel.busy ? "WAIT" : "CAPTURE";
    drawButton(r, l.capture, captureLabel, panel.noCamera || panel.busy ? kMuted : kAccent,
               kBackground, s + 1);
    const bool anyMeasured = panel.guidance && panel.guidance->measured > 0;
    const bool satisfied = panel.guidance && panel.guidance->modelSatisfied;
    drawButton(r, l.finish, "FINISH",
               satisfied     ? kGood
               : anyMeasured ? kMuted
                             : kPanel,
               anyMeasured ? kBackground : kMuted, s);
    if (!anyMeasured) r.drawRect(l.finish, kMuted);
}

namespace {

/// The exported silhouette, fitted into `box` with its aspect kept and y up
/// (as the DXF has it): the operator sees what went to CAD, not just a list.
void drawOutline(renderer::Renderer& r, const renderer::Rect& box, const geometry::Outline2& o,
                 std::int32_t s) {
    r.drawRect(box, kMuted);
    if (o.empty()) return;
    double minX = o.outer[0].x, maxX = minX, minY = o.outer[0].y, maxY = minY;
    for (const auto& p : o.outer) {
        minX = std::min(minX, static_cast<double>(p.x));
        maxX = std::max(maxX, static_cast<double>(p.x));
        minY = std::min(minY, static_cast<double>(p.y));
        maxY = std::max(maxY, static_cast<double>(p.y));
    }
    const double pad = 8.0 * s;
    const double k = std::min((box.w - 2 * pad) / std::max(1.0, maxX - minX),
                              (box.h - 2 * pad) / std::max(1.0, maxY - minY));
    const double ox = box.x + (box.w - (maxX - minX) * k) / 2;
    const double oy = box.y + (box.h - (maxY - minY) * k) / 2;
    // Image rows grow downward, which is what the screen wants too: the
    // silhouette appears as the part lay on the sheet.
    const auto loop = [&](const std::vector<geometry::Vec2>& pts, Color c) {
        for (std::size_t i = 0; i < pts.size(); ++i) {
            const auto& p = pts[i];
            const auto& q = pts[(i + 1) % pts.size()];
            r.drawLine(static_cast<std::int32_t>(ox + (p.x - minX) * k),
                       static_cast<std::int32_t>(oy + (p.y - minY) * k),
                       static_cast<std::int32_t>(ox + (q.x - minX) * k),
                       static_cast<std::int32_t>(oy + (q.y - minY) * k), c);
        }
    };
    loop(o.outer, kAccent);
    for (const auto& h : o.holes)
        loop(h, kAccent);
}

}  // namespace

void drawSessionSummary(renderer::Renderer& r, const session::ObjectSession& s,
                        const session::FinishResult& result, session::CloseReason reason) {
    const auto info = r.displayInfo();
    const std::int32_t scale = std::max(1, info.height / 240);
    const std::int32_t margin = std::max(8, info.height / 30);
    const auto g = s.guidance();
    r.clear(kBackground);

    // Left: what was concluded. Right: the silhouette that was exported.
    const auto rep = s.representativeCapture();
    const bool haveOutline = rep && !s.captures()[*rep].outlinePx.empty();
    const std::int32_t outlineW = haveOutline ? info.width * 2 / 5 : 0;
    const std::int32_t w = info.width - 2 * margin - (haveOutline ? outlineW + margin : 0);

    std::int32_t y = margin;
    const bool satisfied = reason == session::CloseReason::ModelSatisfied;
    y = drawWrapped(r, margin, y, w,
                    "SESSION " + s.id() + (satisfied ? " COMPLETE" : " CLOSED EARLY"),
                    satisfied ? kGood : kAccent, scale);
    y = drawWrapped(r, margin, y, w,
                    satisfied ? std::to_string(g.accepted) + " captures agree within " +
                                    fixed2(s.profile().repeatToleranceMm) + " mm"
                              : "not yet repeatable - figures are provisional",
                    kMuted, scale);
    y += 4 * scale;

    for (const auto& m : g.summary)
        if (m.n > 0)
            y = drawWrapped(
                r, margin, y, w,
                m.label + " " + fixed2(m.mean) + " " + m.unit + " +-" + fixed2(m.spread), kText,
                scale + 1);
    std::string identity;
    for (const auto& c : g.consensus)
        if (c) identity += (identity.empty() ? "" : " ") + *c;
    if (!identity.empty()) y = drawWrapped(r, margin, y, w, identity + " (inferred)", kText, scale);
    y += 4 * scale;
    for (const auto& q : g.openQuestions)
        y = drawWrapped(r, margin, y, w, "open: " + q.field, kMuted, scale);
    y += 4 * scale;

    if (!result.ok()) {
        y = drawWrapped(r, margin, y, w, "EXPORT FAILED: " + result.error, kWarn, scale);
    } else {
        // The directory's last two parts are what the operator looks for; the
        // full path is in the log and in summary.md.
        const auto dir = result.directory.parent_path().filename() / result.directory.filename();
        y = drawWrapped(r, margin, y, w, "saved " + dir.generic_string(), kAccent, scale);
        std::string names;
        for (const auto& a : result.artifacts)
            names += (names.empty() ? "" : "  ") + a.path;
        y = drawWrapped(r, margin, y, w, names, kText, scale);
    }

    const std::int32_t footerY = info.height - margin - renderer::Renderer::textHeight(scale);
    if (haveOutline) {
        const renderer::Rect box{info.width - margin - outlineW, margin, outlineW,
                                 footerY - 2 * margin};
        const std::string label = "OUTLINE " + s.captures()[*rep].observationId;
        r.drawText(box.x, box.y, label, kMuted, scale);
        const std::int32_t top = renderer::Renderer::textHeight(scale) + 4 * scale;
        drawOutline(r, {box.x, box.y + top, box.w, box.h - top}, s.captures()[*rep].outlinePx,
                    scale);
    }

    const std::string footer = "TAP TO START A NEW SESSION";
    const auto fw = renderer::Renderer::textWidth(footer, scale);
    r.drawText((info.width - fw) / 2, footerY, footer, kAccent, scale);
}

std::string guidanceFor(vision::AnalyzeError error) {
    switch (error) {
        case vision::AnalyzeError::NoReferenceTarget:
            return "No 20 mm square found. Put the whole square in view.";
        case vision::AnalyzeError::ReferenceAmbiguous:
            return "Two things look like the square. Leave only one.";
        case vision::AnalyzeError::NoSubject:
            return "No part found beside the square. Keep it fully in frame.";
        case vision::AnalyzeError::UnsupportedFormat:
            return "Camera mode cannot be measured.";
        default:
            return "Could not read the scene. Check light and focus.";
    }
}

CaptureOutcome captureOnce(hal::ICamera& camera, observation::CaptureService& service,
                           const vision::CalibrationSpec& spec, const std::string& device,
                           const std::string& identity) {
    observation::CaptureConfig config;
    config.observationId = service.nextObservationId();
    config.timestampUtc = observation::CaptureService::currentUtcTimestamp();
    config.source = {{"app", "scout_kiosk"}, {"camera", device}, {"camera_identity", identity}};

    CaptureOutcome out;
    auto sceneError = vision::AnalyzeError::None;
    config.enrich = [&](const hal::Frame& frame, observation::EngineeringObservation& record) {
        const auto& m = frame.mode();
        out.thumbnail = apps::CardImage{m.width, m.height, 3, yuyvToRgb(frame)};
        const auto analyzed = vision::analyzeFrame(frame, spec);
        if (!analyzed.ok()) {
            sceneError = analyzed.error;
            return;
        }
        vision::appendEvidence(record, *analyzed.analysis, spec, "source-image");
        ai::appendClassification(record, ai::classify(*analyzed.analysis));
        out.analysis = *analyzed.analysis;
        out.measured = true;
    };

    const auto result = service.capture(camera, config);
    if (!result) {
        out.status = "capture failed: " + std::string(hal::to_string(result.error()));
        out.statusColour = kWarn;
        return out;
    }
    out.record = result.value().record;
    if (out.measured) {
        out.status = "saved " + out.record->observationId;
        out.statusColour = kGood;
        if (out.analysis && out.analysis->cameraTiltDeg >= vision::kTiltWarningDeg) {
            // Hand-held reality: say so before anyone trusts the number.
            char tilt[64];
            std::snprintf(tilt, sizeof(tilt), "; tilted ~%.0f deg: aim straight down",
                          out.analysis->cameraTiltDeg);
            out.status += tilt;
            out.statusColour = kWarn;
        }
    } else {
        // The capture-only record is still written — honest evidence that the
        // attempt happened. The operator is told what to change through the
        // session's guidance, which carries the refusal; the status line only
        // records that the attempt was kept.
        out.refusal = guidanceFor(sceneError);
        out.status = out.record->observationId + ": no measurement";
        out.statusColour = kWarn;
    }
    return out;
}

session::Capture sessionCapture(const CaptureOutcome& outcome,
                                const session::ObjectProfile& profile) {
    auto c = session::captureFrom(*outcome.record, profile, outcome.refusal);
    if (outcome.analysis) {
        c.outlinePx = outcome.analysis->subjectOutlinePx;
        c.mmPerPx = outcome.analysis->mmPerPixel;
    }
    if (outcome.thumbnail && !outcome.thumbnail->pixels.empty())
        c.preview = session::Image{static_cast<std::uint32_t>(outcome.thumbnail->width),
                                   static_cast<std::uint32_t>(outcome.thumbnail->height), 3,
                                   outcome.thumbnail->pixels};
    return c;
}

SceneCamera::SceneCamera() {
    setPose(0);
}

void SceneCamera::setPose(int pose) {
    // Poses mimic the operator shifting and rotating the part between
    // captures; rasterization differences give a realistic small spread.
    constexpr double kPi = 3.14159265358979;
    constexpr double pxPerMm = 4.0;
    struct Pose {
        double cx, cy, degrees;
    };
    constexpr Pose poses[] = {{400, 280, 45}, {380, 300, 30}, {420, 260, 60}, {400, 290, 38}};
    scene_ = hal::testing::SyntheticScene();
    scene_.addSquare(80, 80, static_cast<std::int32_t>(20.0 * pxPerMm));
    if (pose < 0) return;  // part removed: exercises the refused-scene path
    const auto& p = poses[static_cast<std::size_t>(pose) % std::size(poses)];
    // An M8 x 40 bolt: 8 mm shank, 13 mm across-flats head ~5 mm tall.
    scene_.addBolt(p.cx, p.cy, 40.0 * pxPerMm, 8.0 * pxPerMm, p.degrees * kPi / 180.0,
                   13.0 * pxPerMm, 5.2 * pxPerMm);
}

hal::Status SceneCamera::open(const hal::CameraMode&) {
    open_ = true;
    return {};
}

hal::Status SceneCamera::close() {
    open_ = false;
    return {};
}

hal::Result<hal::Frame> SceneCamera::capture(std::chrono::milliseconds) {
    if (!open_) return hal::Error::NotInitialized;
    return scene_.yuyvFrame();
}

hal::Status SceneCamera::startStream(std::function<void(const hal::Frame&)> onFrame) {
    onFrame(scene_.yuyvFrame());
    return {};
}

}  // namespace platypus::kiosk
