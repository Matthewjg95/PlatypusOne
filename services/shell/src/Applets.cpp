#include <platypus/hal/IDisplay.hpp>
#include <platypus/shell/Applets.hpp>
#include <platypus/shell/Shell.hpp>

#include <algorithm>
#include <utility>

namespace platypus::shell {

namespace {

using renderer::Color;
using renderer::Rect;
using Renderer = renderer::Renderer;

constexpr Color kBackground{12, 14, 18};
constexpr Color kText{230, 232, 236};
constexpr Color kMuted{140, 146, 158};
constexpr Color kAccent{255, 196, 0};
constexpr Color kGood{80, 200, 120};
constexpr Color kWarn{255, 110, 90};

Rect contentOf(appfw::AppContext& ctx) {
    if (ctx.contentArea.w > 0) return ctx.contentArea;
    const auto info = ctx.renderer.displayInfo();
    return {0, 0, info.width, info.height};
}

std::int32_t scaleOf(appfw::AppContext& ctx) {
    const auto info = ctx.renderer.displayInfo();
    return std::max(1, std::min<std::int32_t>(info.width, info.height) / 240);
}

/// Word-wrapped lines from (x, y) within width w; returns the y below.
std::int32_t paragraph(Renderer& r, std::int32_t x, std::int32_t y, std::int32_t w,
                       const std::string& text, Color c, std::int32_t s) {
    std::string line;
    std::size_t i = 0;
    const auto flush = [&] {
        if (line.empty()) return;
        r.drawText(x, y, line, c, s);
        y += Renderer::textHeight(s) + 3 * s;
        line.clear();
    };
    while (i <= text.size()) {
        const auto sp = text.find(' ', i);
        const std::string word =
            text.substr(i, sp == std::string::npos ? std::string::npos : sp - i);
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && Renderer::textWidth(candidate, s) > w) flush();
        line = line.empty() ? word : line + " " + word;
        if (sp == std::string::npos) break;
        i = sp + 1;
    }
    flush();
    return y;
}

template <std::size_t I>
std::unique_ptr<appfw::IApp> makePreview() {
    const auto all = standardManifests();
    return std::make_unique<PreviewApplet>(all[I], previewNotes(all[I].id));
}

std::unique_ptr<appfw::IApp> makeSystem() {
    for (auto m : standardManifests())
        if (m.id == ids::kSystem) return std::make_unique<SystemApplet>(std::move(m));
    return nullptr;
}

}  // namespace

std::vector<appfw::AppManifest> standardManifests() {
    const auto card = [](const char* id, const char* name, const char* tagline, const char* icon,
                         bool camera, bool sensors, bool preview) {
        appfw::AppManifest m;
        m.id = id;
        m.name = name;
        m.version = kShellVersion;
        m.requiresCamera = camera;
        m.requiresSensors = sensors;
        m.tagline = tagline;
        m.icon = icon;
        m.preview = preview;
        return m;
    };
    return {
        card(ids::kScout, "SCOUT", "Measure a part on the card; sessions export CAD.", "scout",
             true, false, true),
        card(ids::kDepth, "DEPTH", "Camera and ToF depth, side by side and together.", "depth",
             true, true, true),
        card(ids::kShadowScan, "SHADOWSCAN", "Silhouette to engineering sketch.", "shadowscan",
             true, false, true),
        card(ids::kRoomScan, "ROOM SCAN", "Walls, openings and objects from depth.", "roomscan",
             true, true, true),
        card(ids::kViewer3d, "3D VIEWER", "Inspect models and scans in 3D.", "viewer3d", false,
             false, true),
        card(ids::kSystem, "SYSTEM", "Camera, ToF, IMU, battery and build status.", "system", false,
             false, false),
    };
}

std::vector<std::string> previewNotes(const std::string& id) {
    if (id == ids::kScout)
        return {"Runs on the device: the kiosk's capture, session and CAD export flow.",
                "Needs the CSI camera. Moving into the shell is stage 2 of #51."};
    if (id == ids::kDepth)
        return {"Runs on the device: live camera, the VL53L8CX 8x8 grid and the overlay.",
                "Needs the camera and the ToF sketch (PR #49). Built on claude/kiosk-depth-view."};
    if (id == ids::kShadowScan)
        return {"Turns a part's silhouette into a reviewable engineering sketch.",
                "Consumes ShadowScan Mobile and Platypus Lab outputs; Lab owns the experiment "
                "(#37, #51)."};
    if (id == ids::kRoomScan)
        return {"Maps walls, openings and objects from paired camera and depth evidence.",
                "Consumes Platypus Lab's room-mapping contract; waits on ToF Gate E (#16, #51)."};
    if (id == ids::kViewer3d)
        return {"Views models and scans with the z-buffer renderer from the Tab5 lab build.",
                "Port tracked in #51."};
    return {"Not built yet (#51)."};
}

