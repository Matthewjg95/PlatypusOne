// ui_preview — offline renders of PlatypusOS UI surfaces for visual review.
//
//   ui_preview --font-specimen OUT.ppm
//   ui_preview --scout-card RECORD.json OUT.ppm
//   ui_preview --scout-card-demo OUT.ppm
//   ui_preview --kiosk-session-demo OUTDIR
//   ui_preview --analyze-yuyv SOURCE.yuyv   (re-run a board capture on the host)
//
// Draws through the real Renderer into an in-memory display and writes the
// frame as a P6 PPM. A bring-up tool (like engineering_scout_capture): UI
// changes get eyeballed from a file without opening the interactive sim.
// --scout-card-demo runs the real analyzer + classifier over a synthetic
// bolt scene so the card shows a genuine end-to-end record.
// --kiosk-session-demo scripts a whole Scout session through the kiosk's own
// screens and the real capture, analyzer, session and export code, writing
// one PPM per screen plus the session folder with its CAD artifacts.
#include "KioskCore.hpp"

#include <platypus/ai/FastenerClassifier.hpp>
#include <platypus/apps/EngineeringScoutApp.hpp>
#include <platypus/hal/testing/SyntheticScene.hpp>
#include <platypus/renderer/Renderer.hpp>
#include <platypus/vision/ScoutAnalyzer.hpp>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace platypus;

/// IDisplay that stores the last presented frame; no window, no timing.
class MemoryDisplay final : public hal::IDisplay {
   public:
    MemoryDisplay(std::uint16_t width, std::uint16_t height) : info_{width, height, 16} {}

    hal::DisplayInfo info() const noexcept override { return info_; }
    hal::Status setBacklight(float) override { return {}; }
    hal::Status present(std::span<const std::byte> pixels) override {
        frame_.assign(pixels.begin(), pixels.end());
        return {};
    }
    hal::Status onTouch(std::function<void(const hal::TouchEvent&)>) override { return {}; }
    hal::Status onButton(std::function<void(const hal::ButtonEvent&)>) override { return {}; }

    [[nodiscard]] const std::vector<std::byte>& frame() const noexcept { return frame_; }

   private:
    hal::DisplayInfo info_;
    std::vector<std::byte> frame_;
};

bool writePpm(const std::string& path, const MemoryDisplay& display) {
    const auto info = display.info();
    const auto& frame = display.frame();
    if (frame.size() != static_cast<std::size_t>(info.width) * info.height * 2) {
        std::fprintf(stderr, "error: no frame was presented\n");
        return false;
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "P6\n" << info.width << ' ' << info.height << "\n255\n";
    for (std::size_t i = 0; i < frame.size(); i += 2) {
        const auto low = std::to_integer<std::uint16_t>(frame[i]);
        const auto high = std::to_integer<std::uint16_t>(frame[i + 1]);
        const auto rgb565 = static_cast<std::uint16_t>(low | (high << 8));
        const auto r5 = (rgb565 >> 11) & 0x1F;
        const auto g6 = (rgb565 >> 5) & 0x3F;
        const auto b5 = rgb565 & 0x1F;
        const char rgb[3] = {
            static_cast<char>((r5 << 3) | (r5 >> 2)),
            static_cast<char>((g6 << 2) | (g6 >> 4)),
            static_cast<char>((b5 << 3) | (b5 >> 2)),
        };
        out.write(rgb, 3);
    }
    return out.good();
}

constexpr renderer::Color kBackground{18, 24, 32};
constexpr renderer::Color kInk{230, 235, 240};
constexpr renderer::Color kAccent{120, 200, 255};

int renderFontSpecimen(const std::string& outPath) {
    auto display = std::make_shared<MemoryDisplay>(std::uint16_t{480}, std::uint16_t{320});
    renderer::Renderer r(display);
    r.clear(kBackground);

    std::int32_t y = 8;
    r.drawText(8, y, "PlatypusOS 5x7 specimen", kAccent, 2);
    y += renderer::Renderer::textHeight(2) + 8;

    // Full printable set at scale 2, 24 glyphs per row.
    std::string ascii;
    for (char ch = 32; ch < 127; ++ch)
        ascii.push_back(ch);
    for (std::size_t start = 0; start < ascii.size(); start += 24) {
        r.drawText(8, y, std::string_view(ascii).substr(start, 24), kInk, 2);
        y += renderer::Renderer::textHeight(2) + 4;
    }

    y += 6;
    r.drawText(8, y, "scale 1: The quick brown fox jumps over the lazy dog 0123456789", kInk, 1);
    y += renderer::Renderer::textHeight(1) + 4;
    r.drawText(8, y, "M6 nut, 10.50 mm across flats (fit 0.050) -> confidence 0.67", kInk, 1);
    y += renderer::Renderer::textHeight(1) + 10;
    r.drawText(8, y, "scale 3:", kInk, 1);
    r.drawText(8 + 60, y - 7, "M12 PASS", kAccent, 3);

    if (const auto status = r.present(); !status.ok()) {
        std::fprintf(stderr, "error: present failed\n");
        return 1;
    }
    if (!writePpm(outPath, *display)) return 1;
    std::printf("wrote %s\n", outPath.c_str());
    return 0;
}

int renderCard(const observation::EngineeringObservation& record, const std::string& outPath,
               const std::optional<apps::CardImage>& thumbnail = std::nullopt) {
    auto display = std::make_shared<MemoryDisplay>(std::uint16_t{480}, std::uint16_t{320});
    renderer::Renderer r(display);
    apps::drawObservationCard(r, record, thumbnail);
    if (const auto status = r.present(); !status.ok()) {
        std::fprintf(stderr, "error: present failed\n");
        return 1;
    }
    if (!writePpm(outPath, *display)) return 1;
    std::printf("wrote %s\n", outPath.c_str());
    return 0;
}

int renderScoutCard(const std::string& jsonPath, const std::string& outPath) {
    std::ifstream in(jsonPath, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "error: cannot read %s\n", jsonPath.c_str());
        return 1;
    }
    std::ostringstream text;
    text << in.rdbuf();
    const auto outcome = observation::fromJson(text.str());
    if (!outcome.ok()) {
        std::fprintf(stderr, "error: %s\n", outcome.error.c_str());
        return 1;
    }
    // Thumbnail from the record's image artifact beside the JSON, if present.
    std::optional<apps::CardImage> thumbnail;
    for (const auto& artifact : outcome.record->artifacts) {
        if (artifact.kind.rfind("image/x-portable-", 0) == 0) {
            thumbnail =
                apps::loadCardImage(std::filesystem::path(jsonPath).parent_path() / artifact.path);
            break;
        }
    }
    return renderCard(*outcome.record, outPath, thumbnail);
}

