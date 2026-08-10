// PlatypusOS services — core geometry types shared by vision, measurement
// and export services. Header-only, dependency-free.
#pragma once

#include <array>
#include <cmath>
#include <vector>

namespace platypus::geometry {

struct Vec2 { float x = 0, y = 0; };
struct Vec3 {
    float x = 0, y = 0, z = 0;
    [[nodiscard]] float length() const noexcept { return std::sqrt(x * x + y * y + z * z); }
};

struct PointCloud {
    std::vector<Vec3> points;
};

struct Mesh {
    std::vector<Vec3> vertices;
    std::vector<std::array<std::uint32_t, 3>> triangles;
};

}  // namespace platypus::geometry
