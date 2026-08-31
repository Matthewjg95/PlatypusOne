#include "platypus/apps/EngineeringScoutApp.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace platypus::apps {

namespace {

using observation::Claim;
using renderer::Color;
using renderer::Renderer;

// Card palette. One color per evidence class, used ONLY for that class.
constexpr Color kBackground{14, 19, 26};
constexpr Color kCardEdge{44, 54, 66};
constexpr Color kInk{230, 235, 240};
constexpr Color kDim{150, 160, 172};
constexpr Color kObserved{123, 216, 143};    // green — measured pixel facts
constexpr Color kDerived{127, 180, 255};     // blue — computed through calibration
constexpr Color kInferred{255, 198, 109};    // amber — model/heuristic, has confidence
constexpr Color kUnresolved{255, 123, 114};  // red — honestly unknown
constexpr Color kAccent{120, 200, 255};

std::string formatValue(const Claim& claim) {
    std::string text;
    if (const auto* number = std::get_if<double>(&claim.value)) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.2f", *number);
        text = buffer;
    } else if (const auto* flag = std::get_if<bool>(&claim.value)) {
        text = *flag ? "true" : "false";
    } else {
        text = std::get<std::string>(claim.value);
    }
    if (claim.unit) text += " " + *claim.unit;
    return text;
}

/// Greedy word wrap into at most maxLines lines of maxChars monospace cells;
/// the last line is ellipsized when the text does not fit.
std::vector<std::string> wrap(std::string_view text, std::size_t maxChars, std::size_t maxLines) {
    std::vector<std::string> lines;
    std::istringstream words{std::string(text)};
    std::string word;
    std::string line;
    bool truncated = false;
    while (words >> word) {
        if (line.empty()) {
            line = word;
        } else if (line.size() + 1 + word.size() <= maxChars) {
            line += " " + word;
        } else {
            lines.push_back(line);
            line = word;
            if (lines.size() == maxLines) {
                truncated = true;  // this word and anything after it is dropped
                break;
            }
        }
    }
    if (!truncated && !line.empty()) lines.push_back(line);
    if (truncated && !lines.empty()) {
        auto& last = lines.back();
        if (last.size() > 3) last.replace(last.size() - 3, 3, "...");
    }
    return lines;
}

const Claim* findClaim(const std::vector<Claim>& claims, std::string_view name) {
    for (const auto& claim : claims)
        if (claim.name == name) return &claim;
    return nullptr;
}

/// Section header: colored tag + rule line across the card.
std::int32_t sectionHeader(Renderer& r, std::int32_t x, std::int32_t y, std::int32_t width,
                           std::string_view tag, Color color) {
    r.drawText(x, y, tag, color);
    const auto tagWidth = Renderer::textWidth(tag) + 6;
    r.drawLine(x + tagWidth, y + 3, x + width, y + 3, kCardEdge);
    return y + Renderer::textHeight() + 4;
}

/// 0..1 confidence as a labeled horizontal bar.
void confidenceBar(Renderer& r, std::int32_t x, std::int32_t y, std::int32_t width,
                   double confidence, Color color) {
    const auto clamped = std::clamp(confidence, 0.0, 1.0);
    r.drawRect({x, y, width, 5}, kCardEdge);
    const auto fill = static_cast<std::int32_t>(clamped * (width - 2));
    if (fill > 0) r.fillRect({x + 1, y + 1, fill, 3}, color);
}

}  // namespace

