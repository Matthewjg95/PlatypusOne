#include "platypus/vision/ScoutAnalyzer.hpp"

#include "platypus/vision/Outline.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace platypus::vision {

using hal::PixelFormat;

std::string_view to_string(AnalyzeError error) noexcept {
    switch (error) {
        case AnalyzeError::None:
            return "None";
        case AnalyzeError::InvalidFrame:
            return "InvalidFrame";
        case AnalyzeError::UnsupportedFormat:
            return "UnsupportedFormat";
        case AnalyzeError::NoReferenceTarget:
            return "NoReferenceTarget";
        case AnalyzeError::ReferenceAmbiguous:
            return "ReferenceAmbiguous";
        case AnalyzeError::NoSubject:
            return "NoSubject";
    }
    return "Unknown";
}

namespace {

/// Blobs smaller than this are sensor noise, not scene objects.
constexpr std::size_t kMinBlobAreaPx = 64;
/// Square-reference gates (see the header's scene contract).
constexpr double kMinReferenceAspect = 0.90;
constexpr double kMinReferenceFill = 0.85;
/// A second square candidate at least this fraction of the best one's area
/// makes the reference ambiguous instead of silently picking one.
constexpr double kAmbiguityAreaRatio = 0.5;
/// Smallest physical silhouette accepted as the subject. The MVP scope is M3
/// and up; the smallest in-scope part, an M3 nut, is ~26 mm^2 seen flat.
/// Anything under 10 mm^2 is dust, a glint or a print flaw — and with the
/// fastener out of frame it would otherwise be "measured" with full
/// confidence (bench, 2026-09-29: 2.1 x 1.8 mm reported for a square-only
/// frame). Scaled through the reference so it holds at any camera height.
constexpr double kMinSubjectAreaMm2 = 10.0;
/// Smallest enclosed region kept as a bore in the exported outline. An M3
/// nut's bore is ~7 mm^2; glints on bright steel read as holes of 1-2 mm^2
/// (bench, 2026-09-29) and must not reach a CAD sketch as features.
constexpr double kMinBoreAreaMm2 = 4.0;
/// Enclosed background regions smaller than this are glints, not holes. It is
/// in pixels because holes are counted before the scale is known: ~1 mm² at
/// the bench's 0.13 mm/px, far below the smallest in-scope bore (M3, ~7 mm²).
constexpr std::size_t kMinHoleCountPx = 40;

/// Luma extraction. Gray8 passes through; YUYV keeps the luma byte that leads
/// every pixel; RGB888 uses integer Rec.601-style weights.
std::vector<std::uint8_t> toGray(const hal::Frame& frame) {
    const auto& mode = frame.mode();
    const auto pixels = frame.pixels();
    const std::size_t count = static_cast<std::size_t>(mode.width) * mode.height;

    std::vector<std::uint8_t> gray(count);
    if (mode.format == PixelFormat::Gray8) {
        for (std::size_t i = 0; i < count; ++i)
            gray[i] = std::to_integer<std::uint8_t>(pixels[i]);
        return gray;
    }
    if (mode.format == PixelFormat::YUYV) {
        // Packed Y0 U Y1 V: luma leads each pixel, chroma is discarded. This is
        // the UVC webcam path, and binarization wants luma regardless.
        for (std::size_t i = 0; i < count; ++i)
            gray[i] = std::to_integer<std::uint8_t>(pixels[i * 2]);
        return gray;
    }
    for (std::size_t i = 0; i < count; ++i) {
        const auto r = std::to_integer<std::uint32_t>(pixels[i * 3]);
        const auto g = std::to_integer<std::uint32_t>(pixels[i * 3 + 1]);
        const auto b = std::to_integer<std::uint32_t>(pixels[i * 3 + 2]);
        gray[i] = static_cast<std::uint8_t>((77 * r + 150 * g + 29 * b) >> 8);
    }
    return gray;
}

/// Flattens uneven lighting before thresholding.
///
/// One global Otsu threshold separates dark parts from white paper only if the
/// paper is equally bright everywhere. On the bench it is not: a desk lamp's
/// falloff plus the webcam's vignetting left one corner of the 2026-09-29
/// frame about 7x darker than the centre, the dim paper fell below the
/// threshold, and it merged with the square and the screw into one blob
/// touching the frame edge — so no square was found.
///
/// The paper is modelled as a smooth cubic surface in (x, y), fitted by least
/// squares to a grid of samples, iteratively rejecting samples far below the
/// fit (the parts are dark outliers). Dividing it out leaves the paper evenly
/// bright and the parts dark. Vignetting and lamp falloff are smooth, so a
/// cubic follows them; unlike a max-filter background, the fit is not fooled
/// by parts larger than a filter window. On a uniformly lit frame the fitted
/// surface is flat and this is the identity, which is why the synthetic
/// validation scenes measure exactly as before.
void flattenIllumination(std::vector<std::uint8_t>& gray, std::int32_t width, std::int32_t height) {
    constexpr std::size_t kTerms = 10;
    constexpr double kPaperLevel = 235.0;  // what the paper normalizes to
    if (width < 16 || height < 16) return;

    const auto terms = [](double x, double y, std::array<double, kTerms>& t) {
        t = {1.0, x, y, x * x, x * y, y * y, x * x * x, x * x * y, x * y * y, y * y * y};
    };
    const auto nx = [&](double x) { return 2.0 * x / (width - 1) - 1.0; };
    const auto ny = [&](double y) { return 2.0 * y / (height - 1) - 1.0; };

    // Sample grid: ~60 samples across the short side.
    const std::int32_t step = std::max(4, std::min(width, height) / 60);
    struct Sample {
        std::array<double, kTerms> t;
        double value;
    };
    std::vector<Sample> samples;
    for (std::int32_t y = 0; y < height; y += step)
        for (std::int32_t x = 0; x < width; x += step) {
            Sample s{};
            terms(nx(x), ny(y), s.t);
            s.value = gray[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                           static_cast<std::size_t>(x)];
            samples.push_back(s);
        }
    if (samples.size() < 4 * kTerms) return;

    // Start from the brighter 80%: the parts are the darkest pixels.
    std::vector<double> values;
    values.reserve(samples.size());
    for (const auto& s : samples)
        values.push_back(s.value);
    auto cut = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 5);
    std::nth_element(values.begin(), cut, values.end());
    const double floor = *cut;
    std::vector<bool> keep(samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i)
        keep[i] = samples[i].value >= floor;

