// scout_kiosk — the Dream Lab demo loop on the UNO Q's own display.
//
//   live camera preview -> tap CAPTURE (or press a board button) -> measured,
//   classified evidence record saved -> Scout result card -> tap to go back.
//
//   scout_kiosk [--device /dev/videoN] [--out DIR] [--reference-mm MM]
//               [--prefer dp|dsi] [--offscreen DIR] [--fake]
//
// A composition root, like apps/launcher/src/main.cpp: the only file here that
// knows concrete types. Everything it shows comes from pieces that are already
// tested on their own — V4l2Camera, CaptureService, the vision analyzer and
// fastener classifier (validation battery), drawObservationCard (the evidence
// card), and DrmDisplay. The kiosk adds only the preview, the button, and the
// operator guidance when the analyzer refuses a scene.
//
// --offscreen renders into memory instead of DRM, runs one preview frame and
// one simulated capture, and writes preview.ppm and card.ppm to DIR: the whole
// loop, verifiable with no panel attached. --fake swaps the webcam for the
// validation battery's M8 bolt scene, served as YUYV exactly like the real
// camera; its records name the camera "synthetic" so they can never pass
// for bench evidence.
//
// The display needs DRM master: stop the desktop first (sudo systemctl stop
// lightdm). The camera is found by capability, never by a remembered node
// number — node numbers move between boots on the UNO Q.
#include "drm/DrmDisplay.hpp"
#include "drm/KmsHelpers.hpp"
#include "v4l2/V4l2Camera.hpp"

#include <platypus/ai/FastenerClassifier.hpp>
#include <platypus/apps/EngineeringScoutApp.hpp>
#include <platypus/hal/testing/SyntheticScene.hpp>
#include <platypus/observation/CaptureService.hpp>
#include <platypus/renderer/Renderer.hpp>
#include <platypus/vision/ScoutAnalyzer.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

using namespace platypus;
using renderer::Color;
using renderer::Rect;

std::atomic<bool> g_running{true};
void handleSignal(int) {
    g_running = false;
}

constexpr Color kBackground{12, 14, 18};
constexpr Color kPanel{28, 32, 40};
constexpr Color kText{230, 232, 236};
constexpr Color kMuted{140, 146, 158};
constexpr Color kAccent{255, 196, 0};
constexpr Color kGood{80, 200, 120};
constexpr Color kWarn{255, 110, 90};

// --- camera -----------------------------------------------------------------

struct CameraChoice {
    std::string path;
    hal::CameraMode mode;
};

/// First V4L2 node offering a measurable (YUYV) mode, preferring 640x480 —
/// the analyzer's validated resolution and the webcam's full-rate YUYV mode.
/// Codec and metadata nodes offer none, so they are skipped naturally.
std::optional<CameraChoice> findCamera(const std::string& requested) {
    std::vector<std::string> paths;
    if (!requested.empty()) {
        paths.push_back(requested);
    } else {
        for (int n = 0; n < 32; ++n) {
            const auto p = "/dev/video" + std::to_string(n);
            if (std::filesystem::exists(p)) paths.push_back(p);
        }
    }
    for (const auto& path : paths) {
        unoq::V4l2Camera probe(path);
        std::optional<hal::CameraMode> best;
        for (const auto& m : probe.supportedModes()) {
            if (m.format != hal::PixelFormat::YUYV) continue;
            if (m.width == 640 && m.height == 480) {
                best = m;
                break;
            }
            if (!best) best = m;
        }
        if (best) return CameraChoice{path, *best};
    }
    return std::nullopt;
}

/// Packed YUYV (BT.601, limited range) to tightly packed RGB888.
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
            rgb[o + 3 * static_cast<std::size_t>(k) + 0] = clamp8((c + 409 * v + 128) >> 8);
            rgb[o + 3 * static_cast<std::size_t>(k) + 1] =
                clamp8((c - 100 * u - 208 * v + 128) >> 8);
            rgb[o + 3 * static_cast<std::size_t>(k) + 2] = clamp8((c + 516 * u + 128) >> 8);
        }
    }
    return rgb;
}

// --- layout -----------------------------------------------------------------

/// Laid out against the display's real geometry (ADR-0001): the 800x480 panel
/// and a 1080p bench monitor both come out right.
struct Layout {
    Rect preview;
    Rect panel;
    Rect button;
    std::int32_t textScale = 2;
};

