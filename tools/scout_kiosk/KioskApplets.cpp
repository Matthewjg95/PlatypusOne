#include "KioskApplets.hpp"

#include <platypus/shell/Applets.hpp>

#include <cstdio>
#include <utility>

namespace platypus::kiosk {

namespace {

appfw::AppManifest manifestFor(const char* id) {
    for (auto m : shell::standardManifests())
        if (m.id == id) {
            m.preview = false;
            return m;
        }
    return {};
}

Rect contentOf(appfw::AppContext& ctx) {
    if (ctx.contentArea.w > 0) return ctx.contentArea;
    const auto info = ctx.renderer.displayInfo();
    return {0, 0, info.width, info.height};
}

/// The Scout layout for the content area: laid out at the area's size, text
/// sized from the whole display, then moved below the shell bar.
Layout scoutLayout(appfw::AppContext& ctx, std::int32_t camW, std::int32_t camH) {
    const Rect a = contentOf(ctx);
    const auto full = ctx.renderer.displayInfo();
    auto l = computeLayout(hal::DisplayInfo{static_cast<std::uint16_t>(a.w),
                                            static_cast<std::uint16_t>(a.h), full.bitsPerPixel},
                           camW, camH);
    l.textScale = std::max(1, static_cast<std::int32_t>(full.height) / 240);
    for (Rect* r : {&l.preview, &l.panel, &l.capture, &l.finish}) {
        r->x += a.x;
        r->y += a.y;
    }
    return l;
}

}  // namespace

// --- Scout -------------------------------------------------------------------

ScoutApplet::ScoutApplet(ScoutDeps deps)
    : manifest_(manifestFor(shell::ids::kScout)), deps_(std::move(deps)) {}

void ScoutApplet::newSession() {
    current_ = std::make_unique<session::ObjectSession>(
        deps_.sessions->nextSessionId(), session::fastenerProfile(),
        observation::CaptureService::currentUtcTimestamp());
}

void ScoutApplet::onStart(appfw::AppContext& ctx) {
    layout_ = scoutLayout(ctx, deps_.camW, deps_.camH);
    screen_ = Screen::Preview;
    if (!current_) newSession();
}

void ScoutApplet::onTouch(const hal::TouchEvent& e) {
    if (e.type != hal::TouchEvent::Type::Down) return;
    if (screen_ != Screen::Preview)
        dismissRequested_ = true;
    else if (inside(layout_.capture, e.x, e.y))
        captureRequested_ = true;
    else if (inside(layout_.finish, e.x, e.y))
        finishRequested_ = true;
}

void ScoutApplet::onButton(const hal::ButtonEvent& e) {
    if (!e.pressed) return;
    if (screen_ != Screen::Preview)
        dismissRequested_ = true;
    else if (e.id == deps_.buttonFinish)
        finishRequested_ = true;
    else if (e.id == deps_.buttonCapture)
        captureRequested_ = true;
}

void ScoutApplet::capture(renderer::Renderer& r) {
    const auto identity = deps_.identity ? deps_.identity() : deps_.device;
    auto out = captureOnce(*deps_.camera, *deps_.captures, deps_.spec, deps_.device, identity,
                           deps_.attach);
    if (out.record) {
        current_->add(sessionCapture(out, current_->profile()));
        std::string error;
        if (!deps_.sessions->save(*current_, &error))
            std::fprintf(stderr, "session save: %s\n", error.c_str());
    }
    status_ = out.status;
    statusColour_ = out.statusColour;
    if (out.measured) {
        apps::drawObservationCard(r, *out.record, out.thumbnail);
        screen_ = Screen::Card;
    }
}

void ScoutApplet::finish(renderer::Renderer& r) {
    const auto g = current_->guidance();
    if (g.measured == 0) {
        status_ = "Nothing measured yet - capture first.";
        statusColour_ = kWarn;
        return;
    }
    const auto reason = g.modelSatisfied ? session::CloseReason::ModelSatisfied
                                         : session::CloseReason::ClosedByOperator;
    const auto result = deps_.sessions->finish(*current_, reason,
                                               observation::CaptureService::currentUtcTimestamp());
    drawSessionSummary(r, *current_, result, reason);
    screen_ = Screen::Summary;
}

void ScoutApplet::onFrame(appfw::AppContext& ctx, std::chrono::milliseconds) {
    auto& r = ctx.renderer;
    if (deps_.camera)
        if (auto f = deps_.camera->capture(std::chrono::milliseconds(30)); f)
            lastRgb_ = frameToRgb(f.value());

    if (screen_ != Screen::Preview) {
        // The card or summary stays on screen (drawn once) until a tap.
        if (std::exchange(dismissRequested_, false)) {
            if (screen_ == Screen::Summary) {
                newSession();
                status_ = "New session " + current_->id() + ".";
                statusColour_ = kMuted;
            }
            screen_ = Screen::Preview;
        } else {
            return;
        }
    }
    if (std::exchange(captureRequested_, false) && deps_.camera && deps_.captures) {
        capture(r);
        if (screen_ != Screen::Preview) return;
    }
    if (std::exchange(finishRequested_, false) && deps_.sessions) {
        finish(r);
        if (screen_ != Screen::Preview) return;
    }
    PanelState p;
    p.sessionId = current_ ? current_->id() : std::string{};
    if (current_) p.guidance = current_->guidance();
    p.status = status_;
    p.statusColour = statusColour_;
    p.noCamera = deps_.camera == nullptr;
    drawPreviewScreen(r, layout_, lastRgb_, deps_.camW, deps_.camH, p);
}

// --- Depth -------------------------------------------------------------------

DepthApplet::DepthApplet(DepthDeps deps)
    : manifest_(manifestFor(shell::ids::kDepth)), deps_(std::move(deps)) {}

void DepthApplet::onStart(appfw::AppContext& ctx) {
    layout_ = computeDepthLayout(contentOf(ctx), false);
    if (deps_.tof) deps_.tof->start();
}

void DepthApplet::onStop() {
    if (deps_.tof) deps_.tof->stop();  // frees the Monitor for tof_log.py
}

void DepthApplet::onTouch(const hal::TouchEvent& e) {
    if (e.type == hal::TouchEvent::Type::Down &&
        (inside(layout_.tof, e.x, e.y) || inside(layout_.fused, e.x, e.y)))
        flipRequested_ = true;
}

void DepthApplet::onFrame(appfw::AppContext& ctx, std::chrono::milliseconds) {
    if (deps_.camera)
        if (auto f = deps_.camera->capture(std::chrono::milliseconds(30)); f)
            lastRgb_ = frameToRgb(f.value());
    if (std::exchange(flipRequested_, false)) deps_.orientation = hal::tof::next(deps_.orientation);
    DepthState st;
    if (deps_.tof) {
        const auto snap = deps_.tof->snapshot();
        st.connected = snap.connected;
        st.frame = snap.frame;
        st.stale = snap.frame && std::chrono::steady_clock::now() - snap.frameArrived >
                                     std::chrono::milliseconds(1500);
        st.hz = snap.hz;
        st.frames = snap.frames;
        st.rejects = snap.rejects;
        st.lastEvent = snap.lastEvent;
        st.lastError = snap.lastError;
        st.endpoint = deps_.tof->endpoint();
    } else {
        st.lastError = "no ToF source on this device";
    }
    st.orientation = deps_.orientation;
    st.fov = deps_.fov;
    drawDepthScreen(ctx.renderer, layout_, lastRgb_, deps_.camW, deps_.camH, st);
}

}  // namespace platypus::kiosk