    std::array<double, kTerms> coeff{};
    for (int iteration = 0; iteration < 5; ++iteration) {
        // Normal equations, solved by Gaussian elimination with partial pivoting.
        std::array<std::array<double, kTerms + 1>, kTerms> m{};
        std::size_t kept = 0;
        for (std::size_t i = 0; i < samples.size(); ++i) {
            if (!keep[i]) continue;
            ++kept;
            const auto& t = samples[i].t;
            for (std::size_t r = 0; r < kTerms; ++r) {
                for (std::size_t c = 0; c < kTerms; ++c)
                    m[r][c] += t[r] * t[c];
                m[r][kTerms] += t[r] * samples[i].value;
            }
        }
        if (kept < 4 * kTerms) return;
        for (std::size_t col = 0; col < kTerms; ++col) {
            std::size_t pivot = col;
            for (std::size_t r = col + 1; r < kTerms; ++r)
                if (std::abs(m[r][col]) > std::abs(m[pivot][col])) pivot = r;
            if (std::abs(m[pivot][col]) < 1e-12) return;  // degenerate: leave the frame alone
            std::swap(m[col], m[pivot]);
            for (std::size_t r = 0; r < kTerms; ++r) {
                if (r == col) continue;
                const double f = m[r][col] / m[col][col];
                for (std::size_t c = col; c <= kTerms; ++c)
                    m[r][c] -= f * m[col][c];
            }
        }
        for (std::size_t r = 0; r < kTerms; ++r)
            coeff[r] = m[r][kTerms] / m[r][r];

        // Reject what sits well below the surface: that is the parts.
        double sumSq = 0.0;
        std::vector<double> residual(samples.size());
        for (std::size_t i = 0; i < samples.size(); ++i) {
            double fit = 0.0;
            for (std::size_t k = 0; k < kTerms; ++k)
                fit += coeff[k] * samples[i].t[k];
            residual[i] = samples[i].value - fit;
            if (keep[i]) sumSq += residual[i] * residual[i];
        }
        const double sigma = std::sqrt(sumSq / static_cast<double>(kept));
        for (std::size_t i = 0; i < samples.size(); ++i)
            keep[i] = residual[i] > -2.5 * sigma - 1.0;  // -1: exact-flat frames keep all paper
    }