Layout computeLayout(const hal::DisplayInfo& info, std::int32_t camW, std::int32_t camH) {
    const std::int32_t w = info.width, h = info.height;
    const std::int32_t margin = std::max(8, h / 30);
    Layout l;
    l.textScale = std::max(1, h / 240);
    std::int32_t ph = h - 2 * margin;
    std::int32_t pw = ph * camW / std::max(1, camH);
    const std::int32_t maxPw = w * 3 / 4;
    if (pw > maxPw) {
        pw = maxPw;
        ph = pw * camH / std::max(1, camW);
    }
    l.preview = {margin, (h - ph) / 2, pw, ph};
    const std::int32_t px = margin + pw + margin;
    l.panel = {px, margin, w - px - margin, h - 2 * margin};
    const std::int32_t bh = h / 4;
    l.button = {l.panel.x + margin / 2, l.panel.y + l.panel.h - bh - margin / 2, l.panel.w - margin,
                bh};
    return l;
}

bool inside(const Rect& r, std::int32_t x, std::int32_t y) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

/// Word-wraps `text` to `maxWidth` pixels and draws it from (x, y); returns the
/// y below the last line.
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
                       std::int32_t camW, std::int32_t camH, const std::string& status,
                       Color statusColour, bool busy) {
    r.clear(kBackground);
    if (!rgb.empty()) r.drawImage(l.preview, rgb, camW, camH, 3);
    r.drawRect(l.preview, kMuted);
    r.fillRect(l.panel, kPanel);

    const std::int32_t s = l.textScale;
    const std::int32_t tx = l.panel.x + 6 * s;
    const std::int32_t tw = l.panel.w - 12 * s;
    std::int32_t y = l.panel.y + 6 * s;
    y = drawWrapped(r, tx, y, tw, "ENGINEERING SCOUT", kAccent, s);
    y += 4 * s;
    y = drawWrapped(r, tx, y, tw, "20 mm square + one fastener, flat, in view.", kMuted,
                    std::max(1, s - 1));
    y += 6 * s;
    if (!status.empty()) drawWrapped(r, tx, y, tw, status, statusColour, std::max(1, s - 1));

    r.fillRect(l.button, busy ? kMuted : kAccent);
    const std::string label = busy ? "WORKING" : "CAPTURE";
    const std::int32_t ls =
        std::max(1, std::min(s + 1, l.button.w / renderer::Renderer::textWidth(label, 1)));
    const auto lw = renderer::Renderer::textWidth(label, ls);
    r.drawText(l.button.x + (l.button.w - lw) / 2,
               l.button.y + (l.button.h - renderer::Renderer::textHeight(ls)) / 2, label,
               kBackground, ls);
}

/// The analyzer refuses a scene rather than guess; the operator gets told what
/// to change. This is the MVP's "active guidance" requirement at the moment it
/// is needed most.
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

// --- synthetic camera -------------------------------------------------------

/// Development source for --fake: the validation battery's M8 case — a 20 mm
/// square and an 8 x 40 mm shaft at 45 degrees, at 0.25 mm/px — served as YUYV,
/// the webcam's own format, so preview and analysis take the hardware path.
class SceneCamera final : public hal::ICamera {
   public:
    SceneCamera() {
        constexpr double pxPerMm = 4.0;
        scene_.addSquare(80, 80, static_cast<std::int32_t>(20.0 * pxPerMm));
        scene_.addRect(400.0, 280.0, 40.0 * pxPerMm, 8.0 * pxPerMm,
                       45.0 * 3.14159265358979 / 180.0);
    }
    [[nodiscard]] std::vector<hal::CameraMode> supportedModes() const override { return {kMode}; }
    hal::Status open(const hal::CameraMode&) override {
        open_ = true;
        return {};
    }
    hal::Status close() override {
        open_ = false;
        return {};
    }
    [[nodiscard]] bool isOpen() const noexcept override { return open_; }
    hal::Status setControls(const hal::CameraControls&) override { return {}; }
    hal::Result<hal::Frame> capture(std::chrono::milliseconds) override {
        if (!open_) return hal::Error::NotInitialized;
        return scene_.yuyvFrame();
    }
    hal::Status startStream(std::function<void(const hal::Frame&)> onFrame) override {
        onFrame(scene_.yuyvFrame());
        return {};
    }
    hal::Status stopStream() override { return {}; }

    static constexpr hal::CameraMode kMode{640, 480, hal::PixelFormat::YUYV, 30.0f};

   private:
    hal::testing::SyntheticScene scene_;
    bool open_ = false;
};

// --- offscreen display --------------------------------------------------------

/// IDisplay that keeps the last frame so it can be written to a file.
class SnapshotDisplay final : public hal::IDisplay {
   public:
    explicit SnapshotDisplay(hal::DisplayInfo info)
        : info_(info), frame_(std::size_t{info.width} * info.height * 2) {}
    hal::DisplayInfo info() const noexcept override { return info_; }
    hal::Status setBacklight(float) override { return {}; }
    hal::Status present(std::span<const std::byte> pixels) override {
        if (pixels.size() == frame_.size()) std::copy(pixels.begin(), pixels.end(), frame_.begin());
        return {};
    }
    hal::Status onTouch(std::function<void(const hal::TouchEvent&)>) override { return {}; }
    hal::Status onButton(std::function<void(const hal::ButtonEvent&)>) override { return {}; }

