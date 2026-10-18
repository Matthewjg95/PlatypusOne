#include <platypus/shell/Shell.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace platypus::shell {

namespace {

using renderer::Color;
using renderer::Rect;
using Renderer = renderer::Renderer;

constexpr Color kBackground{12, 14, 18};
constexpr Color kPanel{28, 32, 40};
constexpr Color kPanelDim{20, 23, 29};
constexpr Color kText{230, 232, 236};
constexpr Color kMuted{140, 146, 158};
constexpr Color kAccent{255, 196, 0};
constexpr Color kGood{80, 200, 120};
constexpr Color kBill{40, 44, 52};
constexpr Color kBody{150, 104, 62};

constexpr std::string_view kLauncherId = "one.platypus.launcher";

Color mix(Color a, Color b, double t) {
    t = std::clamp(t, 0.0, 1.0);
    const auto m = [&](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(std::lround(x + (y - x) * t));
    };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b)};
}

void fillEllipse(Renderer& r, std::int32_t cx, std::int32_t cy, std::int32_t rx, std::int32_t ry,
                 Color c) {
    for (std::int32_t dy = -ry; dy <= ry; ++dy) {
        const double f = 1.0 - static_cast<double>(dy * dy) / std::max(1, ry * ry);
        const auto half = static_cast<std::int32_t>(std::lround(rx * std::sqrt(std::max(0.0, f))));
        r.fillRect({cx - half, cy + dy, 2 * half + 1, 1}, c);
    }
}

void thickLine(Renderer& r, std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1,
               std::int32_t t, Color c) {
    for (std::int32_t o = -t / 2; o <= t / 2; ++o) {
        r.drawLine(x0 + o, y0, x1 + o, y1, c);
        r.drawLine(x0, y0 + o, x1, y1 + o, c);
    }
}

void centredText(Renderer& r, std::int32_t cx, std::int32_t y, std::string_view text, Color c,
                 std::int32_t scale) {
    r.drawText(cx - Renderer::textWidth(text, scale) / 2, y, text, c, scale);
}

/// Word-wraps into `box`, at most `maxLines` lines; returns the y below.
std::int32_t wrapped(Renderer& r, const Rect& box, std::string_view text, Color c,
                     std::int32_t scale, std::int32_t maxLines) {
    std::int32_t y = box.y;
    std::string line;
    std::int32_t lines = 0;
    const auto flush = [&] {
        if (line.empty() || lines >= maxLines) return;
        r.drawText(box.x, y, line, c, scale);
        y += Renderer::textHeight(scale) + 3 * scale;
        ++lines;
        line.clear();
    };
    std::size_t i = 0;
    while (i <= text.size()) {
        const auto sp = text.find(' ', i);
        const auto word = text.substr(i, sp == text.npos ? text.npos : sp - i);
        const std::string candidate = line.empty() ? std::string(word) : line + " " + std::string(word);
        if (!line.empty() && Renderer::textWidth(candidate, scale) > box.w) flush();
        line = line.empty() ? std::string(word) : line + " " + std::string(word);
        if (sp == text.npos) break;
        i = sp + 1;
    }
    flush();
    return y;
}

/// The mascot: a platypus seen from the side, built from ellipses.
void drawPlatypus(Renderer& r, std::int32_t cx, std::int32_t cy, std::int32_t size) {
    const std::int32_t u = std::max(2, size / 16);
    fillEllipse(r, cx - 5 * u, cy + 2 * u, 5 * u, 2 * u, kBody);   // tail
    fillEllipse(r, cx, cy, 6 * u, 4 * u, kBody);                   // body
    fillEllipse(r, cx + 6 * u, cy - u, 3 * u, 3 * u, kBody);       // head
    fillEllipse(r, cx + 10 * u, cy, 3 * u, u + u / 2, kBill);      // bill
    fillEllipse(r, cx + 7 * u, cy - 2 * u, u / 2 + 1, u / 2 + 1, kText);  // eye
    r.fillRect({cx - 3 * u, cy + 3 * u, u * 2, u * 2}, kBill);     // feet
    r.fillRect({cx + 2 * u, cy + 3 * u, u * 2, u * 2}, kBill);
    // a hard hat: the engineer's
    fillEllipse(r, cx + 6 * u, cy - 4 * u, 3 * u, 2 * u, kAccent);
    r.fillRect({cx + 2 * u, cy - 4 * u, 8 * u, u}, kAccent);
}

}  // namespace