    std::array<double, kTerms> t{};
    for (std::int32_t y = 0; y < height; ++y)
        for (std::int32_t x = 0; x < width; ++x) {
            terms(nx(x), ny(y), t);
            double paper = 0.0;
            for (std::size_t k = 0; k < kTerms; ++k)
                paper += coeff[k] * t[k];
            auto& px = gray[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                            static_cast<std::size_t>(x)];
            const double flattened = px * kPaperLevel / std::max(paper, 1.0);
            px = static_cast<std::uint8_t>(std::clamp(std::lround(flattened), 0L, 255L));
        }
}

/// Otsu's method over a 256-bin histogram. Returns the threshold that
/// maximizes between-class variance; dark pixels (< threshold) are foreground.
std::uint8_t otsuThreshold(const std::vector<std::uint8_t>& gray) {
    std::array<std::size_t, 256> histogram{};
    for (const auto value : gray)
        ++histogram[value];

    const double total = static_cast<double>(gray.size());
    double sumAll = 0.0;
    for (std::size_t v = 0; v < 256; ++v)
        sumAll += static_cast<double>(v) * static_cast<double>(histogram[v]);

    double sumBelow = 0.0;
    double weightBelow = 0.0;
    double bestVariance = -1.0;
    std::uint8_t best = 0;
    for (std::size_t t = 0; t < 256; ++t) {
        weightBelow += static_cast<double>(histogram[t]);
        if (weightBelow == 0.0) continue;
        const double weightAbove = total - weightBelow;
        if (weightAbove == 0.0) break;
        sumBelow += static_cast<double>(t) * static_cast<double>(histogram[t]);
        const double meanBelow = sumBelow / weightBelow;
        const double meanAbove = (sumAll - sumBelow) / weightAbove;
        const double variance =
            weightBelow * weightAbove * (meanBelow - meanAbove) * (meanBelow - meanAbove);
        if (variance > bestVariance) {
            bestVariance = variance;
            best = static_cast<std::uint8_t>(t + 1);  // foreground is strictly below
        }
    }
    return best;
}

struct LabeledBlob {
    std::int32_t label = 0;
    BlobStats stats;
};

/// 4-connected components over the dark-foreground mask, flood-filled with an
/// explicit stack (no recursion — frames are large). Labels start at 0 in the
/// labels image; -1 is background.
std::vector<LabeledBlob> labelComponents(const std::vector<std::uint8_t>& gray,
                                         std::uint8_t threshold, std::int32_t width,
                                         std::int32_t height, std::vector<std::int32_t>& labels) {
    labels.assign(gray.size(), -1);
    std::vector<LabeledBlob> blobs;
    std::vector<std::size_t> stack;

    const auto isForeground = [&](std::size_t index) { return gray[index] < threshold; };

    for (std::size_t seed = 0; seed < gray.size(); ++seed) {
        if (!isForeground(seed) || labels[seed] != -1) continue;

        const auto label = static_cast<std::int32_t>(blobs.size());
        LabeledBlob blob;
        blob.label = label;
        auto& s = blob.stats;
        s.minX = s.minY = std::numeric_limits<std::int32_t>::max();
        s.maxX = s.maxY = std::numeric_limits<std::int32_t>::min();
        double sumX = 0.0;
        double sumY = 0.0;

        stack.clear();
        stack.push_back(seed);
        labels[seed] = label;
        while (!stack.empty()) {
            const std::size_t index = stack.back();
            stack.pop_back();
            const auto x = static_cast<std::int32_t>(index % static_cast<std::size_t>(width));
            const auto y = static_cast<std::int32_t>(index / static_cast<std::size_t>(width));

            ++s.areaPx;
            sumX += x;
            sumY += y;
            s.minX = std::min(s.minX, x);
            s.maxX = std::max(s.maxX, x);
            s.minY = std::min(s.minY, y);
            s.maxY = std::max(s.maxY, y);

            const std::array<std::pair<std::int32_t, std::int32_t>, 4> neighbours{
                {{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}}};
            for (const auto& [nx, ny] : neighbours) {
                if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
                const std::size_t neighbour =
                    static_cast<std::size_t>(ny) * static_cast<std::size_t>(width) +
                    static_cast<std::size_t>(nx);
                if (!isForeground(neighbour) || labels[neighbour] != -1) continue;
                labels[neighbour] = label;
                stack.push_back(neighbour);
            }
        }

        const auto area = static_cast<double>(s.areaPx);
        s.centroidX = sumX / area;
        s.centroidY = sumY / area;
        const auto boxArea =
            static_cast<double>(s.maxX - s.minX + 1) * static_cast<double>(s.maxY - s.minY + 1);
        s.fillRatio = area / boxArea;
        blobs.push_back(std::move(blob));
    }
    return blobs;
}

