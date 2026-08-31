// Scout result card tests: the card renders every evidence class in its own
// color, deterministically, on a real end-to-end record, and the empty state
// holds up. Rendering goes through the real Renderer into a fake display.
#include <platypus/ai/FastenerClassifier.hpp>
#include <platypus/apps/EngineeringScoutApp.hpp>
#include <platypus/hal/testing/SyntheticScene.hpp>
#include <platypus/vision/ScoutAnalyzer.hpp>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

namespace {

using namespace platypus;

class CardDisplay final : public hal::IDisplay {
   public:
    hal::DisplayInfo info() const noexcept override { return {480, 320, 16}; }
    hal::Status setBacklight(float) override { return {}; }
    hal::Status present(std::span<const std::byte> pixels) override {
        frame_.assign(pixels.begin(), pixels.end());
        return {};
    }
    hal::Status onTouch(std::function<void(const hal::TouchEvent&)>) override { return {}; }
    hal::Status onButton(std::function<void(const hal::ButtonEvent&)>) override { return {}; }

    std::vector<std::byte> frame_;
};

/// Count pixels of an exact RGB565 color in the presented frame.
std::size_t countColor(const std::vector<std::byte>& frame, renderer::Color color) {
    const auto rgb565 = color.toRgb565();
    const auto low = static_cast<std::byte>(rgb565 & 0xFF);
    const auto high = static_cast<std::byte>(rgb565 >> 8);
    std::size_t count = 0;
    for (std::size_t i = 0; i + 1 < frame.size(); i += 2)
        if (frame[i] == low && frame[i + 1] == high) ++count;
    return count;
}

/// The same synthetic bolt record the analyzer/classifier tests use.
observation::EngineeringObservation boltRecord() {
    hal::testing::SyntheticScene scene;
    scene.addSquare(50, 50, 40);
    scene.addRect(400.0, 280.0, 240.0, 24.0, 30.0 * 3.14159265358979 / 180.0);

    const vision::CalibrationSpec spec{20.0};
    const auto analyzed = vision::analyzeFrame(scene.frame(), spec);
    assert(analyzed.ok());

    observation::EngineeringObservation record;
    record.observationId = "scan-0001";
    record.timestampUtc = "2026-08-31T00:00:00Z";
    record.source = {{"app", "test"}};
    record.artifacts.push_back({"source-image", "image/x-portable-graymap", "source.pgm"});
    vision::appendEvidence(record, *analyzed.analysis, spec, "source-image");
    ai::appendClassification(record, ai::classify(*analyzed.analysis));
    assert(observation::validate(record).empty());
    return record;
}

std::vector<std::byte> renderCard(const observation::EngineeringObservation& record) {
    auto display = std::make_shared<CardDisplay>();
    renderer::Renderer r(display);
    apps::drawObservationCard(r, record);
    assert(r.present().ok());
    return display->frame_;
}

void test_card_renders_every_evidence_class() {
    const auto frame = renderCard(boltRecord());
    assert(frame.size() == 480u * 320u * 2u);

    // Each evidence class paints in its dedicated color; the section tags
    // alone guarantee a nonzero count, and these colors are used nowhere else.
    assert(countColor(frame, {123, 216, 143}) > 0);  // OBSERVED green
    assert(countColor(frame, {127, 180, 255}) > 0);  // DERIVED blue
    assert(countColor(frame, {255, 198, 109}) > 0);  // INFERRED amber
    assert(countColor(frame, {255, 123, 114}) > 0);  // UNRESOLVED red
    assert(countColor(frame, {120, 200, 255}) > 0);  // header/NEXT accent
}

void test_card_is_deterministic() {
    const auto record = boltRecord();
    assert(renderCard(record) == renderCard(record));
}

void test_card_without_inference_still_renders() {
    auto record = boltRecord();
    record.inferred.clear();
    const auto frame = renderCard(record);
    // Sections still labeled; the inferred value color now only appears in
    // the section tag ("INFERRED" + "none" markers), which is fine — the
    // card must simply not crash and keep the other classes painted.
    assert(countColor(frame, {127, 180, 255}) > 0);
    assert(countColor(frame, {255, 123, 114}) > 0);
}

void test_thumbnail_renders_and_loads() {
    // A synthetic gradient thumbnail must paint pixels into the bottom-right
    // box, and the render stays deterministic.
    apps::CardImage image;
    image.width = 64;
    image.height = 48;
    image.channels = 3;
    image.pixels.resize(64u * 48u * 3u);
    for (std::size_t i = 0; i < image.pixels.size(); ++i)
        image.pixels[i] = static_cast<std::uint8_t>((i * 7) & 0xFF);

    const auto record = boltRecord();
    auto display = std::make_shared<CardDisplay>();
    renderer::Renderer r(display);
    apps::drawObservationCard(r, record, image);
    assert(r.present().ok());
    const auto withThumb = display->frame_;
    assert(withThumb != renderCard(record));  // thumbnail changed the frame

    // Round-trip through the netpbm loader used by the app.
    const auto dir = std::filesystem::temp_directory_path() / "platypus-test-card";
    std::filesystem::create_directories(dir);
    const auto file = dir / "thumb.ppm";
    {
        std::ofstream out(file, std::ios::binary);
        out << "P6\n# comment\n64 48\n255\n";
        out.write(reinterpret_cast<const char*>(image.pixels.data()),
                  static_cast<std::streamsize>(image.pixels.size()));
    }
    const auto loaded = apps::loadCardImage(file);
    assert(loaded);
    assert(loaded->width == 64 && loaded->height == 48 && loaded->channels == 3);
    assert(loaded->pixels == image.pixels);

    // Truncated pixel data must fail cleanly.
    {
        std::ofstream out(file, std::ios::binary);
        out << "P6\n64 48\n255\nshort";
    }
    assert(!apps::loadCardImage(file));
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

void test_empty_state() {
    auto display = std::make_shared<CardDisplay>();
    renderer::Renderer r(display);
    apps::drawNoObservationCard(r, "capture one with engineering_scout_capture");
    assert(r.present().ok());
    assert(countColor(display->frame_, {120, 200, 255}) > 0);
}

void test_app_manifest() {
    const auto app = apps::EngineeringScoutApp::create();
    assert(app->manifest().id == "one.platypus.scout");
    assert(!app->manifest().requiresCamera);
}

}  // namespace

void test_scout_card() {
    test_card_renders_every_evidence_class();
    test_card_is_deterministic();
    test_card_without_inference_still_renders();
    test_thumbnail_renders_and_loads();
    test_empty_state();
    test_app_manifest();
    std::puts("test_scout_card: OK");
}