void drawObservationCard(Renderer& r, const observation::EngineeringObservation& record) {
    const auto info = r.displayInfo();
    const std::int32_t margin = 10;
    const std::int32_t x = margin;
    const std::int32_t width = info.width - 2 * margin;
    const auto charsPerLine = static_cast<std::size_t>(width / 6);
    std::int32_t y = margin;

    r.clear(kBackground);
    r.drawRect({margin - 4, margin - 4, width + 8, info.height - 2 * margin + 8}, kCardEdge);

    // Header: app identity + record identity.
    r.drawText(x, y, "ENGINEERING SCOUT", kAccent);
    const std::string stamp = record.observationId + "  " + record.timestampUtc;
    r.drawText(x + width - Renderer::textWidth(stamp), y, stamp, kDim);
    y += Renderer::textHeight() + 8;

    // Headline: the derived measurement is the product's answer.
    const auto* lengthMm = findClaim(record.derived, "subject_length");
    const auto* widthMm = findClaim(record.derived, "subject_width");
    if (lengthMm && widthMm) {
        char headline[64];
        std::snprintf(headline, sizeof(headline), "%.1f x %.1f mm",
                      std::get<double>(lengthMm->value), std::get<double>(widthMm->value));
        r.drawText(x, y, headline, kInk, 2);
        y += Renderer::textHeight(2) + 4;
    }

    // Inferred identity line under the headline, when present.
    const auto* classClaim = findClaim(record.inferred, "fastener_class");
    const auto* nominalClaim = findClaim(record.inferred, "nominal_size");
    if (classClaim) {
        std::string identity = std::get<std::string>(classClaim->value);
        if (nominalClaim) identity += "  ~" + std::get<std::string>(nominalClaim->value);
        r.drawText(x, y, identity, kInferred);
        const auto barX = x + Renderer::textWidth(identity) + 8;
        confidenceBar(r, barX, y + 1, 50, classClaim->confidence.value_or(0.0), kInferred);
        y += Renderer::textHeight() + 8;
    } else {
        y += 4;
    }

    // OBSERVED — pixel facts.
    y = sectionHeader(r, x, y, width, "OBSERVED", kObserved);
    char observedLine[64];
    std::snprintf(observedLine, sizeof(observedLine), "%zu pixel facts from the source frame",
                  record.observed.size());
    r.drawText(x, y, observedLine, kDim);
    y += Renderer::textHeight() + 6;

    // DERIVED — every derived claim, name = value.
    y = sectionHeader(r, x, y, width, "DERIVED", kDerived);
    for (const auto& claim : record.derived) {
        r.drawText(x, y, claim.name, kInk);
        const auto value = formatValue(claim);
        r.drawText(x + width - Renderer::textWidth(value), y, value, kDerived);
        y += Renderer::textHeight() + 2;
    }
    y += 4;

    // INFERRED — claims with confidence bars; the section exists even when
    // empty so its absence is legible.
    y = sectionHeader(r, x, y, width, "INFERRED", kInferred);
    if (record.inferred.empty()) {
        r.drawText(x, y, "none", kDim);
        y += Renderer::textHeight() + 2;
    }
    for (const auto& claim : record.inferred) {
        r.drawText(x, y, claim.name, kInk);
        const auto value = formatValue(claim);
        const auto barWidth = 40;
        const auto valueX = x + width - Renderer::textWidth(value) - barWidth - 8;
        r.drawText(valueX, y, value, kInferred);
        confidenceBar(r, x + width - barWidth, y + 1, barWidth, claim.confidence.value_or(0.0),
                      kInferred);
        y += Renderer::textHeight() + 2;
    }
    y += 4;

    // UNRESOLVED — names only; reasons live in the record.
    y = sectionHeader(r, x, y, width, "UNRESOLVED", kUnresolved);
    if (record.unresolved.empty()) {
        r.drawText(x, y, "none", kDim);
        y += Renderer::textHeight() + 2;
    } else {
        std::string names;
        for (const auto& item : record.unresolved) {
            if (!names.empty()) names += "  ";
            names += item.name;
        }
        for (const auto& line : wrap(names, charsPerLine, 2)) {
            r.drawText(x, y, line, kUnresolved);
            y += Renderer::textHeight() + 2;
        }
    }
    y += 4;

    // NEXT — the first active recommendation, if any.
    if (!record.recommendedNextObservations.empty()) {
        y = sectionHeader(r, x, y, width, "NEXT", kAccent);
        for (const auto& line :
             wrap(record.recommendedNextObservations.front().action, charsPerLine, 2)) {
            r.drawText(x, y, line, kInk);
            y += Renderer::textHeight() + 2;
        }
    }
}

void drawNoObservationCard(Renderer& r, std::string_view detail) {
    const auto info = r.displayInfo();
    r.clear(kBackground);
    r.drawText(10, 10, "ENGINEERING SCOUT", kAccent);
    r.drawText(10, 34, "No observations yet", kInk, 2);
    std::int32_t y = 34 + Renderer::textHeight(2) + 8;
    for (const auto& line : wrap(detail, static_cast<std::size_t>((info.width - 20) / 6), 3)) {
        r.drawText(10, y, line, kDim);
        y += Renderer::textHeight() + 2;
    }
}

EngineeringScoutApp::EngineeringScoutApp(std::filesystem::path observationsRoot)
    : manifest_{"one.platypus.scout", "Engineering Scout", "0.1.0", false, false},
      root_(std::move(observationsRoot)) {}

std::unique_ptr<appfw::IApp> EngineeringScoutApp::create() {
    return std::make_unique<EngineeringScoutApp>();
}

const appfw::AppManifest& EngineeringScoutApp::manifest() const noexcept {
    return manifest_;
}

void EngineeringScoutApp::loadNewest() {
    record_.reset();
    loadDetail_ = "capture one with engineering_scout_capture into '" + root_.string() + "'";

    std::error_code ec;
    if (!std::filesystem::is_directory(root_, ec)) return;

    // scan-NNNN ids sort lexicographically; the last one is the newest.
    std::vector<std::filesystem::path> records;
    for (const auto& entry : std::filesystem::directory_iterator(root_, ec)) {
        const auto candidate = entry.path() / "observation.json";
        if (entry.is_directory(ec) && std::filesystem::exists(candidate, ec))
            records.push_back(candidate);
    }
    if (records.empty()) return;
    std::sort(records.begin(), records.end());

    std::ifstream in(records.back(), std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    auto outcome = observation::fromJson(text.str());
    if (!outcome.ok()) {
        loadDetail_ = "newest record failed to parse: " + outcome.error;
        return;
    }
    record_ = std::move(*outcome.record);
}

void EngineeringScoutApp::onStart(appfw::AppContext&) {
    exitRequested_ = false;
    needsRedraw_ = true;
    loadNewest();
}

void EngineeringScoutApp::onStop() {}

void EngineeringScoutApp::onButton(const hal::ButtonEvent& event) {
    if (!event.pressed) return;
    if (event.id == 1) {
        loadNewest();
        needsRedraw_ = true;
    } else {
        exitRequested_ = true;
    }
}

void EngineeringScoutApp::onFrame(appfw::AppContext& ctx, std::chrono::milliseconds) {
    if (exitRequested_) {
        ctx.requestLaunch("");
        return;
    }
    if (!needsRedraw_) return;
    if (record_) {
        drawObservationCard(ctx.renderer, *record_);
    } else {
        drawNoObservationCard(ctx.renderer, loadDetail_);
    }
    ctx.renderer.present();
    needsRedraw_ = false;
}

}  // namespace platypus::apps