/// Counts enclosed background regions ("holes") per blob label. Background
/// 4-connected to the frame border is outside; any other background region is
/// a hole in the blob that surrounds it (attributed to the first adjacent
/// foreground label in scan order — deterministic).
std::vector<std::size_t> countHoles(const std::vector<std::int32_t>& labels, std::int32_t width,
                                    std::int32_t height, std::size_t blobCount) {
    std::vector<std::uint8_t> visited(labels.size(), 0);
    std::vector<std::size_t> stack;

    const auto tryPush = [&](std::int32_t x, std::int32_t y) {
        if (x < 0 || y < 0 || x >= width || y >= height) return;
        const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                  static_cast<std::size_t>(x);
        if (labels[index] != -1 || visited[index]) return;
        visited[index] = 1;
        stack.push_back(index);
    };
    const auto drain = [&](std::int32_t* owner) {
        std::size_t drained = 0;
        while (!stack.empty()) {
            const std::size_t index = stack.back();
            stack.pop_back();
            ++drained;
            const auto x = static_cast<std::int32_t>(index % static_cast<std::size_t>(width));
            const auto y = static_cast<std::int32_t>(index / static_cast<std::size_t>(width));
            const std::array<std::pair<std::int32_t, std::int32_t>, 4> neighbours{
                {{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}}};
            for (const auto& [nx, ny] : neighbours) {
                if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
                const std::size_t neighbour =
                    static_cast<std::size_t>(ny) * static_cast<std::size_t>(width) +
                    static_cast<std::size_t>(nx);
                if (labels[neighbour] == -1) {
                    if (!visited[neighbour]) {
                        visited[neighbour] = 1;
                        stack.push_back(neighbour);
                    }
                } else if (owner && *owner == -1) {
                    *owner = labels[neighbour];
                }
            }
        }
        return drained;
    };

    for (std::int32_t x = 0; x < width; ++x) {
        tryPush(x, 0);
        tryPush(x, height - 1);
    }
    for (std::int32_t y = 0; y < height; ++y) {
        tryPush(0, y);
        tryPush(width - 1, y);
    }
    drain(nullptr);

    std::vector<std::size_t> holes(blobCount, 0);
    for (std::size_t seed = 0; seed < labels.size(); ++seed) {
        if (labels[seed] != -1 || visited[seed]) continue;
        std::int32_t owner = -1;
        visited[seed] = 1;
        stack.push_back(seed);
        // Specular glints on polished metal punch tiny background-coloured
        // pockets into a part (a shiny screw showed 42); they are not bores.
        const auto area = drain(&owner);
        if (area >= kMinHoleCountPx && owner >= 0 && static_cast<std::size_t>(owner) < blobCount)
            ++holes[static_cast<std::size_t>(owner)];
    }
    return holes;
}