    bool writePpm(const std::filesystem::path& file) const {
        std::ofstream out(file, std::ios::binary);
        out << "P6\n" << info_.width << " " << info_.height << "\n255\n";
        for (std::size_t i = 0; i + 1 < frame_.size(); i += 2) {
            const auto p =
                static_cast<std::uint16_t>(std::to_integer<std::uint16_t>(frame_[i]) |
                                           (std::to_integer<std::uint16_t>(frame_[i + 1]) << 8));
            const char rgb[3] = {static_cast<char>(((p >> 11) & 0x1F) << 3),
                                 static_cast<char>(((p >> 5) & 0x3F) << 2),
                                 static_cast<char>((p & 0x1F) << 3)};
            out.write(rgb, 3);
        }
        return static_cast<bool>(out);
    }

   private:
    hal::DisplayInfo info_;
    std::vector<std::byte> frame_;
};

// --- capture ------------------------------------------------------------------

struct CaptureOutcome {
    std::optional<observation::EngineeringObservation> record;
    std::optional<apps::CardImage> thumbnail;
    std::string status;
    Color statusColour = kText;
};

CaptureOutcome captureOnce(hal::ICamera& camera, observation::CaptureService& service,
                           const vision::CalibrationSpec& spec, const std::string& device,
                           const std::string& identity) {
    observation::CaptureConfig config;
    config.observationId = service.nextObservationId();
    config.timestampUtc = observation::CaptureService::currentUtcTimestamp();
    config.source = {{"app", "scout_kiosk"}, {"camera", device}, {"camera_identity", identity}};

    CaptureOutcome out;
    auto sceneError = vision::AnalyzeError::None;
    bool measured = false;
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
        measured = true;
    };

    const auto result = service.capture(camera, config);
    if (!result) {
        const auto reason = hal::to_string(result.error());
        out.status = "Capture failed: " + std::string(reason);
        out.statusColour = kWarn;
        return out;
    }
    if (measured) {
        out.record = result.value().record;
        out.status = "Saved " + result.value().record.observationId;
        out.statusColour = kGood;
    } else {
        // The capture-only record is still written — honest evidence that the
        // attempt happened — but the operator sees what to change, not a card
        // with nothing on it.
        out.status =
            guidanceFor(sceneError) + " (" + result.value().record.observationId + " saved)";
        out.statusColour = kWarn;
    }
    return out;
}

