// PlatypusOS services — core geometry types shared by vision, measurement
// and export services. Header-only, dependency-free.
#pragma once

#include <array>
#include <cmath>
#include <vector>

namespace platypus::geometry {

struct Vec2 {
    float x = 0, y = 0;
};
struct Vec3 {
    float x = 0, y = 0, z = 0;
    [[nodiscard]] float length() const noexcept { return std::sqrt(x * x + y * y + z * z); }
};

/// A closed 2D shape: one outer boundary plus zero or more holes (bores).
/// Loops are closed implicitly — the last point connects back to the first.
/// Units are the producer's (pixels from vision, millimetres after scaling);
/// types stay unit-free so one outline flows from camera to CAD export.
struct Outline2 {
    std::vector<Vec2> outer;
    std::vector<std::vector<Vec2>> holes;
    [[nodiscard]] bool empty() const noexcept { return outer.size() < 3; }
};

struct PointCloud {
    std::vector<Vec3> points;
};

struct Mesh {
    std::vector<Vec3> vertices;
    std::vector<std::array<std::uint32_t, 3>> triangles;
};

}  // namespace platypus::geometry