/// Width and length by minimum support width: sweep directions in 1° steps,
/// take the direction with the smallest projected extent as the width axis
/// and the orthogonal extent as the length. Rotation-invariant — a rotated
/// hexagon measures its across-flats, a rotated rod its true diameter —
/// unlike principal-axis extents, which overestimate as shapes rotate.
void measureSupportExtents(const std::vector<std::int32_t>& labels, std::int32_t width,
                           std::int32_t label, BlobStats& stats) {
    std::vector<std::pair<double, double>> points;
    for (std::size_t index = 0; index < labels.size(); ++index) {
        if (labels[index] != label) continue;
        points.emplace_back(
            static_cast<double>(index % static_cast<std::size_t>(width)) - stats.centroidX,
            static_cast<double>(index / static_cast<std::size_t>(width)) - stats.centroidY);
    }
    if (points.empty()) return;

    constexpr double kPi = 3.14159265358979;
    double bestWidth = std::numeric_limits<double>::max();
    double bestAngle = 0.0;
    for (std::int32_t step = 0; step < 180; ++step) {
        const double angle = static_cast<double>(step) * kPi / 180.0;
        const double nx = -std::sin(angle);
        const double ny = std::cos(angle);
        double lo = std::numeric_limits<double>::max();
        double hi = std::numeric_limits<double>::lowest();
        for (const auto& [x, y] : points) {
            const double projection = x * nx + y * ny;
            lo = std::min(lo, projection);
            hi = std::max(hi, projection);
        }
        if (hi - lo < bestWidth) {
            bestWidth = hi - lo;
            bestAngle = angle;
        }
    }

    const double ax = std::cos(bestAngle);
    const double ay = std::sin(bestAngle);
    double lo = std::numeric_limits<double>::max();
    double hi = std::numeric_limits<double>::lowest();
    for (const auto& [x, y] : points) {
        const double projection = x * ax + y * ay;
        lo = std::min(lo, projection);
        hi = std::max(hi, projection);
    }

    // +1: extents span pixel centres; each end pixel contributes half a pixel.
    stats.widthPx = bestWidth + 1.0;
    stats.lengthPx = (hi - lo) + 1.0;
    // The sweep angle at minimum width IS the length direction; normalize to
    // [-pi/2, pi/2).
    stats.majorAxisAngleRad = bestAngle >= kPi / 2.0 ? bestAngle - kPi : bestAngle;
}

bool isSquareCandidate(const BlobStats& stats) {
    const auto boxW = static_cast<double>(stats.maxX - stats.minX + 1);
    const auto boxH = static_cast<double>(stats.maxY - stats.minY + 1);
    const double aspect = boxW < boxH ? boxW / boxH : boxH / boxW;
    // The printed reference is solid: a square-ish part with a hole (a plate,
    // a PCB) must never be mistaken for it and silently rescale the scene.
    return aspect >= kMinReferenceAspect && stats.fillRatio >= kMinReferenceFill &&
           stats.holeCount == 0;
}

std::string decimalClaim(double value) {
    // toJson owns numeric formatting; claims carry doubles directly. This
    // helper only builds method strings.
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%g", value);
    return buffer;
}