void drawAppIcon(Renderer& r, const Rect& box, std::string_view key) {
    const std::int32_t s = std::min(box.w, box.h);
    const std::int32_t x = box.x + (box.w - s) / 2, y = box.y + (box.h - s) / 2;
    const std::int32_t t = std::max(2, s / 24);
    if (key == "scout") {
        r.fillRect({x + s / 10, y + s / 10, s * 8 / 10, s * 8 / 10}, kText);  // the card
        r.fillRect({x + s / 5, y + s / 5, s / 5, s / 5}, kBackground);  // 20 mm square
        thickLine(r, x + s / 2, y + s * 4 / 10, x + s * 8 / 10, y + s * 7 / 10, s / 10, kBill);
        thickLine(r, x + s * 45 / 100, y + s * 35 / 100, x + s * 55 / 100, y + s * 45 / 100, s / 6,
                  kBill);  // bolt head
    } else if (key == "depth") {
        static constexpr std::array<Color, 4> v{
            {{253, 231, 37}, {94, 201, 98}, {33, 145, 140}, {68, 1, 84}}};
        const std::int32_t c = s * 8 / 10 / 4;
        const std::int32_t ox = x + (s - 4 * c) / 2, oy = y + (s - 4 * c) / 2;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                r.fillRect({ox + j * c, oy + i * c, c - t, c - t},
                           v[static_cast<std::size_t>(std::min(3, (i + j) / 2))]);
    } else if (key == "shadowscan") {
        const std::array<std::array<int, 2>, 8> p{
            {{20, 30}, {55, 15}, {80, 30}, {75, 55}, {85, 75}, {50, 85}, {25, 70}, {30, 50}}};
        for (std::size_t i = 0; i < p.size(); ++i) {
            const auto& a = p[i];
            const auto& b = p[(i + 1) % p.size()];
            thickLine(r, x + a[0] * s / 100 + s / 20, y + a[1] * s / 100 + s / 20,
                      x + b[0] * s / 100 + s / 20, y + b[1] * s / 100 + s / 20, t, kPanel);
            thickLine(r, x + a[0] * s / 100, y + a[1] * s / 100, x + b[0] * s / 100,
                      y + b[1] * s / 100, t, kAccent);
        }
    } else if (key == "roomscan") {
        const std::int32_t m = s / 8;
        thickLine(r, x + m, y + m, x + s - m, y + m, t, kText);
        thickLine(r, x + s - m, y + m, x + s - m, y + s - m, t, kText);
        thickLine(r, x + s - m, y + s - m, x + s / 2, y + s - m, t, kText);
        thickLine(r, x + s * 3 / 10, y + s - m, x + m, y + s - m, t, kText);  // door gap
        thickLine(r, x + m, y + s - m, x + m, y + m, t, kText);
        fillEllipse(r, x + s * 4 / 10, y + s * 6 / 10, s / 20, s / 20, kAccent);
        thickLine(r, x + s * 4 / 10, y + s * 6 / 10, x + s * 7 / 10, y + s * 3 / 10, 1, kAccent);
        thickLine(r, x + s * 4 / 10, y + s * 6 / 10, x + s * 8 / 10, y + s * 5 / 10, 1, kAccent);
    } else if (key == "viewer3d") {
        const std::int32_t a = s * 45 / 100, o = s / 5;
        const std::int32_t x0 = x + s / 10, y0 = y + s * 3 / 10;
        const auto sq = [&](std::int32_t px, std::int32_t py, Color c) {
            thickLine(r, px, py, px + a, py, t, c);
            thickLine(r, px + a, py, px + a, py + a, t, c);
            thickLine(r, px + a, py + a, px, py + a, t, c);
            thickLine(r, px, py + a, px, py, t, c);
        };
        sq(x0 + o, y0 - o, kMuted);
        for (const auto& [dx, dy] : std::array<std::array<int, 2>, 4>{{{0, 0}, {a, 0}, {a, a}, {0, a}}})
            thickLine(r, x0 + dx, y0 + dy, x0 + dx + o, y0 + dy - o, t, kMuted);
        sq(x0, y0, kAccent);
    } else {  // system
        const std::int32_t h = s / 8;
        const std::array<int, 3> fill{{80, 45, 65}};
        for (int i = 0; i < 3; ++i) {
            const Rect bar{x + s / 10, y + s / 5 + i * 2 * h, s * 8 / 10, h};
            r.fillRect(bar, kPanel);
            r.fillRect({bar.x, bar.y, bar.w * fill[static_cast<std::size_t>(i)] / 100, bar.h},
                       i == 1 ? kAccent : kGood);
        }
    }
}

