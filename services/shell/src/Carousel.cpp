#include <platypus/shell/Carousel.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace platypus::shell {

Carousel::Carousel(std::int32_t pages) {
    setPages(pages);
}

void Carousel::setPages(std::int32_t pages) {
    pages_ = std::max(1, pages);
    target_ = std::clamp(target_, 0, pages_ - 1);
    position_ = std::clamp(position_, 0.0, static_cast<double>(pages_ - 1));
}

void Carousel::setPitch(std::int32_t px) {
    pitch_ = std::max(1, px);
}

void Carousel::press(std::int32_t x, std::int32_t y) {
    pressed_ = true;
    moved_ = false;
    pressX_ = x;
    pressY_ = y;
    pressPosition_ = position_;
    sincePress_ = std::chrono::milliseconds{0};
}

double Carousel::clampWithResistance(double p) const {
    const double last = pages_ - 1;
    if (p < 0.0) return std::max(-0.25, p * 0.35);
    if (p > last) return std::min(last + 0.25, last + (p - last) * 0.35);
    return p;
}

void Carousel::drag(std::int32_t x, std::int32_t y) {
    if (!pressed_) return;
    if (std::abs(x - pressX_) > kTapSlopPx || std::abs(y - pressY_) > kTapSlopPx) moved_ = true;
    // Finger left moves the strip toward later pages.
    position_ = clampWithResistance(pressPosition_ + static_cast<double>(pressX_ - x) / pitch_);
}

Carousel::Release Carousel::release(std::int32_t x, std::int32_t y) {
    if (!pressed_) return {};
    drag(x, y);
    pressed_ = false;
    if (!moved_) {
        position_ = pressPosition_;
        return {true, x, y};
    }
    const std::int32_t start = static_cast<std::int32_t>(std::lround(pressPosition_));
    const double moved = position_ - pressPosition_;
    const bool flick = sincePress_ <= kFlickTime && std::abs(pressX_ - x) >= kFlickPx;
    std::int32_t to = static_cast<std::int32_t>(std::lround(position_));
    if ((std::abs(moved) >= kSnapFraction || flick) && to == start)
        to = start + (moved > 0 ? 1 : -1);
    target_ = std::clamp(to, 0, pages_ - 1);
    return {};
}

void Carousel::step(std::int32_t delta) {
    target_ = std::clamp(target_ + delta, 0, pages_ - 1);
}

void Carousel::jumpTo(std::int32_t page) {
    target_ = std::clamp(page, 0, pages_ - 1);
    position_ = target_;
}

void Carousel::tick(std::chrono::milliseconds dt) {
    if (pressed_) {
        sincePress_ += dt;
        return;
    }
    // Exponential ease: about 90 % of the way in 250 ms, frame-rate independent.
    const double k = 1.0 - std::exp(-static_cast<double>(dt.count()) / 110.0);
    position_ += (target_ - position_) * k;
    if (std::abs(target_ - position_) < 0.002) position_ = target_;
}

bool Carousel::settled() const noexcept {
    return !pressed_ && position_ == static_cast<double>(target_);
}

}  // namespace platypus::shell
