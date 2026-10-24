// scout_kiosk — Engineering Scout sessions on the UNO Q's own display.
//
//   live preview -> CAPTURE (tap, or board button A) -> evidence record saved
//   and added to the session -> card -> back to preview with guidance on what
//   to capture next -> FINISH (tap, or board button B) -> CAD artifacts
//   exported -> summary -> tap for a new session.
//
//   scout_kiosk [--device /dev/videoN] [--out DIR] [--sessions DIR]
//               [--reference-mm MM] [--prefer dp|dsi] [--offscreen DIR] [--fake]
//               [--tof auto|off] [--tof-port N] [--camera-fov HxV]
//               [--tof-orientation raw|fliph|flipv|rot180] [--shell]
//
// --shell runs the device shell instead (issue #51): splash, the swipeable
// applet carousel, and Scout and the depth view as applets on this camera,
// capture store and ToF client.
//
// DEPTH opens the depth view: camera, the VL53L8CX zone grid (UNO Q MCU
// sketch, read through arduino-router's Monitor) and the two together. The
// view owns the ToF stream only while it is open, so tools/tof_unoq/tof_log.py
// can use it the rest of the time.
//
// A composition root: this file knows the Linux hardware (V4L2 discovery, the
// DRM display, evdev input) and nothing else. Screens, capture, and the
// synthetic camera live in KioskCore (portable — tools/ui_preview renders the
// same screens on any host); sessions live in services/session.
//
// --offscreen renders into memory instead of DRM and writes preview.ppm,
// card.ppm and summary.ppm after one capture and a finish. --fake swaps the
// webcam for the validation battery's M8 scene (records name the camera
// "synthetic", so they never pass for bench evidence).
//
// The display needs DRM master (the desktop must be stopped — platypus-mode
// does it). The camera is found by capability, never by node number.
#include "KioskApplets.hpp"
#include "KioskCore.hpp"
#include "csi/CsiCamera.hpp"
#include "drm/DrmDisplay.hpp"
#include "drm/KmsHelpers.hpp"
#include "tof/TofMonitorClient.hpp"
#include "v4l2/V4l2Camera.hpp"

#include <platypus/appfw/AppRegistry.hpp>
#include <platypus/filesystem/ProjectStore.hpp>
#include <platypus/shell/Applets.hpp>
#include <platypus/shell/Shell.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace {

using namespace platypus;
using namespace platypus::kiosk;

std::atomic<bool> g_running{true};
void handleSignal(int) {
    g_running = false;
}

struct CameraChoice {
    std::string path;
    hal::CameraMode mode;
};

/// First V4L2 node offering a measurable (YUYV) mode, preferring 640x480 —
/// the analyzer's validated resolution. Codec and metadata nodes offer none.
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

