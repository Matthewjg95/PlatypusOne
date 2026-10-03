#include "platypus/vision/Outline.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>

namespace platypus::vision {

using geometry::Vec2;

namespace {

/// Vertices sit on midpoints between pixel centres, so doubling a coordinate
/// makes it an exact integer; the pair packs into one hashable key.
std::uint64_t key(std::int32_t x2, std::int32_t y2) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x2)) << 32) |
           static_cast<std::uint32_t>(y2);
}

/// Rotates a loop to start at its top-most, then left-most vertex, so the
/// same shape always simplifies to the same points on every platform.
void canonicalStart(std::vector<Vec2>& loop) {
    if (loop.empty()) return;
    const auto first = std::min_element(loop.begin(), loop.end(), [](const Vec2& a, const Vec2& b) {
        return a.y < b.y || (a.y == b.y && a.x < b.x);
    });
    std::rotate(loop.begin(), first, loop.end());
}

double distanceToSegment(const Vec2& p, const Vec2& a, const Vec2& b) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double len2 = dx * dx + dy * dy;
    if (len2 <= 0.0) return std::hypot(p.x - a.x, p.y - a.y);
    const double t = std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / len2, 0.0, 1.0);
    return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}

/// Douglas-Peucker on the open chain pts[first..last] (inclusive), marking the
/// points to keep. Iterative, so a long outline cannot overflow the stack.
void douglasPeucker(const std::vector<Vec2>& pts, std::size_t first, std::size_t last,
                    double tolerance, std::vector<bool>& keep) {
    std::vector<std::pair<std::size_t, std::size_t>> stack{{first, last}};
    while (!stack.empty()) {
        const auto [a, b] = stack.back();
        stack.pop_back();
        double worst = -1.0;
        std::size_t index = a;
        for (std::size_t i = a + 1; i < b; ++i) {
            const double d = distanceToSegment(pts[i], pts[a], pts[b]);
            if (d > worst) {
                worst = d;
                index = i;
            }
        }
        if (worst > tolerance) {
            keep[index] = true;
            stack.push_back({a, index});
            stack.push_back({index, b});
        }
    }
}

}  // namespace

std::vector<std::vector<Vec2>> traceBoundaryLoops(const std::vector<std::int32_t>& labels,
                                                  std::int32_t width, std::int32_t height,
                                                  std::int32_t label, std::int32_t minX,
                                                  std::int32_t minY, std::int32_t maxX,
                                                  std::int32_t maxY) {
    const auto inside = [&](std::int32_t x, std::int32_t y) {
        if (x < 0 || y < 0 || x >= width || y >= height) return false;
        return labels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                      static_cast<std::size_t>(x)] == label;
    };

    // Every boundary vertex has exactly two incident segments; the insertion
    // order is kept so loops are discovered in scan order, deterministically.
    std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> adjacency;
    std::unordered_map<std::uint64_t, std::pair<std::int32_t, std::int32_t>> position;
    std::vector<std::uint64_t> order;
    const auto link = [&](std::int32_t ax, std::int32_t ay, std::int32_t bx, std::int32_t by) {
        const auto ka = key(ax, ay), kb = key(bx, by);
        for (const auto& [k, xy] :
             {std::pair{ka, std::pair{ax, ay}}, std::pair{kb, std::pair{bx, by}}})
            if (position.emplace(k, xy).second) order.push_back(k);
        adjacency[ka].push_back(kb);
        adjacency[kb].push_back(ka);
    };

    // A cell spans the pixel centres (x, y)..(x+1, y+1); scanning one cell
    // beyond the box on every side closes loops that touch the box edge.
    for (std::int32_t y = minY - 1; y <= maxY; ++y)
        for (std::int32_t x = minX - 1; x <= maxX; ++x) {
            const int tl = inside(x, y), tr = inside(x + 1, y);
            const int br = inside(x + 1, y + 1), bl = inside(x, y + 1);
            const int cell = tl * 8 + tr * 4 + br * 2 + bl;
            if (cell == 0 || cell == 15) continue;
            // Edge midpoints, doubled: top, right, bottom, left.
            const std::int32_t tx = 2 * x + 1, ty = 2 * y;
            const std::int32_t rx = 2 * x + 2, ry = 2 * y + 1;
            const std::int32_t bx = 2 * x + 1, by = 2 * y + 2;
            const std::int32_t lx = 2 * x, ly = 2 * y + 1;
            switch (cell) {
                case 1:
                    link(lx, ly, bx, by);
                    break;
                case 2:
                    link(bx, by, rx, ry);
                    break;
                case 3:
                    link(lx, ly, rx, ry);
                    break;
                case 4:
                    link(tx, ty, rx, ry);
                    break;
                case 5:  // tr + bl only: diagonal contact, two separate corners
                    link(tx, ty, rx, ry);
                    link(lx, ly, bx, by);
                    break;
                case 6:
                    link(tx, ty, bx, by);
                    break;
                case 7:
                    link(tx, ty, lx, ly);
                    break;
                case 8:
                    link(tx, ty, lx, ly);
                    break;
                case 9:
                    link(tx, ty, bx, by);
                    break;
                case 10:  // tl + br only: diagonal contact, two separate corners
                    link(tx, ty, lx, ly);
                    link(bx, by, rx, ry);
                    break;
                case 11:
                    link(tx, ty, rx, ry);
                    break;
                case 12:
                    link(lx, ly, rx, ry);
                    break;
                case 13:
                    link(rx, ry, bx, by);
                    break;
                case 14:
                    link(lx, ly, bx, by);
                    break;
                default:
                    break;
            }
        }

    std::vector<std::vector<Vec2>> loops;
    std::unordered_map<std::uint64_t, bool> visited;
    for (const auto start : order) {
        if (visited[start]) continue;
        std::vector<Vec2> loop;
        std::uint64_t previous = start, current = start;
        bool first = true;
        while (true) {
            visited[current] = true;
            const auto& [x2, y2] = position[current];
            loop.push_back({static_cast<float>(x2) / 2.0f, static_cast<float>(y2) / 2.0f});
            const auto& next = adjacency[current];
            if (next.size() != 2) break;  // cannot happen on a closed boundary; be safe
            const auto step = (first || next[0] != previous) ? next[0] : next[1];
            first = false;
            previous = current;
            current = step;
            if (current == start) break;
        }
        if (loop.size() >= 3) loops.push_back(std::move(loop));
    }
    return loops;
}