/// The reference square's four corners (extremes along its own diagonals)
/// and from them the camera tilt and keystone. A tilted view squashes the
/// square into a parallelogram/trapezoid; the singular values of its mean
/// edge vectors give the foreshortening whatever the card's rotation.
void measureTilt(const std::vector<std::int32_t>& labels, std::int32_t width, const BlobStats& ref,
                 std::int32_t label, ScoutAnalysis& out) {
    constexpr double kPi = 3.14159265358979;
    const double theta = ref.majorAxisAngleRad;
    std::array<double, 4> best{};
    std::array<std::array<double, 2>, 4> corner{};
    std::array<bool, 4> seen{};
    for (std::int32_t y = ref.minY; y <= ref.maxY; ++y)
        for (std::int32_t x = ref.minX; x <= ref.maxX; ++x) {
            if (labels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                       static_cast<std::size_t>(x)] != label)
                continue;
            for (int k = 0; k < 4; ++k) {
                const double a = theta + kPi / 4.0 + k * kPi / 2.0;
                const double d = x * std::cos(a) + y * std::sin(a);
                if (!seen[k] || d > best[k]) {
                    best[k] = d;
                    corner[k] = {static_cast<double>(x), static_cast<double>(y)};
                    seen[k] = true;
                }
            }
        }
    const auto sub = [](const std::array<double, 2>& p, const std::array<double, 2>& q) {
        return std::array<double, 2>{p[0] - q[0], p[1] - q[1]};
    };
    const auto len = [](const std::array<double, 2>& v) { return std::hypot(v[0], v[1]); };
    // c0..c3 go round the square; c0->c1 and c3->c2 are one pair of opposite
    // sides, c0->c3 and c1->c2 the other.
    const auto s01 = sub(corner[1], corner[0]), s32 = sub(corner[2], corner[3]);
    const auto s03 = sub(corner[3], corner[0]), s12 = sub(corner[2], corner[1]);
    const std::array<double, 2> e1{(s01[0] + s32[0]) / 2.0, (s01[1] + s32[1]) / 2.0};
    const std::array<double, 2> e2{(s03[0] + s12[0]) / 2.0, (s03[1] + s12[1]) / 2.0};
    const double sum = e1[0] * e1[0] + e1[1] * e1[1] + e2[0] * e2[0] + e2[1] * e2[1];
    const double det = std::abs(e1[0] * e2[1] - e1[1] * e2[0]);
    const double disc = std::sqrt(std::max(0.0, sum * sum - 4.0 * det * det));
    const double s1 = std::sqrt((sum + disc) / 2.0),
                 s2 = std::sqrt(std::max(0.0, (sum - disc) / 2.0));
    if (s1 <= 0.0) return;
    out.cameraTiltDeg = std::acos(std::clamp(s2 / s1, 0.0, 1.0)) * 180.0 / kPi;
    const auto ratio = [](double a, double b) { return a > b ? a / b : b / a; };
    const double k1 = len(s32) > 0.0 ? ratio(len(s01), len(s32)) : 1.0;
    const double k2 = len(s12) > 0.0 ? ratio(len(s03), len(s12)) : 1.0;
    out.referenceKeystone = std::max(k1, k2) - 1.0;
}

/// Six-fold harmonic of a closed loop's radius about its area centroid,
/// sampled every half pixel along the perimeter (simplified loops have long
/// vertex-free edges), as a fraction of the mean radius.
double sixFoldHarmonic(const std::vector<geometry::Vec2>& loop) {
    if (loop.size() < 3) return 0.0;
    std::vector<std::array<double, 2>> pts;
    for (std::size_t i = 0; i < loop.size(); ++i) {
        const auto& p = loop[i];
        const auto& q = loop[(i + 1) % loop.size()];
        const double len = std::hypot(q.x - p.x, q.y - p.y);
        const int steps = std::max(1, static_cast<int>(std::ceil(len / 0.5)));
        for (int k = 0; k < steps; ++k) {
            const double f = static_cast<double>(k) / steps;
            pts.push_back({p.x + (q.x - p.x) * f, p.y + (q.y - p.y) * f});
        }
    }
    double cx = 0.0, cy = 0.0;
    for (const auto& p : pts) {
        cx += p[0];
        cy += p[1];
    }
    cx /= static_cast<double>(pts.size());
    cy /= static_cast<double>(pts.size());
    double meanR = 0.0;
    for (const auto& p : pts)
        meanR += std::hypot(p[0] - cx, p[1] - cy);
    meanR /= static_cast<double>(pts.size());
    if (meanR <= 0.0) return 0.0;
    double re = 0.0, im = 0.0;
    for (const auto& p : pts) {
        const double dev = std::hypot(p[0] - cx, p[1] - cy) / meanR - 1.0;
        const double th = 6.0 * std::atan2(p[1] - cy, p[0] - cx);
        re += dev * std::cos(th);
        im += dev * std::sin(th);
    }
    return 2.0 * std::hypot(re, im) / static_cast<double>(pts.size());
}

}  // namespace