ShellLayout computeShellLayout(const hal::DisplayInfo& info) {
    const std::int32_t w = info.width, h = info.height;
    ShellLayout l;
    l.textScale = std::max(1, std::min(w, h) / 240);
    const std::int32_t bh = std::max(28, h / 13);
    l.bar = {0, 0, w, bh};
    l.home = {0, 0, std::max(96, w / 6), bh};
    l.content = {0, bh, w, h - bh};
    const std::int32_t cw = std::min(w * 58 / 100, h * 9 / 10);
    const std::int32_t ch = h * 64 / 100;
    l.card = {(w - cw) / 2, h * 15 / 100, cw, ch};
    l.pitch = cw + std::max(16, w / 20);
    l.dotsY = l.card.y + ch + std::max(12, h / 24);
    return l;
}

Shell::Shell(const appfw::AppRegistry& registry, appfw::AppContext context, ShellOptions options)
    : registry_(registry), context_(std::move(context)), options_(std::move(options)) {
    layout_ = computeShellLayout(context_.renderer.displayInfo());
    for (const auto& m : registry_.manifests())
        if (m.id != kLauncherId) cards_.push_back(m);
    carousel_.setPages(static_cast<std::int32_t>(cards_.size()));
    carousel_.setPitch(layout_.pitch);
    context_.requestLaunch = [this](const std::string& id) {
        if (id.empty())
            pendingHome_ = true;
        else
            pendingLaunch_ = id;
    };
}

Shell::~Shell() {
    stopActive();
}

std::string Shell::activeAppId() const {
    return active_ ? active_->manifest().id : std::string{};
}

void Shell::skipSplash() {
    if (state_ == State::Splash) state_ = State::Home;
}

void Shell::stopActive() {
    if (active_) {
        active_->onStop();
        active_.reset();
    }
}

void Shell::open(std::string_view appId) {
    auto app = registry_.create(appId);
    if (!app) return;
    stopActive();
    active_ = std::move(app);
    context_.contentArea = layout_.content;
    active_->onStart(context_);
    state_ = State::App;
    for (std::size_t i = 0; i < cards_.size(); ++i)
        if (cards_[i].id == appId) carousel_.jumpTo(static_cast<std::int32_t>(i));
}

void Shell::home() {
    stopActive();
    state_ = State::Home;
}

