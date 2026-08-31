// PlatypusOS app — Engineering Scout result card.
//
// MVP build-order step 5 (docs/contest/DIGIKEY_ENGINEERING_SCOUT_MVP.md):
// the human-readable result. One card per observation, rendering the four
// evidence classes with a strict visual separation — OBSERVED / DERIVED /
// INFERRED / UNRESOLVED — plus the active next-observation recommendation.
// The contract's honesty rules are the UI's layout: measurement never shares
// a section with inference.
//
// v1 shows the newest record under the observations root and refreshes on
// button 1; any other button exits to the launcher.
#pragma once

#include <platypus/appfw/IApp.hpp>
#include <platypus/observation/Observation.hpp>
#include <platypus/renderer/Renderer.hpp>

#include <filesystem>
#include <optional>

namespace platypus::apps {

/// Draws one observation as a full-screen card. Pure function of the record
/// and the renderer's geometry — shared by the app, tests, and ui_preview.
void drawObservationCard(renderer::Renderer& renderer,
                         const observation::EngineeringObservation& record);

/// Full-screen empty state when no observation exists yet.
void drawNoObservationCard(renderer::Renderer& renderer, std::string_view detail);

class EngineeringScoutApp final : public appfw::IApp {
   public:
    explicit EngineeringScoutApp(std::filesystem::path observationsRoot = "observations");

    static std::unique_ptr<appfw::IApp> create();

    [[nodiscard]] const appfw::AppManifest& manifest() const noexcept override;

    void onStart(appfw::AppContext& ctx) override;
    void onStop() override;
    void onFrame(appfw::AppContext& ctx, std::chrono::milliseconds dt) override;
    void onButton(const hal::ButtonEvent& event) override;

   private:
    void loadNewest();

    appfw::AppManifest manifest_;
    std::filesystem::path root_;
    std::optional<observation::EngineeringObservation> record_;
    std::string loadDetail_;
    bool needsRedraw_ = true;
    bool exitRequested_ = false;
};

}  // namespace platypus::apps