AnalyzeOutcome analyzeFrame(const hal::Frame& frame, const CalibrationSpec& spec) {
    const auto& mode = frame.mode();
    if (frame.empty() || mode.width == 0 || mode.height == 0 || spec.referenceSideMm <= 0.0)
        return {std::nullopt, AnalyzeError::InvalidFrame};
    if (mode.format != PixelFormat::RGB888 && mode.format != PixelFormat::Gray8 &&
        mode.format != PixelFormat::YUYV)
        return {std::nullopt, AnalyzeError::UnsupportedFormat};

    const std::size_t bytesPerPixel = mode.format == PixelFormat::RGB888 ? 3u
                                      : mode.format == PixelFormat::YUYV ? 2u
                                                                         : 1u;
    const std::size_t expected = static_cast<std::size_t>(mode.width) * mode.height * bytesPerPixel;
    if (frame.pixels().size() != expected) return {std::nullopt, AnalyzeError::InvalidFrame};

    auto gray = toGray(frame);
    flattenIllumination(gray, mode.width, mode.height);
    const auto threshold = otsuThreshold(gray);

    std::vector<std::int32_t> labels;
    auto blobs = labelComponents(gray, threshold, mode.width, mode.height, labels);
    const auto holes = countHoles(labels, mode.width, mode.height, blobs.size());
    for (auto& blob : blobs)
        blob.stats.holeCount = holes[static_cast<std::size_t>(blob.label)];
    std::erase_if(blobs,
                  [](const LabeledBlob& blob) { return blob.stats.areaPx < kMinBlobAreaPx; });
    if (blobs.empty()) return {std::nullopt, AnalyzeError::NoReferenceTarget};

    // Reference: the largest square candidate; a comparable runner-up means
    // the scene is ambiguous and the operator must fix it, not the code.
    std::vector<const LabeledBlob*> squares;
    for (const auto& blob : blobs)
        if (isSquareCandidate(blob.stats)) squares.push_back(&blob);
    if (squares.empty()) return {std::nullopt, AnalyzeError::NoReferenceTarget};
    std::sort(squares.begin(), squares.end(), [](const LabeledBlob* a, const LabeledBlob* b) {
        return a->stats.areaPx > b->stats.areaPx;
    });
    if (squares.size() > 1 &&
        static_cast<double>(squares[1]->stats.areaPx) >=
            kAmbiguityAreaRatio * static_cast<double>(squares[0]->stats.areaPx))
        return {std::nullopt, AnalyzeError::ReferenceAmbiguous};
    const LabeledBlob* reference = squares.front();

    // Subject: largest remaining blob, if it is big enough to be a part.
    const LabeledBlob* subject = nullptr;
    for (const auto& blob : blobs) {
        if (blob.label == reference->label) continue;
        if (!subject || blob.stats.areaPx > subject->stats.areaPx) subject = &blob;
    }
    const double mm2PerPx = (spec.referenceSideMm * spec.referenceSideMm) /
                            static_cast<double>(reference->stats.areaPx);
    if (!subject || static_cast<double>(subject->stats.areaPx) * mm2PerPx < kMinSubjectAreaMm2)
        return {std::nullopt, AnalyzeError::NoSubject};

    ScoutAnalysis analysis;
    analysis.binarizationThreshold = threshold;
    analysis.reference = reference->stats;
    analysis.subject = subject->stats;
    measureSupportExtents(labels, mode.width, reference->label, analysis.reference);
    measureSupportExtents(labels, mode.width, subject->label, analysis.subject);

    // sqrt(area) is the square's side regardless of small rotations, unlike
    // the axis-aligned bounding box.
    analysis.mmPerPixel =
        spec.referenceSideMm / std::sqrt(static_cast<double>(reference->stats.areaPx));
    analysis.subjectLengthMm = analysis.subject.lengthPx * analysis.mmPerPixel;
    analysis.subjectWidthMm = analysis.subject.widthPx * analysis.mmPerPixel;
    measureTilt(labels, mode.width, analysis.reference, reference->label, analysis);
    analysis.subjectOutlinePx = outlineOf(
        labels, mode.width, mode.height, subject->label, subject->stats.minX, subject->stats.minY,
        subject->stats.maxX, subject->stats.maxY, kMinBoreAreaMm2 / mm2PerPx);
    // Outline loops are in pixels, so hole areas scale by mm^2 per px^2.
    for (const auto& hole : analysis.subjectOutlinePx.holes)
        analysis.holeDiametersMm.push_back(
            2.0 * std::sqrt(std::abs(signedArea(hole)) * mm2PerPx / 3.14159265358979));
    analysis.outlineSixFold = sixFoldHarmonic(analysis.subjectOutlinePx.outer);
    return {analysis, AnalyzeError::None};
}