int usage() {
    std::fprintf(stderr,
                 "usage: scout_kiosk [--camera auto|usb|csi] [--device /dev/videoN] [--out DIR]\n"
                 "                   [--sessions DIR]\n"
                 "                   [--reference-mm MM] [--prefer dp|dsi] [--offscreen DIR]"
                 " [--fake]\n"
                 "                   [--tof auto|off] [--tof-port N] [--camera-fov HxV]\n"
                 "                   [--tof-orientation raw|fliph|flipv|rot180] [--shell]\n");
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    std::string device;
    std::string cameraKind = "auto";  // auto: CSI when a sensor is bound, else the USB webcam
    std::string outDir = "observations";
    std::string sessionsDir = "sessions";
    std::string offscreen;
    bool fake = false;
    double referenceMm = vision::CalibrationSpec{}.referenceSideMm;
    drm::DrmDisplayConfig displayConfig;
    bool tofEnabled = true;
    bool shellMode = false;  // the device shell (#51) instead of the single-purpose kiosk
    int tofPort = 7500;
    FieldsOfView fov;
    auto orientation = hal::tof::Orientation::Raw;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (arg == "--device")
            device = next();
        else if (arg == "--camera") {
            cameraKind = next();
            if (cameraKind != "auto" && cameraKind != "usb" && cameraKind != "csi") return usage();
        } else if (arg == "--out")
            outDir = next();
        else if (arg == "--sessions")
            sessionsDir = next();
        else if (arg == "--offscreen")
            offscreen = next();
        else if (arg == "--shell")
            shellMode = true;
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
        } else if (arg == "--tof") {
            const auto v = next();
            if (v != "auto" && v != "off") return usage();
            tofEnabled = v == "auto";
        } else if (arg == "--tof-port") {
            tofPort = std::atoi(next().c_str());
            if (tofPort <= 0 || tofPort > 65535) return usage();
        } else if (arg == "--camera-fov") {
            const auto v = next();
            const auto x = v.find('x');
            if (x == std::string::npos) return usage();
            fov.cameraHDeg = std::atof(v.substr(0, x).c_str());
            fov.cameraVDeg = std::atof(v.substr(x + 1).c_str());
            if (fov.cameraHDeg <= 0 || fov.cameraVDeg <= 0) return usage();
        } else if (arg == "--tof-orientation") {
            const auto v = next();
            if (v == "raw")
                orientation = hal::tof::Orientation::Raw;
            else if (v == "fliph")
                orientation = hal::tof::Orientation::FlipH;
            else if (v == "flipv")
                orientation = hal::tof::Orientation::FlipV;
            else if (v == "rot180")
                orientation = hal::tof::Orientation::Rotate180;
            else
                return usage();
        } else {
            return usage();
        }
    }
    if (referenceMm <= 0.0) return usage();

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    // --- display (first, so a missing camera can be reported on screen)
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
    std::int32_t camW = 640, camH = 480;
    // The offscreen run renders the Scout flow only; it never opens the ToF.
    std::unique_ptr<hal::tof::TofMonitorClient> tofClient;
    if (tofEnabled && offscreen.empty())
        tofClient = std::make_unique<hal::tof::TofMonitorClient>(
            "127.0.0.1", static_cast<std::uint16_t>(tofPort));
    const auto layout = computeLayout(r.displayInfo(), camW, camH, tofClient != nullptr);
    const auto depthLayout = computeDepthLayout(r.displayInfo());

    // --- camera: acquired in a loop, so the kiosk waits for a webcam (and
    // recovers if it is unplugged) instead of exiting to a blank console.
    std::unique_ptr<hal::ICamera> camera;
    std::string cameraPath, identity;
    unoq::CsiCamera* csi = nullptr;  // set while the CSI camera is the source
    const auto acquireCamera = [&]() -> bool {
        if (fake) {
            camera = std::make_unique<SceneCamera>();
            cameraPath = identity = "synthetic";
            return static_cast<bool>(camera->open(SceneCamera::kMode));
        }
        bool announced = false;
        while (g_running) {
            csi = nullptr;
            // CSI first when allowed: it is the product camera path. The USB
            // webcam stays the proven fallback (--camera usb forces it).
            if (cameraKind != "usb" && device.empty() && unoq::CsiCamera::sensorPresent()) {
                auto cam = std::make_unique<unoq::CsiCamera>();
                if (cam->open(unoq::CsiCamera::kMode)) {
                    csi = cam.get();
                    cameraPath = "csi";
                    identity = cam->deviceIdentity();
                    camW = unoq::CsiCamera::kMode.width;
                    camH = unoq::CsiCamera::kMode.height;
                    camera = std::move(cam);
                    std::printf("camera:  %s %dx%d rgb\n", identity.c_str(), camW, camH);
                    std::fflush(stdout);
                    return true;
                }
                std::printf("camera:  csi sensor present but not usable: %s\n",
                            cam->deviceIdentity().c_str());
                std::fflush(stdout);
            }
            if (cameraKind == "csi") {
                // CSI only: keep waiting for it rather than falling back.
            } else if (const auto choice = findCamera(device)) {
                auto v4l2 = std::make_unique<unoq::V4l2Camera>(choice->path);
                if (v4l2->open(choice->mode)) {
                    cameraPath = choice->path;
                    identity =
                        v4l2->deviceIdentity().empty() ? choice->path : v4l2->deviceIdentity();
                    camW = choice->mode.width;
                    camH = choice->mode.height;
                    camera = std::move(v4l2);
                    std::printf("camera:  %s (%s) %dx%d yuyv\n", cameraPath.c_str(),
                                identity.c_str(), camW, camH);
                    std::fflush(stdout);
                    return true;
                }
            }
            if (snapshot) return false;  // offscreen runs never wait
            if (!announced) {
                std::printf("camera:  none yet - waiting\n");
                std::fflush(stdout);
                announced = true;
            }
            PanelState waiting;
            waiting.status = "No camera found. Scout starts as soon as one is plugged in.";
            waiting.statusColour = kWarn;
            waiting.noCamera = true;
            drawPreviewScreen(r, layout, {}, camW, camH, waiting);
            (void)r.present();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        return false;
    };
    if (!acquireCamera()) {
        if (g_running) std::fprintf(stderr, "error: no usable camera (CSI or V4L2 YUYV) found\n");
        if (drmDisplay) drmDisplay->close();
        return g_running ? 1 : 0;
    }

    observation::CaptureService service(outDir);
    session::SessionStore store(sessionsDir);
    const vision::CalibrationSpec spec{referenceMm};
    const auto newSession = [&] {
        return std::make_unique<session::ObjectSession>(
            store.nextSessionId(), session::fastenerProfile(),
            observation::CaptureService::currentUtcTimestamp());
    };
    auto current = newSession();
    std::printf("session: %s\n", current->id().c_str());

    std::string status;
    Color statusColour = kMuted;
    std::vector<std::uint8_t> lastRgb;

    const auto panelState = [&](bool busy) {
        PanelState p;
        p.sessionId = current->id();
        p.guidance = current->guidance();
        p.status = status;
        p.statusColour = statusColour;
        p.busy = busy;
        p.depthAvailable = tofClient != nullptr;
        return p;
    };
    // The CSI camera's identity carries the exposure and gain in use, and
    // every CSI capture keeps its raw frame for re-analysis on a host
    // (ui_preview --analyze): analysis improves; old captures do not.
    const observation::AttachFn attachRaw = [&](const hal::Frame&) {
        std::vector<observation::AttachedFile> files;
        if (!csi) return files;
        auto raw = csi->lastRaw();
        if (raw.packed.empty()) return files;
        char meta[640];
        std::snprintf(meta, sizeof(meta),
                      "{\"format\":\"SRGGB10P\",\"cfa\":\"RGGB\",\"width\":%u,\"height\":%u,"
                      "\"bytes_per_line\":%u,\"black_level\":%u,\"white_level\":1023,"
                      "\"requested_exposure_lines\":%u,\"requested_analogue_gain_code\":%u,"
                      "\"requested_digital_gain_q8\":%u,\"line_time_ns\":%u,"
                      "\"white_balance_gains\":[%.4f,%.4f,%.4f],"
                      "\"note\":\"exposure changes reach the sensor about two frames "
                      "late\"}\n",
                      raw.width, raw.height, raw.bytesPerLine, unsigned{raw.blackLevel},
                      raw.requested.lines, raw.requested.gainCode, raw.requested.digitalCode,
                      raw.lineTimeNs, static_cast<double>(raw.whiteBalance[0]),
                      static_cast<double>(raw.whiteBalance[1]),
                      static_cast<double>(raw.whiteBalance[2]));
        const std::string m(meta);
        std::vector<std::byte> metaBytes(m.size());
        std::memcpy(metaBytes.data(), m.data(), m.size());
        files.push_back({"raw-frame", "image/x-raw-srggb10p", "raw.raw10", std::move(raw.packed)});
        files.push_back({"raw-frame-meta", "application/json", "raw.json", std::move(metaBytes)});
        return files;
    };
    const auto doCapture = [&]() -> CaptureOutcome {
        drawPreviewScreen(r, layout, lastRgb, camW, camH, [&] {
            auto p = panelState(true);
            p.status = "Measuring...";
            p.statusColour = kAccent;
            return p;
        }());
        (void)r.present();
        auto outcome = captureOnce(*camera, service, spec, cameraPath,
                                   csi ? csi->deviceIdentity() : identity, attachRaw);
        if (outcome.record) {
            current->add(sessionCapture(outcome, current->profile()));
            std::string error;
            if (!store.save(*current, &error))
                std::fprintf(stderr, "session save: %s\n", error.c_str());
        }
        status = outcome.status;
        statusColour = outcome.statusColour;
        std::printf("capture: %s\n", outcome.status.c_str());
        std::fflush(stdout);
        return outcome;
    };
    const auto doFinish =
        [&](session::FinishResult& result) -> std::optional<session::CloseReason> {
        const auto g = current->guidance();
        if (g.measured == 0) {
            status = "Nothing measured yet - capture first.";
            statusColour = kWarn;
            return std::nullopt;
        }
        const auto reason = g.modelSatisfied ? session::CloseReason::ModelSatisfied
                                             : session::CloseReason::ClosedByOperator;
        result = store.finish(*current, reason, observation::CaptureService::currentUtcTimestamp());
        std::printf("finish:  %s %s -> %s\n", current->id().c_str(),
                    std::string(session::to_string(reason)).c_str(),
                    result.ok() ? result.directory.string().c_str() : result.error.c_str());
        std::fflush(stdout);
        return reason;
    };

    // Offscreen: preview, one capture, the card, a finish, the summary.
    if (snapshot) {
        if (auto f = camera->capture(std::chrono::milliseconds(2000)); f)
            lastRgb = frameToRgb(f.value());
        drawPreviewScreen(r, layout, lastRgb, camW, camH, panelState(false));
        (void)r.present();
        snapshot->writePpm(std::filesystem::path(offscreen) / "preview.ppm");
        const auto outcome = doCapture();
        if (outcome.measured) apps::drawObservationCard(r, *outcome.record, outcome.thumbnail);
        (void)r.present();
        snapshot->writePpm(std::filesystem::path(offscreen) / "card.ppm");
        session::FinishResult result;
        if (const auto reason = doFinish(result)) {
            drawSessionSummary(r, *current, result, *reason);
            (void)r.present();
            snapshot->writePpm(std::filesystem::path(offscreen) / "summary.ppm");
        }
        camera->close();
        return 0;
    }

    // --- the device shell (#51): splash, applet carousel, and Scout and Depth
    // as applets on this camera, capture store and ToF client.
    if (shellMode) {
        shell::MinimalBoard board(display);
        filesystem::ProjectStore projects(nullptr);
        appfw::AppRegistry registry;
        std::vector<shell::ProvidedApplet> provided;
        provided.push_back({shell::ids::kScout, [&]() -> std::unique_ptr<appfw::IApp> {
                                ScoutDeps d;
                                d.camera = camera.get();
                                d.camW = camW;
                                d.camH = camH;
                                d.captures = &service;
                                d.sessions = &store;
                                d.spec = spec;
                                d.device = cameraPath;
                                d.identity = [&] { return csi ? csi->deviceIdentity() : identity; };
                                d.attach = attachRaw;
                                d.buttonCapture = drm::kBoardButtonA;
                                d.buttonFinish = drm::kBoardButtonB;
                                return std::make_unique<ScoutApplet>(std::move(d));
                            }});
        if (tofClient)
            provided.push_back({shell::ids::kDepth, [&]() -> std::unique_ptr<appfw::IApp> {
                                    DepthDeps d;
                                    d.camera = camera.get();
                                    d.camW = camW;
                                    d.camH = camH;
                                    d.tof = tofClient.get();
                                    d.fov = fov;
                                    d.orientation = orientation;
                                    return std::make_unique<DepthApplet>(std::move(d));
                                }});
        shell::registerStandardApplets(registry, provided);
        shell::ShellOptions options;
        options.buttonNext = drm::kBoardButtonA;
        options.buttonOpen = drm::kBoardButtonB;
        shell::Shell sh(registry, appfw::AppContext{board, r, projects, {}}, options);

        // Input arrives on the display's thread; the shell runs on this one.
        std::mutex inputMutex;
        std::vector<std::variant<hal::TouchEvent, hal::ButtonEvent>> inputs;
        display->onTouch([&](const hal::TouchEvent& e) {
            std::lock_guard lock(inputMutex);
            inputs.emplace_back(e);
        });
        display->onButton([&](const hal::ButtonEvent& e) {
            std::lock_guard lock(inputMutex);
            inputs.emplace_back(e);
        });
        std::printf(
            "running: shell - swipe the cards, tap to open, HOME returns; Ctrl-C to quit\n");
        std::fflush(stdout);
        auto last = std::chrono::steady_clock::now();
        while (g_running) {
            std::vector<std::variant<hal::TouchEvent, hal::ButtonEvent>> batch;
            {
                std::lock_guard lock(inputMutex);
                batch.swap(inputs);
            }
            for (const auto& ev : batch) {
                if (const auto* t = std::get_if<hal::TouchEvent>(&ev))
                    sh.onTouch(*t);
                else
                    sh.onButton(std::get<hal::ButtonEvent>(ev));
            }
            const auto now = std::chrono::steady_clock::now();
            sh.frame(std::chrono::duration_cast<std::chrono::milliseconds>(now - last));
            last = now;
            // Applets pace themselves on camera frames; the home screen does not.
            if (sh.state() != shell::Shell::State::App)
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        if (drmDisplay) drmDisplay->close();  // joins the input thread first
        sh.home();                            // stops the active applet: releases the ToF
        if (camera) camera->close();
        return 0;
    }

    // --- input: handlers run on the display's input thread; the loop consumes.
    enum class Screen { Preview, Card, Summary, Depth };
    std::atomic<Screen> screen{Screen::Preview};
    std::atomic<bool> captureRequested{false}, finishRequested{false}, dismissRequested{false};
    std::atomic<bool> depthRequested{false}, flipRequested{false};
    display->onTouch([&](const hal::TouchEvent& e) {
        if (e.type != hal::TouchEvent::Type::Down) return;
        if (screen == Screen::Depth) {
            if (inside(depthLayout.back, e.x, e.y))
                dismissRequested = true;
            else if (inside(depthLayout.tof, e.x, e.y) || inside(depthLayout.fused, e.x, e.y))
                flipRequested = true;
        } else if (screen != Screen::Preview) {
            dismissRequested = true;
        } else if (inside(layout.capture, e.x, e.y)) {
            captureRequested = true;
        } else if (inside(layout.finish, e.x, e.y)) {
            finishRequested = true;
        } else if (tofClient && inside(layout.depth, e.x, e.y)) {
            depthRequested = true;
        }
    });
    display->onButton([&](const hal::ButtonEvent& e) {
        if (!e.pressed) return;
        if (screen != Screen::Preview)
            dismissRequested = true;
        else if (e.id == drm::kBoardButtonB)
            finishRequested = true;
        else
            captureRequested = true;
    });

    std::printf("running: CAPTURE / FINISH on the panel or board buttons A / B; Ctrl-C to quit\n");
    std::fflush(stdout);
    int missedFrames = 0;
    while (g_running) {
        const auto frame = camera->capture(std::chrono::milliseconds(200));
        // ~5 s without a frame means the webcam went away: wait for it again.
        missedFrames = frame ? 0 : missedFrames + 1;
        if (missedFrames >= 25) {
            std::printf("camera:  lost - waiting for it to come back\n");
            std::fflush(stdout);
            camera->close();
            camera.reset();
            csi = nullptr;
            lastRgb.clear();
            if (tofClient) tofClient->stop();
            screen = Screen::Preview;
            if (!acquireCamera()) break;
            missedFrames = 0;
            status = "Camera reconnected.";
            statusColour = kGood;
            continue;
        }
        if (screen == Screen::Card) {
            if (dismissRequested.exchange(false)) screen = Screen::Preview;
            continue;  // keep draining the camera so the preview is live on return
        }
        if (screen == Screen::Summary) {
            if (dismissRequested.exchange(false)) {
                current = newSession();
                status = "New session " + current->id() + ".";
                statusColour = kMuted;
                std::printf("session: %s\n", current->id().c_str());
                screen = Screen::Preview;
            }
            continue;
        }
        if (frame) lastRgb = frameToRgb(frame.value());

        if (screen == Screen::Depth) {
            if (dismissRequested.exchange(false)) {
                tofClient->stop();  // frees the Monitor for tof_log.py
                std::printf("depth:   closed\n");
                std::fflush(stdout);
                screen = Screen::Preview;
                continue;
            }
            if (flipRequested.exchange(false)) {
                orientation = hal::tof::next(orientation);
                const auto name = hal::tof::to_string(orientation);
                std::printf("depth:   orientation %.*s\n", static_cast<int>(name.size()),
                            name.data());
                std::fflush(stdout);
            }
            const auto snap = tofClient->snapshot();
            DepthState st;
            st.connected = snap.connected;
            st.frame = snap.frame;
            st.stale = snap.frame && std::chrono::steady_clock::now() - snap.frameArrived >
                                         std::chrono::milliseconds(1500);
            st.hz = snap.hz;
            st.frames = snap.frames;
            st.rejects = snap.rejects;
            st.lastEvent = snap.lastEvent;
            st.lastError = snap.lastError;
            st.endpoint = tofClient->endpoint();
            st.orientation = orientation;
            st.fov = fov;
            drawDepthScreen(r, depthLayout, lastRgb, camW, camH, st);
            (void)r.present();
            continue;
        }
        if (depthRequested.exchange(false)) {
            tofClient->start();
            std::printf("depth:   opened, ToF via %s\n", tofClient->endpoint().c_str());
            std::fflush(stdout);
            dismissRequested = false;
            flipRequested = false;
            screen = Screen::Depth;
            continue;
        }

        if (captureRequested.exchange(false)) {
            const auto outcome = doCapture();
            if (outcome.measured) {
                apps::drawObservationCard(r, *outcome.record, outcome.thumbnail);
                (void)r.present();
                dismissRequested = false;
                screen = Screen::Card;
                continue;
            }
        }
        if (finishRequested.exchange(false)) {
            session::FinishResult result;
            if (const auto reason = doFinish(result)) {
                drawSessionSummary(r, *current, result, *reason);
                (void)r.present();
                dismissRequested = false;
                screen = Screen::Summary;
                continue;
            }
        }
        drawPreviewScreen(r, layout, lastRgb, camW, camH, panelState(false));
        (void)r.present();
    }

    // Join the display's input thread before the state its handlers capture
    // goes out of scope; close() also hands the screen back.
    if (drmDisplay) drmDisplay->close();
    if (tofClient) tofClient->stop();
    if (camera) camera->close();
    return 0;
}
