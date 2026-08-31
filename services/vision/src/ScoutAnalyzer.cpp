#include "platypus/vision/ScoutAnalyzer.hpp"

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

/// Luma conversion for RGB888, integer Rec.601-style weights.
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
    for (std::size_t i = 0; i < count; ++i) {
        const auto r = std::to_integer<std::uint32_t>(pixels[i * 3]);
        const auto g = std::to_integer<std::uint32_t>(pixels[i * 3 + 1]);
        const auto b = std::to_integer<std::uint32_t>(pixels[i * 3 + 2]);
        gray[i] = static_cast<std::uint8_t>((77 * r + 150 * g + 29 * b) >> 8);
    }
    return gray;
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
        while (!stack.empty()) {
            const std::size_t index = stack.back();
            stack.pop_back();
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
        drain(&owner);
        if (owner >= 0 && static_cast<std::size_t>(owner) < blobCount)
            ++holes[static_cast<std::size_t>(owner)];
    }
    return holes;
}

/// Principal-axis extents from the labeled pixels: second central moments give
/// the axis angle; projecting every pixel onto the axes gives exact extents.
void measurePrincipalExtents(const std::vector<std::int32_t>& labels, std::int32_t width,
                             std::int32_t label, BlobStats& stats) {
    double sumXX = 0.0;
    double sumYY = 0.0;
    double sumXY = 0.0;
    for (std::size_t index = 0; index < labels.size(); ++index) {
        if (labels[index] != label) continue;
        const auto x =
            static_cast<double>(index % static_cast<std::size_t>(width)) - stats.centroidX;
        const auto y =
            static_cast<double>(index / static_cast<std::size_t>(width)) - stats.centroidY;
        sumXX += x * x;
        sumYY += y * y;
        sumXY += x * y;
    }

    const double angle = 0.5 * std::atan2(2.0 * sumXY, sumXX - sumYY);
    const double axisX = std::cos(angle);
    const double axisY = std::sin(angle);

    double minMajor = std::numeric_limits<double>::max();
    double maxMajor = std::numeric_limits<double>::lowest();
    double minMinor = std::numeric_limits<double>::max();
    double maxMinor = std::numeric_limits<double>::lowest();
    for (std::size_t index = 0; index < labels.size(); ++index) {
        if (labels[index] != label) continue;
        const auto x =
            static_cast<double>(index % static_cast<std::size_t>(width)) - stats.centroidX;
        const auto y =
            static_cast<double>(index / static_cast<std::size_t>(width)) - stats.centroidY;
        const double major = x * axisX + y * axisY;
        const double minor = -x * axisY + y * axisX;
        minMajor = std::min(minMajor, major);
        maxMajor = std::max(maxMajor, major);
        minMinor = std::min(minMinor, minor);
        maxMinor = std::max(maxMinor, minor);
    }

    // +1: extents span pixel centres; each end pixel contributes half a pixel.
    const double along = maxMajor - minMajor + 1.0;
    const double across = maxMinor - minMinor + 1.0;
    if (along >= across) {
        stats.majorAxisAngleRad = angle;
        stats.lengthPx = along;
        stats.widthPx = across;
    } else {
        stats.majorAxisAngleRad =
            angle > 0.0 ? angle - 1.5707963267948966 : angle + 1.5707963267948966;
        stats.lengthPx = across;
        stats.widthPx = along;
    }
}

bool isSquareCandidate(const BlobStats& stats) {
    const auto boxW = static_cast<double>(stats.maxX - stats.minX + 1);
    const auto boxH = static_cast<double>(stats.maxY - stats.minY + 1);
    const double aspect = boxW < boxH ? boxW / boxH : boxH / boxW;
    return aspect >= kMinReferenceAspect && stats.fillRatio >= kMinReferenceFill;
}

std::string decimalClaim(double value) {
    // toJson owns numeric formatting; claims carry doubles directly. This
    // helper only builds method strings.
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%g", value);
    return buffer;
}

}  // namespace

AnalyzeOutcome analyzeFrame(const hal::Frame& frame, const CalibrationSpec& spec) {
    const auto& mode = frame.mode();
    if (frame.empty() || mode.width == 0 || mode.height == 0 || spec.referenceSideMm <= 0.0)
        return {std::nullopt, AnalyzeError::InvalidFrame};
    if (mode.format != PixelFormat::RGB888 && mode.format != PixelFormat::Gray8)
        return {std::nullopt, AnalyzeError::UnsupportedFormat};

    const std::size_t expected = static_cast<std::size_t>(mode.width) * mode.height *
                                 (mode.format == PixelFormat::RGB888 ? 3u : 1u);
    if (frame.pixels().size() != expected) return {std::nullopt, AnalyzeError::InvalidFrame};

    const auto gray = toGray(frame);
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

    // Subject: largest remaining blob.
    const LabeledBlob* subject = nullptr;
    for (const auto& blob : blobs) {
        if (blob.label == reference->label) continue;
        if (!subject || blob.stats.areaPx > subject->stats.areaPx) subject = &blob;
    }
    if (!subject) return {std::nullopt, AnalyzeError::NoSubject};

    ScoutAnalysis analysis;
    analysis.binarizationThreshold = threshold;
    analysis.reference = reference->stats;
    analysis.subject = subject->stats;
    measurePrincipalExtents(labels, mode.width, reference->label, analysis.reference);
    measurePrincipalExtents(labels, mode.width, subject->label, analysis.subject);

    // sqrt(area) is the square's side regardless of small rotations, unlike
    // the axis-aligned bounding box.
    analysis.mmPerPixel =
        spec.referenceSideMm / std::sqrt(static_cast<double>(reference->stats.areaPx));
    analysis.subjectLengthMm = analysis.subject.lengthPx * analysis.mmPerPixel;
    analysis.subjectWidthMm = analysis.subject.widthPx * analysis.mmPerPixel;
    return {analysis, AnalyzeError::None};
}

void appendEvidence(observation::EngineeringObservation& record, const ScoutAnalysis& analysis,
                    const CalibrationSpec& spec, std::string_view sourceArtifactId) {
    const std::string source(sourceArtifactId);
    const std::string method = "vision.scout_analyzer.v1";

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

    record.unresolved.push_back(
        {"fastener_class", "classification is a later Scout chunk; not attempted"});
    record.unresolved.push_back(
        {"nominal_size", "requires fastener_class and thread evidence; not attempted"});
    record.unresolved.push_back(
        {"thread_pitch", "a single top-down silhouette cannot resolve the thread profile"});
    record.recommendedNextObservations.push_back(
        {"capture a side-on view so the thread profile is visible against the background",
         {"thread_pitch"}});
}

}  // namespace platypus::vision