void appendEvidence(observation::EngineeringObservation& record, const ScoutAnalysis& analysis,
                    const CalibrationSpec& spec, std::string_view sourceArtifactId) {
    const std::string source(sourceArtifactId);
    const std::string method = "vision.scout_analyzer.v3";  // v3: illumination flattening

    const auto observed = [&](std::string id, std::string name, double value,
                              std::optional<std::string> unit) {
        record.observed.push_back(observation::Claim{std::move(id),
                                                     std::move(name),
                                                     value,
                                                     std::move(unit),
                                                     std::nullopt,
                                                     {source},
                                                     method});
    };
    observed("sa-threshold", "binarization_threshold",
             static_cast<double>(analysis.binarizationThreshold), std::nullopt);
    observed("sa-ref-area-px", "reference_area", static_cast<double>(analysis.reference.areaPx),
             "px^2");
    observed("sa-ref-keystone", "reference_keystone", analysis.referenceKeystone, std::nullopt);
    observed("sa-subj-area-px", "subject_area", static_cast<double>(analysis.subject.areaPx),
             "px^2");
    observed("sa-subj-length-px", "subject_length", analysis.subject.lengthPx, "px");
    observed("sa-subj-width-px", "subject_width", analysis.subject.widthPx, "px");
    observed("sa-subj-axis-rad", "subject_major_axis_angle", analysis.subject.majorAxisAngleRad,
             "rad");
    observed("sa-subj-holes", "subject_hole_count", static_cast<double>(analysis.subject.holeCount),
             std::nullopt);

    record.derived.push_back(observation::Claim{
        "sa-mm-per-px",
        "mm_per_pixel",
        analysis.mmPerPixel,
        "mm/px",
        std::nullopt,
        {"sa-ref-area-px"},
        method + "; scale = reference_side_mm / sqrt(reference_area); reference_side_mm = " +
            decimalClaim(spec.referenceSideMm)});
    record.derived.push_back(observation::Claim{"sa-subj-length-mm",
                                                "subject_length",
                                                analysis.subjectLengthMm,
                                                "mm",
                                                std::nullopt,
                                                {"sa-mm-per-px", "sa-subj-length-px"},
                                                method + "; length_px * mm_per_pixel"});
    record.derived.push_back(observation::Claim{"sa-subj-width-mm",
                                                "subject_width",
                                                analysis.subjectWidthMm,
                                                "mm",
                                                std::nullopt,
                                                {"sa-mm-per-px", "sa-subj-width-px"},
                                                method + "; width_px * mm_per_pixel"});

    record.derived.push_back(observation::Claim{
        "sa-camera-tilt-deg",
        "camera_tilt",
        analysis.cameraTiltDeg,
        "deg",
        std::nullopt,
        {"sa-ref-area-px", "sa-ref-keystone"},
        method + "; acos of the singular-value ratio of the reference square's mean edge "
                 "vectors; a lower bound, ~10 deg resolution at bench scale"});
    if (analysis.cameraTiltDeg >= kTiltWarningDeg) {
        char tilt[160];
        std::snprintf(tilt, sizeof(tilt),
                      "camera tilted ~%.0f deg: sizes read high; aim straight down at the card",
                      analysis.cameraTiltDeg);
        record.recommendedNextObservations.insert(record.recommendedNextObservations.begin(),
                                                  {tilt, {"camera_tilt"}});
    }

    // Geometry the reasoner needs to tell part families apart. Holes are
    // reported as holes; calling one a bore is an inference.
    observed("sa-six-fold", "outline_six_fold", analysis.outlineSixFold, std::nullopt);
    for (std::size_t i = 0; i < analysis.holeDiametersMm.size() && i < 4; ++i) {
        const auto n = std::to_string(i + 1);
        record.derived.push_back(observation::Claim{
            "sa-hole-" + n + "-dia-mm",
            "hole_" + n + "_diameter",
            analysis.holeDiametersMm[i],
            "mm",
            std::nullopt,
            {"sa-mm-per-px"},
            method + "; equal-area diameter of outline hole " + n + " (holes >= 4 mm^2)"});
    }
}

}  // namespace platypus::vision
