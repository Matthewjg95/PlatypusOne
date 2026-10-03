// Silhouette outlines: the geometry a measured part leaves PlatypusOne with.
// A CAD sketch built from these must enclose what was measured, carry real
// bores, drop glints, and come out identical on every run and platform.
#include <platypus/hal/testing/SyntheticScene.hpp>
#include <platypus/vision/Outline.hpp>
#include <platypus/vision/ScoutAnalyzer.hpp>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

using namespace platypus;
using hal::testing::SyntheticScene;

constexpr double kPi = 3.14159265358979;

void test_square_boundary_sits_on_pixel_edges() {
    // A 10x10 block of label 7 at (5, 5) in a 30x30 image.
    constexpr std::int32_t w = 30, h = 30;
    std::vector<std::int32_t> labels(w * h, -1);
    for (int y = 5; y < 15; ++y)
        for (int x = 5; x < 15; ++x)
            labels[static_cast<std::size_t>(y * w + x)] = 7;

    const auto loops = vision::traceBoundaryLoops(labels, w, h, 7, 5, 5, 14, 14);
    assert(loops.size() == 1);
    // Vertices on pixel boundaries: the loop spans 4.5 .. 14.5 on both axes,
    // and encloses the pixel count less four cut corners of 1/8 px^2 each.
    float minX = 1e9f, maxX = -1e9f;
    for (const auto& p : loops[0]) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
    }
    assert(minX == 4.5f && maxX == 14.5f);
    assert(std::abs(std::abs(vision::signedArea(loops[0])) - (100.0 - 0.5)) < 1e-6);
}

void test_simplification_keeps_the_shape() {
    // A staircase approximating a 45-degree edge collapses to few points
    // without moving the enclosed area by more than the tolerance allows.
    std::vector<geometry::Vec2> loop;
    for (int i = 0; i <= 20; ++i) {
        loop.push_back({static_cast<float>(i), static_cast<float>(i)});
        loop.push_back({static_cast<float>(i + 1), static_cast<float>(i)});
    }
    loop.push_back({21.0f, 0.0f});
    const auto simplified = vision::simplifyClosed(loop, 0.75);
    assert(simplified.size() < loop.size() / 4);
    assert(std::abs(vision::signedArea(simplified) - vision::signedArea(loop)) <
           0.05 * std::abs(vision::signedArea(loop)));
}

/// 20 mm reference at 0.5 mm/px (40 px), so 1 px^2 = 0.25 mm^2.
SyntheticScene withReference() {
    SyntheticScene scene;
    scene.addSquare(50, 50, 40);
    return scene;
}

void test_analysis_outline_encloses_the_measured_part() {
    auto scene = withReference();
    scene.addRect(400.0, 280.0, 240.0, 36.0, 25.0 * kPi / 180.0);
    const auto outcome = vision::analyzeFrame(scene.frame(), {20.0});
    assert(outcome.ok());
    const auto& a = *outcome.analysis;
    assert(!a.subjectOutlinePx.empty());
    assert(a.subjectOutlinePx.holes.empty());
    // Same area as the blob the measurement came from, within 1 %.
    const double area = std::abs(vision::signedArea(a.subjectOutlinePx.outer));
    const double pixels = static_cast<double>(a.subject.areaPx);
    assert(std::abs(area - pixels) < 0.01 * pixels);
    // Simplified: the raw boundary of this rotated rectangle is ~730 pixel
    // steps; at 0.5 px tolerance it drops ~10x (measured: 70 points) while the
    // enclosed area moves by ~0.05 %. Fidelity is preferred over fewer points:
    // a CAD sketch can take thousands, but not a shape with its corners cut.
    assert(a.subjectOutlinePx.outer.size() < 120);
}

void test_bores_are_kept_and_glints_are_not() {
    // A nut: hexagon (40 px = 20 mm across flats) with a 5 px (2.5 mm) bore,
    // 19.6 mm^2 — a real feature for a CAD sketch.
    {
        auto scene = withReference();
        scene.addHexagon(400.0, 280.0, 40.0);
        scene.addBore(400.0, 280.0, 5.0);
        const auto outcome = vision::analyzeFrame(scene.frame(), {20.0});
        assert(outcome.ok());
        const auto& outline = outcome.analysis->subjectOutlinePx;
        assert(outline.holes.size() == 1);
        const double bore = std::abs(vision::signedArea(outline.holes[0]));
        assert(std::abs(bore - kPi * 25.0) < 0.25 * kPi * 25.0);
        // Winding is consistent: outer and holes wind opposite ways.
        assert((vision::signedArea(outline.outer) > 0) !=
               (vision::signedArea(outline.holes[0]) > 0));
    }
    // A glint: a 1.5 px (~1.8 mm^2) hole in a shaft is counted by the blob
    // statistics but never exported as a feature.
    {
        auto scene = withReference();
        scene.addRect(400.0, 280.0, 240.0, 36.0, 0.0);
        scene.addBore(400.0, 280.0, 1.5);
        const auto outcome = vision::analyzeFrame(scene.frame(), {20.0});
        assert(outcome.ok());
        assert(outcome.analysis->subject.holeCount == 1);
        assert(outcome.analysis->subjectOutlinePx.holes.empty());
    }
}

void test_outline_is_deterministic() {
    auto scene = withReference();
    scene.addHexagon(400.0, 280.0, 40.0, 0.3);
    scene.addBore(400.0, 280.0, 6.0);
    const auto a = vision::analyzeFrame(scene.frame(), {20.0});
    const auto b = vision::analyzeFrame(scene.frame(), {20.0});
    assert(a.ok() && b.ok());
    const auto& oa = a.analysis->subjectOutlinePx;
    const auto& ob = b.analysis->subjectOutlinePx;
    assert(oa.outer.size() == ob.outer.size() && oa.holes.size() == ob.holes.size());
    for (std::size_t i = 0; i < oa.outer.size(); ++i)
        assert(oa.outer[i].x == ob.outer[i].x && oa.outer[i].y == ob.outer[i].y);
}

}  // namespace

void test_outline() {
    test_square_boundary_sits_on_pixel_edges();
    test_simplification_keeps_the_shape();
    test_analysis_outline_encloses_the_measured_part();
    test_bores_are_kept_and_glints_are_not();
    test_outline_is_deterministic();
    std::puts("test_outline: OK");
}