PreviewApplet::PreviewApplet(appfw::AppManifest manifest, std::vector<std::string> notes)
    : manifest_(std::move(manifest)), notes_(std::move(notes)) {}

void PreviewApplet::onFrame(appfw::AppContext& ctx, std::chrono::milliseconds) {
    auto& r = ctx.renderer;
    const Rect c = contentOf(ctx);
    const std::int32_t s = scaleOf(ctx);
    const std::int32_t m = std::max(8, c.h / 20);
    r.fillRect(c, kBackground);
    const std::int32_t icon = c.h * 45 / 100;
    drawAppIcon(r, {c.x + m, c.y + m, icon, icon}, manifest_.icon);
    const std::int32_t tx = c.x + 2 * m + icon;
    const std::int32_t tw = c.x + c.w - m - tx;
    std::int32_t y = c.y + m;
    r.drawText(tx, y, manifest_.name, kText, s + 2);
    y += Renderer::textHeight(s + 2) + 4 * s;
    y = paragraph(r, tx, y, tw, manifest_.tagline, kMuted, s);
    y += 4 * s;
    r.drawText(tx, y, "NOT AVAILABLE HERE YET", kAccent, s);
    y += Renderer::textHeight(s) + 6 * s;
    y = std::max(y, c.y + m + icon + m);
    for (const auto& n : notes_)
        y = paragraph(r, c.x + m, y, c.w - 2 * m, n, kText, s) + 2 * s;
}

SystemApplet::SystemApplet(appfw::AppManifest manifest) : manifest_(std::move(manifest)) {}

void SystemApplet::onFrame(appfw::AppContext& ctx, std::chrono::milliseconds) {
    auto& r = ctx.renderer;
    const Rect c = contentOf(ctx);
    const std::int32_t s = scaleOf(ctx);
    const std::int32_t m = std::max(8, c.h / 20);
    r.fillRect(c, kBackground);
    const auto info = r.displayInfo();
    std::int32_t y = c.y + m;
    const auto row = [&](const std::string& label, const std::string& value, Color vc) {
        r.drawText(c.x + m, y, label, kMuted, s);
        r.drawText(c.x + m + Renderer::textWidth("XXXXXXXXXX", s), y, value, vc, s);
        y += Renderer::textHeight(s) + 5 * s;
    };
    r.drawText(c.x + m, y, "SYSTEM", kAccent, s + 1);
    y += Renderer::textHeight(s + 1) + 6 * s;
    row("SHELL", std::string("v") + kShellVersion, kText);
    row("BOARD", std::string(ctx.board.id()), kText);
    row("DISPLAY", std::to_string(info.width) + " x " + std::to_string(info.height), kText);
    row("CAMERA", ctx.board.camera() ? "present" : "not on this board",
        ctx.board.camera() ? kGood : kMuted);
    row("TOF", "Depth applet owns it (PR #49)", kMuted);
    row("IMU", "not wired yet (#50)", kWarn);
    row("TRIGGER", "not wired yet (#50)", kWarn);
    row("BATTERY", "not wired yet (#50)", kWarn);
}

void registerStandardApplets(appfw::AppRegistry& registry,
                             const std::vector<ProvidedApplet>& provided) {
    static constexpr appfw::AppFactory kPreview[] = {makePreview<0>, makePreview<1>, makePreview<2>,
                                                     makePreview<3>, makePreview<4>};
    auto manifests = standardManifests();
    for (std::size_t i = 0; i < manifests.size(); ++i) {
        auto m = manifests[i];
        const auto it = std::find_if(provided.begin(), provided.end(),
                                     [&](const ProvidedApplet& p) { return p.id == m.id; });
        if (it != provided.end() && it->factory) {
            m.preview = false;
            registry.add(std::move(m), it->factory);
        } else if (m.id == ids::kSystem) {
            registry.add(std::move(m), makeSystem);
        } else if (i < std::size(kPreview)) {
            registry.add(std::move(m), kPreview[i]);
        }
    }
}

}  // namespace platypus::shell