int renderScoutCardDemo(const std::string& outPath, const std::string& kind) {
    // Synthetic calibrated scenes via the shared builder: a 20 mm reference
    // square at 0.5 mm/px plus either an M12-class rod or an M6 nut.
    hal::testing::SyntheticScene scene;
    scene.addSquare(50, 50, 40);
    if (kind == "nut") {
        scene.addHexagon(400.0, 280.0, 20.0, 15.0 * 3.14159265358979 / 180.0);
        scene.addBore(400.0, 280.0, 5.0);
    } else {
        scene.addBolt(400.0, 280.0, 240.0, 24.0, 30.0 * 3.14159265358979 / 180.0, 36.0, 16.0);
    }

    const vision::CalibrationSpec spec{20.0};
    const auto analyzed = vision::analyzeFrame(scene.frame(), spec);
    if (!analyzed.ok()) {
        std::fprintf(stderr, "error: demo analysis failed: %.*s\n",
                     static_cast<int>(vision::to_string(analyzed.error).size()),
                     vision::to_string(analyzed.error).data());
        return 1;
    }

    observation::EngineeringObservation record;
    record.observationId = kind == "nut" ? "scan-0008" : "scan-0007";
    record.timestampUtc = "2026-08-31T09:41:00Z";
    record.source = {{"app", "ui_preview"}, {"camera", "synthetic"}};
    record.artifacts.push_back({"source-image", "image/x-portable-graymap", "source.pgm"});
    vision::appendEvidence(record, *analyzed.analysis, spec, "source-image");
    ai::appendClassification(record, ai::classify(*analyzed.analysis));

    apps::CardImage thumbnail;
    const auto frame2 = scene.frame();
    thumbnail.width = frame2.mode().width;
    thumbnail.height = frame2.mode().height;
    thumbnail.channels = 1;
    const auto scenePixels = frame2.pixels();
    thumbnail.pixels.resize(scenePixels.size());
    for (std::size_t i = 0; i < scenePixels.size(); ++i)
        thumbnail.pixels[i] = std::to_integer<std::uint8_t>(scenePixels[i]);
    return renderCard(record, outPath, thumbnail);
}