void Shell::onTouch(const hal::TouchEvent& e) {
    using T = hal::TouchEvent::Type;
    switch (state_) {
        case State::Splash:
            if (e.type == T::Down) skipSplash();
            return;
        case State::App:
            if (e.type == T::Down && e.x < layout_.home.x + layout_.home.w &&
                e.y < layout_.home.y + layout_.home.h) {
                pendingHome_ = true;
                return;
            }
            if (e.y < layout_.bar.y + layout_.bar.h) return;  // the bar is the shell's
            if (active_) active_->onTouch(e);
            return;
        case State::Home:
            if (e.type == T::Down) {
                carousel_.press(e.x, e.y);
            } else if (e.type == T::Move) {
                carousel_.drag(e.x, e.y);
            } else {
                const auto rel = carousel_.release(e.x, e.y);
                if (!rel.tap || cards_.empty()) return;
                const auto& c = layout_.card;
                if (rel.x >= c.x && rel.x < c.x + c.w && rel.y >= c.y && rel.y < c.y + c.h) {
                    if (std::abs(carousel_.position() - carousel_.page()) < 0.15)
                        pendingLaunch_ = cards_[static_cast<std::size_t>(carousel_.page())].id;
                } else if (rel.y >= c.y && rel.y < c.y + c.h) {
                    carousel_.step(rel.x < c.x ? -1 : 1);  // tapped a neighbour: bring it in
                }
            }
            return;
    }
}

void Shell::onButton(const hal::ButtonEvent& e) {
    if (state_ == State::Splash) {
        if (e.pressed) skipSplash();
        return;
    }
    if (state_ == State::App) {
        if (active_) active_->onButton(e);
        return;
    }
    if (!e.pressed || cards_.empty()) return;
    if (e.id == options_.buttonNext) {
        if (carousel_.page() == carousel_.pages() - 1)
            carousel_.step(-(carousel_.pages() - 1));  // wrap, animated
        else
            carousel_.step(1);
    } else if (e.id == options_.buttonOpen) {
        pendingLaunch_ = cards_[static_cast<std::size_t>(carousel_.page())].id;
    }
}

void Shell::frame(std::chrono::milliseconds dt) {
    if (pendingHome_) {
        pendingHome_ = false;
        pendingLaunch_.clear();
        home();
    }
    if (!pendingLaunch_.empty()) {
        const auto id = std::move(pendingLaunch_);
        pendingLaunch_.clear();
        open(id);
    }
    switch (state_) {
        case State::Splash:
            splashElapsed_ += dt;
            drawSplash();
            if (splashElapsed_ >= options_.splash) state_ = State::Home;
            break;
        case State::Home:
            carousel_.tick(dt);
            drawHome();
            break;
        case State::App:
            if (active_) active_->onFrame(context_, dt);
            drawBar();
            break;
    }
    (void)context_.renderer.present();
}

void Shell::drawSplash() {
    auto& r = context_.renderer;
    const auto info = r.displayInfo();
    const std::int32_t w = info.width, h = info.height, s = layout_.textScale;
    r.clear(kBackground);
    drawPlatypus(r, w / 2 - h / 40, h * 30 / 100, h / 3);
    const std::int32_t big = std::max(2, std::min(w / (12 * 6 + 12), h / 16));
    const std::int32_t wordY = h * 52 / 100;
    const std::int32_t ww = Renderer::textWidth("PLATYPUS ONE", big);
    r.drawText(w / 2 - ww / 2, wordY, "PLATYPUS", kAccent, big);
    r.drawText(w / 2 - ww / 2 + Renderer::textWidth("PLATYPUS ", big), wordY, "ONE", kText, big);
    centredText(r, w / 2, wordY + Renderer::textHeight(big) + 4 * s,
                "PHYSICAL PARTS TO ENGINEERING DATA", kMuted, s);
    const Rect bar{w / 2 - w / 5, h * 76 / 100, 2 * (w / 5), std::max(4, h / 80)};
    r.fillRect(bar, kPanel);
    const double p = std::min(1.0, static_cast<double>(splashElapsed_.count()) /
                                       std::max<std::int64_t>(1, options_.splash.count()));
    r.fillRect({bar.x, bar.y, static_cast<std::int32_t>(bar.w * p), bar.h}, kAccent);
    const std::string ver = "v" + options_.version;
    r.drawText(w - Renderer::textWidth(ver, s) - 4 * s, h - Renderer::textHeight(s) - 4 * s, ver,
               kMuted, s);
}

