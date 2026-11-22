// PlatypusOS shell — the home carousel's motion, without any drawing.
//
// One card per page. A finger drag moves the strip 1:1; on release it snaps
// to a page: past a fifth of a card, or a quick flick, goes to the next one.
// Dragging past either end gives way with resistance instead of stopping
// dead. Buttons (and later the encoder) step one page. A press that barely
// moves is a tap, which the shell turns into "open the centre card".
#pragma once

#include <chrono>
#include <cstdint>

namespace platypus::shell {

class Carousel {
   public:
    explicit Carousel(std::int32_t pages = 1);

    void setPages(std::int32_t pages);
    /// Pixels between neighbouring card centres; drag distance is measured
    /// against it.
    void setPitch(std::int32_t px);

    void press(std::int32_t x, std::int32_t y);
    void drag(std::int32_t x, std::int32_t y);
    struct Release {
        bool tap = false;  ///< the finger barely moved: open what is under it
        std::int32_t x = 0, y = 0;
    };
    Release release(std::int32_t x, std::int32_t y);

    /// One page left (-1) or right (+1), clamped.
    void step(std::int32_t delta);
    /// Jumps without animation (startup, returning home to the same card).
    void jumpTo(std::int32_t page);

    /// Eases the strip toward its target page.
    void tick(std::chrono::milliseconds dt);

    /// Strip position in pages: 1.5 is halfway between cards 1 and 2.
    [[nodiscard]] double position() const noexcept { return position_; }
    /// The page the strip is resting on or heading to.
    [[nodiscard]] std::int32_t page() const noexcept { return target_; }
    [[nodiscard]] std::int32_t pages() const noexcept { return pages_; }
    [[nodiscard]] bool dragging() const noexcept { return pressed_; }
    [[nodiscard]] bool settled() const noexcept;

    static constexpr std::int32_t kTapSlopPx = 12;
    static constexpr double kSnapFraction = 0.2;
    static constexpr std::int32_t kFlickPx = 40;  ///< within kFlickTime
    static constexpr std::chrono::milliseconds kFlickTime{250};

   private:
    [[nodiscard]] double clampWithResistance(double p) const;

    std::int32_t pages_ = 1;
    std::int32_t pitch_ = 400;
    std::int32_t target_ = 0;
    double position_ = 0.0;

    bool pressed_ = false;
    bool moved_ = false;
    std::int32_t pressX_ = 0, pressY_ = 0;
    double pressPosition_ = 0.0;
    std::chrono::milliseconds sincePress_{0};
};

}  // namespace platypus::shell