int renderKioskSessionDemo(const std::string& outDir) {
    namespace fs = std::filesystem;
    const fs::path root(outDir);
    std::error_code ec;
    fs::remove_all(root / "observations", ec);
    fs::remove_all(root / "sessions", ec);
    fs::create_directories(root, ec);

    auto display = std::make_shared<MemoryDisplay>(std::uint16_t{800}, std::uint16_t{480});
    renderer::Renderer r(display);
    const auto layout = kiosk::computeLayout(r.displayInfo(), 640, 480);
    kiosk::SceneCamera camera;
    (void)camera.open(kiosk::SceneCamera::kMode);
    observation::CaptureService service(root / "observations");
    session::SessionStore store(root / "sessions");
    session::ObjectSession current(store.nextSessionId(), session::fastenerProfile(),
                                   "2026-09-30T02:00:00Z");
    const vision::CalibrationSpec spec{20.0};
    std::string status;
    renderer::Color statusColour = kiosk::kMuted;
    int shot = 0;

    const auto snap = [&](const std::string& name) {
        (void)r.present();
        char file[64];
        std::snprintf(file, sizeof(file), "%02d-%s.ppm", ++shot, name.c_str());
        return writePpm((root / file).string(), *display);
    };
    const auto preview = [&](const std::string& name) {
        std::vector<std::uint8_t> rgb;
        if (auto f = camera.capture(std::chrono::milliseconds(0)); f)
            rgb = kiosk::yuyvToRgb(f.value());
        kiosk::PanelState p;
        p.sessionId = current.id();
        p.guidance = current.guidance();
        p.status = status;
        p.statusColour = statusColour;
        kiosk::drawPreviewScreen(r, layout, rgb, 640, 480, p);
        return snap(name);
    };
    const auto capture = [&](int pose, const std::string& cardName) {
        camera.setPose(pose);
        const auto outcome = kiosk::captureOnce(camera, service, spec, "synthetic", "synthetic");
        if (!outcome.record) return false;
        current.add(kiosk::sessionCapture(outcome, current.profile()));
        (void)store.save(current);
        status = outcome.status;
        statusColour = outcome.statusColour;
        if (outcome.measured && !cardName.empty()) {
            apps::drawObservationCard(r, *outcome.record, outcome.thumbnail);
            if (!snap(cardName)) return false;
        }
        return true;
    };

    bool ok = preview("new-session");
    ok = ok && capture(0, "card") && preview("after-1");
    ok = ok && capture(-1, "") && preview("refused-no-part");  // part removed
    ok = ok && capture(1, "") && capture(2, "") && preview("stable");
    const auto g = current.guidance();
    const auto reason = g.modelSatisfied ? session::CloseReason::ModelSatisfied
                                         : session::CloseReason::ClosedByOperator;
    const auto result = store.finish(current, reason, "2026-09-30T02:05:00Z");
    kiosk::drawSessionSummary(r, current, result, reason);
    ok = ok && snap("summary");
    if (!ok || !result.ok()) {
        std::fprintf(stderr, "error: demo failed %s\n", result.error.c_str());
        return 1;
    }
    std::printf("wrote %d screens and %s\n", shot, result.directory.string().c_str());
    return 0;
}

/// Re-analyze a frame the board saved (640x480 YUYV source.yuyv) with this
/// build's analyzer + classifier and print the record: lets a change be
/// checked against real bench captures without the board.
int analyzeYuyv(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    auto bytes = std::make_shared<std::vector<std::byte>>(640u * 480u * 2u);
    if (!in.read(reinterpret_cast<char*>(bytes->data()),
                 static_cast<std::streamsize>(bytes->size()))) {
        std::fprintf(stderr, "error: %s is not a 640x480 YUYV frame\n", path.c_str());
        return 1;
    }
    const hal::Frame frame({640, 480, hal::PixelFormat::YUYV, 30.0f}, bytes,
                           std::chrono::steady_clock::time_point{});
    const vision::CalibrationSpec spec{20.0};
    const auto analyzed = vision::analyzeFrame(frame, spec);
    if (!analyzed.ok()) {
        const auto why = vision::to_string(analyzed.error);
        std::printf("refused: %.*s\n", static_cast<int>(why.size()), why.data());
        return 0;
    }
    observation::EngineeringObservation record;
    record.observationId = "reanalysis";
    record.timestampUtc = "1970-01-01T00:00:00Z";
    record.source = {{"app", "ui_preview"}};
    record.artifacts.push_back({"source-image", "image/x-yuyv", path});
    vision::appendEvidence(record, *analyzed.analysis, spec, "source-image");
    ai::appendClassification(record, ai::classify(*analyzed.analysis));
    std::printf("%s\n", observation::toJson(record).c_str());
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    if (args.size() == 2 && args[0] == "--font-specimen") return renderFontSpecimen(args[1]);
    if (args.size() == 3 && args[0] == "--scout-card") return renderScoutCard(args[1], args[2]);
    if (args.size() >= 2 && args.size() <= 3 && args[0] == "--scout-card-demo")
        return renderScoutCardDemo(args[1], args.size() == 3 ? args[2] : "bolt");
    if (args.size() == 2 && args[0] == "--kiosk-session-demo")
        return renderKioskSessionDemo(args[1]);
    if (args.size() == 2 && args[0] == "--analyze-yuyv") return analyzeYuyv(args[1]);
    std::fprintf(stderr,
                 "usage: ui_preview --font-specimen OUT.ppm\n"
                 "       ui_preview --scout-card RECORD.json OUT.ppm\n"
                 "       ui_preview --scout-card-demo OUT.ppm [bolt|nut]\n"
                 "       ui_preview --kiosk-session-demo OUTDIR\n"
                 "       ui_preview --analyze-yuyv SOURCE.yuyv\n");
    return 2;
}