void Shell::drawHome() {
    auto& r = context_.renderer;
    const auto info = r.displayInfo();
    const std::int32_t w = info.width, s = layout_.textScale;
    const std::int32_t m = std::max(8, static_cast<std::int32_t>(info.height) / 30);
    r.clear(kBackground);
    r.drawText(m, m, "PLATYPUS", kAccent, s);
    r.drawText(m + Renderer::textWidth("PLATYPUS ", s), m, "ONE", kText, s);
    const std::string hint = "SWIPE - TAP TO OPEN";
    r.drawText(w - m - Renderer::textWidth(hint, s), m, hint, kMuted, s);

    const auto& base = layout_.card;
    for (std::size_t i = 0; i < cards_.size(); ++i) {
        const double off = static_cast<double>(i) - carousel_.position();
        if (std::abs(off) > 2.0) continue;
        const Rect c{base.x + static_cast<std::int32_t>(std::lround(off * layout_.pitch)), base.y,
                     base.w, base.h};
        const double focus = 1.0 - std::min(1.0, std::abs(off));
        const auto& app = cards_[i];
        r.fillRect(c, mix(kPanelDim, kPanel, focus));
        r.drawRect(c, focus > 0.9 ? kAccent : mix(kBackground, kMuted, focus));
        const std::int32_t pad = 6 * s;
        const Rect iconBox{c.x + pad, c.y + pad, c.w - 2 * pad, c.h * 42 / 100};
        drawAppIcon(r, iconBox, app.icon);
        std::int32_t y = iconBox.y + iconBox.h + 4 * s;
        r.drawText(c.x + pad, y, app.name, focus > 0.5 ? kText : kMuted, s + 1);
        y += Renderer::textHeight(s + 1) + 4 * s;
        wrapped(r, {c.x + pad, y, c.w - 2 * pad, c.h}, app.tagline, kMuted, s, 3);
        const std::string badge = app.preview ? "PREVIEW" : "READY";
        const Color bc = app.preview ? kMuted : kGood;
        r.drawText(c.x + c.w - pad - Renderer::textWidth(badge, s), c.y + pad, badge, bc, s);
    }

    // Page dots.
    const std::int32_t n = static_cast<std::int32_t>(cards_.size());
    const std::int32_t d = std::max(4, s * 3), gap = d * 2;
    std::int32_t x = w / 2 - (n * (d + gap) - gap) / 2;
    for (std::int32_t i = 0; i < n; ++i, x += d + gap)
        r.fillRect({x, layout_.dotsY, d, d}, i == carousel_.page() ? kAccent : kPanel);
}

void Shell::drawBar() {
    auto& r = context_.renderer;
    const std::int32_t s = layout_.textScale;
    r.fillRect(layout_.bar, kPanel);
    r.fillRect({layout_.bar.x, layout_.bar.y + layout_.bar.h - 1, layout_.bar.w, 1}, kBackground);
    const auto ty = layout_.bar.y + (layout_.bar.h - Renderer::textHeight(s)) / 2;
    r.drawText(layout_.home.x + 4 * s, ty, "< HOME", kAccent, s);
    if (active_) {
        const auto& name = active_->manifest().name;
        r.drawText(layout_.bar.w / 2 - Renderer::textWidth(name, s) / 2, ty, name, kText, s);
    }
    const std::string ver = "v" + options_.version;
    r.drawText(layout_.bar.w - Renderer::textWidth(ver, s) - 4 * s, ty, ver, kMuted, s);
}

}  // namespace platypus::shell