int usage() {
    std::fprintf(stderr,
                 "usage: scout_kiosk [--device /dev/videoN] [--out DIR] [--reference-mm MM]\n"
                 "                   [--prefer dp|dsi] [--offscreen DIR] [--fake]\n");
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    std::string device;
    std::string outDir = "observations";
    std::string offscreen;
    bool fake = false;
    double referenceMm = vision::CalibrationSpec{}.referenceSideMm;
    drm::DrmDisplayConfig displayConfig;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (arg == "--device")
            device = next();
        else if (arg == "--out")
            outDir = next();
        else if (arg == "--offscreen")
            offscreen = next();
        else if (arg == "--fake")
            fake = true;
        else if (arg == "--reference-mm")
            referenceMm = std::atof(next().c_str());
        else if (arg == "--prefer") {
            const auto which = next();
            if (which == "dp")
                displayConfig.preferredConnectorType = drm::kConnectorDisplayPort;
            else if (which == "dsi")
                displayConfig.preferredConnectorType = drm::kConnectorDsi;
            else
                return usage();
        } else {
            return usage();
        }
    }
    if (referenceMm <= 0.0) return usage();

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    // --- camera
    std::unique_ptr<hal::ICamera> camera;
    std::string cameraPath, identity;
    hal::CameraMode mode;
    if (fake) {
        camera = std::make_unique<SceneCamera>();
        cameraPath = identity = "synthetic";
        mode = SceneCamera::kMode;
    } else {
        const auto choice = findCamera(device);
        if (!choice) {
            std::fprintf(stderr, "error: no V4L2 camera with a YUYV mode found\n");
            return 1;
        }
        auto v4l2 = std::make_unique<unoq::V4l2Camera>(choice->path);
        cameraPath = choice->path;
        identity = v4l2->deviceIdentity().empty() ? choice->path : v4l2->deviceIdentity();
        mode = choice->mode;
        camera = std::move(v4l2);
    }
    if (const auto s = camera->open(mode); !s) {
        const auto reason = hal::to_string(s.error());
        std::fprintf(stderr, "error: open %s: %.*s\n", cameraPath.c_str(),
                     static_cast<int>(reason.size()), reason.data());
        return 1;
    }
    const std::int32_t camW = mode.width, camH = mode.height;
    std::printf("camera:  %s (%s) %dx%d yuyv\n", cameraPath.c_str(), identity.c_str(), camW, camH);

    // --- display
    std::shared_ptr<hal::IDisplay> display;
    std::shared_ptr<SnapshotDisplay> snapshot;
    std::shared_ptr<drm::DrmDisplay> drmDisplay;
    if (!offscreen.empty()) {
        snapshot = std::make_shared<SnapshotDisplay>(hal::DisplayInfo{800, 480, 16});
        display = snapshot;
        std::printf("display: offscreen 800x480 -> %s\n", offscreen.c_str());
    } else {
        drmDisplay = std::make_shared<drm::DrmDisplay>(displayConfig);
        if (const auto s = drmDisplay->open(); !s) {
            const auto reason = hal::to_string(s.error());
            std::fprintf(stderr, "error: display: %.*s - %s\n", static_cast<int>(reason.size()),
                         reason.data(), drmDisplay->diagnostic().c_str());
            return 1;
        }
        const auto& sel = drmDisplay->selection();
        std::printf("display: %s %ux%u, touch %s\n",
                    std::string(drm::connectorTypeName(sel.connectorType)).c_str(), sel.width,
                    sel.height, sel.touchDevice.empty() ? "none" : sel.touchDevice.c_str());
        display = drmDisplay;
    }
    std::fflush(stdout);

    renderer::Renderer r(display);
    const auto layout = computeLayout(r.displayInfo(), camW, camH);
    observation::CaptureService service(outDir);
    const vision::CalibrationSpec spec{referenceMm};

    // --- input: handlers run on the display's input thread; the loop consumes.
    enum class Screen { Preview, Card };
    std::atomic<Screen> screen{Screen::Preview};
    std::atomic<bool> captureRequested{false};
    std::atomic<bool> dismissRequested{false};
    display->onTouch([&](const hal::TouchEvent& e) {
        if (e.type != hal::TouchEvent::Type::Down) return;
        if (screen == Screen::Card)
            dismissRequested = true;
        else if (inside(layout.button, e.x, e.y))
            captureRequested = true;
    });
    display->onButton([&](const hal::ButtonEvent& e) {
        if (!e.pressed) return;
        if (screen == Screen::Card)
            dismissRequested = true;
        else
            captureRequested = true;
    });

    std::string status = "Ready.";
    Color statusColour = kMuted;
    std::vector<std::uint8_t> lastRgb;

    // Offscreen: one preview frame, one capture, two screenshots, done.
    if (snapshot) {
        for (int k = 0; k < 3 && lastRgb.empty(); ++k)
            if (auto f = camera->capture(std::chrono::milliseconds(2000)); f)
                lastRgb = yuyvToRgb(f.value());
        drawPreviewScreen(r, layout, lastRgb, camW, camH, status, statusColour, false);
        (void)r.present();
        snapshot->writePpm(std::filesystem::path(offscreen) / "preview.ppm");
        auto outcome = captureOnce(*camera, service, spec, cameraPath, identity);
        std::printf("capture: %s\n", outcome.status.c_str());
        if (outcome.record)
            apps::drawObservationCard(r, *outcome.record, outcome.thumbnail);
        else
            drawPreviewScreen(r, layout, lastRgb, camW, camH, outcome.status, outcome.statusColour,
                              false);
        (void)r.present();
        snapshot->writePpm(std::filesystem::path(offscreen) / "card.ppm");
        camera->close();
        return 0;
    }

    std::printf("running: tap CAPTURE or press a board button; Ctrl-C to quit\n");
    std::fflush(stdout);
    while (g_running) {
        const auto frame = camera->capture(std::chrono::milliseconds(200));
        if (screen == Screen::Card) {
            if (dismissRequested.exchange(false)) {
                screen = Screen::Preview;
                captureRequested = false;
            }
            continue;  // keep draining the camera so the preview is live on return
        }
        if (frame) lastRgb = yuyvToRgb(frame.value());

        if (captureRequested.exchange(false)) {
            drawPreviewScreen(r, layout, lastRgb, camW, camH, "Measuring...", kAccent, true);
            (void)r.present();
            auto outcome = captureOnce(*camera, service, spec, cameraPath, identity);
            std::printf("capture: %s\n", outcome.status.c_str());
            std::fflush(stdout);
            status = outcome.status;
            statusColour = outcome.statusColour;
            if (outcome.record) {
                apps::drawObservationCard(r, *outcome.record, outcome.thumbnail);
                (void)r.present();
                dismissRequested = false;
                screen = Screen::Card;
                continue;
            }
        }
        drawPreviewScreen(r, layout, lastRgb, camW, camH, status, statusColour, false);
        (void)r.present();
    }

    // Join the display's input thread before the state its handlers capture
    // goes out of scope; close() also hands the screen back.
    if (drmDisplay) drmDisplay->close();
    camera->close();
    return 0;
}
