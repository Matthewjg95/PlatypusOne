// ui_preview — offline renders of PlatypusOS UI surfaces for visual review.
//
//   ui_preview --font-specimen OUT.ppm
//   ui_preview --scout-card RECORD.json OUT.ppm
//   ui_preview --scout-card-demo OUT.ppm
//
// Draws through the real Renderer into an in-memory display and writes the
// frame as a P6 PPM. A bring-up tool (like engineering_scout_capture): UI
// changes get eyeballed from a file without opening the interactive sim.
// --scout-card-demo runs the real analyzer + classifier over a synthetic
// bolt scene so the card shows a genuine end-to-end record.
#include <platypus/ai/FastenerClassifier.hpp>
#include <platypus/apps/EngineeringScoutApp.hpp>
#include <platypus/renderer/Renderer.hpp>
#include <platypus/vision/ScoutAnalyzer.hpp>

#include <cmath>
#include <cstdio>
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

int renderCard(const observation::EngineeringObservation& record, const std::string& outPath) {
    auto display = std::make_shared<MemoryDisplay>(std::uint16_t{480}, std::uint16_t{320});
    renderer::Renderer r(display);
    apps::drawObservationCard(r, record);
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
    return renderCard(*outcome.record, outPath);
}

int renderScoutCardDemo(const std::string& outPath) {
    // Synthetic calibrated bolt scene, identical in spirit to the test suite:
    // 40 px / 20 mm reference square + a 240x24 px rod at 30 degrees.
    constexpr std::uint16_t kW = 640;
    constexpr std::uint16_t kH = 480;
    auto pixels =
        std::make_shared<std::vector<std::byte>>(static_cast<std::size_t>(kW) * kH, std::byte{235});
    const auto darken = [&](int x, int y) {
        if (x >= 0 && y >= 0 && x < kW && y < kH)
            (*pixels)[static_cast<std::size_t>(y) * kW + static_cast<std::size_t>(x)] =
                std::byte{20};
    };
    for (int dy = 0; dy < 40; ++dy)
        for (int dx = 0; dx < 40; ++dx)
            darken(50 + dx, 50 + dy);
    const double angle = 30.0 * 3.14159265358979 / 180.0;
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    for (int y = 0; y < kH; ++y)
        for (int x = 0; x < kW; ++x) {
            const double dx = x - 400.0;
            const double dy = y - 280.0;
            const double u = c * dx + s * dy;
            const double v = -s * dx + c * dy;
            if (std::abs(u) <= 120.0 && std::abs(v) <= 12.0) darken(x, y);
        }

    const hal::Frame frame({kW, kH, hal::PixelFormat::Gray8, 30.0f}, pixels,
                           std::chrono::steady_clock::time_point{});
    const vision::CalibrationSpec spec{20.0};
    const auto analyzed = vision::analyzeFrame(frame, spec);
    if (!analyzed.ok()) {
        std::fprintf(stderr, "error: demo analysis failed: %.*s\n",
                     static_cast<int>(vision::to_string(analyzed.error).size()),
                     vision::to_string(analyzed.error).data());
        return 1;
    }

    observation::EngineeringObservation record;
    record.observationId = "scan-0007";
    record.timestampUtc = "2026-08-31T09:41:00Z";
    record.source = {{"app", "ui_preview"}, {"camera", "synthetic"}};
    record.artifacts.push_back({"source-image", "image/x-portable-graymap", "source.pgm"});
    vision::appendEvidence(record, *analyzed.analysis, spec, "source-image");
    ai::appendClassification(record, ai::classify(*analyzed.analysis));
    return renderCard(record, outPath);
}

}  // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    if (args.size() == 2 && args[0] == "--font-specimen") return renderFontSpecimen(args[1]);
    if (args.size() == 3 && args[0] == "--scout-card") return renderScoutCard(args[1], args[2]);
    if (args.size() == 2 && args[0] == "--scout-card-demo") return renderScoutCardDemo(args[1]);
    std::fprintf(stderr,
                 "usage: ui_preview --font-specimen OUT.ppm\n"
                 "       ui_preview --scout-card RECORD.json OUT.ppm\n"
                 "       ui_preview --scout-card-demo OUT.ppm\n");
    return 2;
}