double signedArea(const std::vector<Vec2>& loop) noexcept {
    double twice = 0.0;
    for (std::size_t i = 0, n = loop.size(); i < n; ++i) {
        const auto& a = loop[i];
        const auto& b = loop[(i + 1) % n];
        twice += static_cast<double>(a.x) * b.y - static_cast<double>(b.x) * a.y;
    }
    return twice / 2.0;
}

std::vector<Vec2> simplifyClosed(const std::vector<Vec2>& loop, double tolerance) {
    if (loop.size() <= 3 || tolerance <= 0.0) return loop;
    // Split the ring at its first point and the point farthest from it, and
    // simplify each half as an open chain.
    std::size_t far = 0;
    double farthest = -1.0;
    for (std::size_t i = 1; i < loop.size(); ++i) {
        const double d = std::hypot(loop[i].x - loop[0].x, loop[i].y - loop[0].y);
        if (d > farthest) {
            farthest = d;
            far = i;
        }
    }
    std::vector<Vec2> ring(loop);
    ring.push_back(loop[0]);
    std::vector<bool> keep(ring.size(), false);
    keep[0] = keep[far] = keep[ring.size() - 1] = true;
    douglasPeucker(ring, 0, far, tolerance, keep);
    douglasPeucker(ring, far, ring.size() - 1, tolerance, keep);

    std::vector<Vec2> out;
    for (std::size_t i = 0; i + 1 < ring.size(); ++i)
        if (keep[i]) out.push_back(ring[i]);
    return out.size() >= 3 ? out : loop;
}

geometry::Outline2 outlineOf(const std::vector<std::int32_t>& labels, std::int32_t width,
                             std::int32_t height, std::int32_t label, std::int32_t minX,
                             std::int32_t minY, std::int32_t maxX, std::int32_t maxY,
                             double minHoleAreaPx, double tolerancePx) {
    auto loops = traceBoundaryLoops(labels, width, height, label, minX, minY, maxX, maxY);
    geometry::Outline2 outline;
    if (loops.empty()) return outline;

    // Largest first: the outer boundary, then holes by size.
    std::sort(loops.begin(), loops.end(), [](const auto& a, const auto& b) {
        return std::abs(signedArea(a)) > std::abs(signedArea(b));
    });
    for (std::size_t i = 0; i < loops.size(); ++i) {
        auto& loop = loops[i];
        if (i > 0 && std::abs(signedArea(loop)) < minHoleAreaPx) continue;  // a glint, not a bore
        canonicalStart(loop);
        auto simplified = simplifyClosed(loop, tolerancePx);
        // Consistent winding: outer positive, holes negative (image axes).
        const bool positive = signedArea(simplified) > 0.0;
        if ((i == 0) != positive) std::reverse(simplified.begin(), simplified.end());
        if (i == 0)
            outline.outer = std::move(simplified);
        else
            outline.holes.push_back(std::move(simplified));
    }
    return outline;
}

}  // namespace platypus::vision
