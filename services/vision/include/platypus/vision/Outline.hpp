// PlatypusOS vision — silhouette outlines for CAD handoff.
//
// Turns one labelled blob into a closed polygon (plus holes) in pixel
// coordinates, so a measured part can leave PlatypusOne as a sketch: the
// session exporter scales it by the capture's mm/px and writes it for
// mesh2cad's Outline Forge and for Fusion 360 (services/export).
//
// Marching squares over the blob's mask, with vertices on the midpoints
// between pixel centres — i.e. on the pixel boundaries — so the outline
// encloses the same area the blob's pixel count measures, and a loop is
// produced for every hole as well as the outer edge. Diagonal-only pixel
// contacts are treated as separate (the labeller is 4-connected), which
// resolves the two ambiguous saddle cases consistently.
#pragma once

#include <platypus/geometry/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace platypus::vision {

/// Every closed boundary loop of the pixels whose label equals `label`,
/// searched within the inclusive bounding box [minX, maxX] x [minY, maxY].
/// Loops are unsimplified and unordered; see outlineOf() for the usual entry.
[[nodiscard]] std::vector<std::vector<geometry::Vec2>> traceBoundaryLoops(
    const std::vector<std::int32_t>& labels, std::int32_t width, std::int32_t height,
    std::int32_t label, std::int32_t minX, std::int32_t minY, std::int32_t maxX, std::int32_t maxY);

/// Signed shoelace area (positive for counter-clockwise in y-up axes).
[[nodiscard]] double signedArea(const std::vector<geometry::Vec2>& loop) noexcept;

/// Douglas-Peucker simplification of a closed loop: removes points closer
/// than `tolerance` to the chord between their neighbours. Keeps >= 3 points.
[[nodiscard]] std::vector<geometry::Vec2> simplifyClosed(const std::vector<geometry::Vec2>& loop,
                                                         double tolerance);

/// The blob's outline: the largest loop as `outer`, and every other loop
/// whose area is at least `minHoleAreaPx` as a hole — small holes are glints
/// on bright metal, not bores, and must not reach a CAD sketch. Each loop is
/// simplified with `tolerancePx`.
[[nodiscard]] geometry::Outline2 outlineOf(const std::vector<std::int32_t>& labels,
                                           std::int32_t width, std::int32_t height,
                                           std::int32_t label, std::int32_t minX, std::int32_t minY,
                                           std::int32_t maxX, std::int32_t maxY,
                                           double minHoleAreaPx, double tolerancePx = 0.5);

}  // namespace platypus::vision
