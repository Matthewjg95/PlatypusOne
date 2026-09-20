// Platypus One — LED matrix mascot for the Arduino UNO Q (8 rows x 13 cols).
//
// Replaces the stock heart/animation demo with the project's own mascot, and
// doubles as the "MCU side alive" check in the first-boot checklist
// (docs/hardware/TEST_CHECKLISTS.md §1) — if the platypus paddles, the MCU is
// running our code rather than factory firmware.
//
// The silhouette is a side view facing left: flat duck bill at the front, an
// eye notch behind it, a humped body, a broad paddle tail, and two webbed feet
// that sweep back and forth so the animal reads as swimming. Frames are
// written as literal per-pixel brightness tokens so the shape is editable by
// eye — what you see in the source is what lights up.
//
// SHADING: like the factory boot animation, this uses the matrix's grayscale
// mode (setGrayscaleBits(8) + draw(); the ISR quantises 0..255 to 8 hardware
// levels). Four tones are used: full for the lit top of the body, mid for the
// underside and the bill/tail base, dim for the extremities and the feet in
// the water, off for the eye. Proportion still carries the read — the bill is
// two rows tall and the tail three against a six-row body — the tones just
// give it volume.
//
// PORTING NOTE: the UNO Q's matrix is 8x13 (104 LEDs), unlike the UNO R4
// WiFi's 12x8. Only showFrame() and the two lines in setup() touch the vendor
// API; if your core exposes a different call, that is where to adapt.

#include "Arduino_LED_Matrix.h"

namespace {

constexpr uint8_t kRows = 8;
constexpr uint8_t kCols = 13;
constexpr uint8_t kFrameCount = 4;

/// Hold time per frame. Slow enough to read as paddling, not flicker.
constexpr unsigned long kFrameHoldMs = 220;

// Brightness tokens. Values are chosen so they land on hardware levels
// 1 / 3 / 7 of the driver's 3-bit ramp whether it shifts or scales the byte.
constexpr uint8_t o = 0;    // off
constexpr uint8_t d = 36;   // dim   — bill tip, tail tip, feet underwater
constexpr uint8_t m = 109;  // mid   — belly, body edges, bill/tail base
constexpr uint8_t X = 255;  // full  — top-lit body

// clang-format off
const uint8_t kPlatypus[kFrameCount][kRows][kCols] = {
    // 0 — glide: feet tucked under the body.
    {{o,o,o,o,o,o,o,o,o,o,o,o,o},
     {o,o,o,o,m,X,X,X,m,o,o,o,o},   // crown of the head, lit from above
     {o,o,o,m,o,X,X,X,X,m,o,o,o},   // the dark pixel is the eye
     {d,m,m,X,X,X,X,X,X,X,m,d,d},   // bill | body | tail
     {d,m,m,X,X,X,X,X,X,X,m,d,d},
     {o,o,o,m,X,X,X,X,X,X,m,d,d},   // body + tail (bill has ended)
     {o,o,o,d,m,m,m,m,m,d,o,o,o},   // belly, in shadow
     {o,o,o,o,d,d,o,o,d,d,o,o,o}},  // webbed feet, under the water

    // 1 — kick: the feet sweep back.
    {{o,o,o,o,o,o,o,o,o,o,o,o,o},
     {o,o,o,o,m,X,X,X,m,o,o,o,o},
     {o,o,o,m,o,X,X,X,X,m,o,o,o},
     {d,m,m,X,X,X,X,X,X,X,m,d,d},
     {d,m,m,X,X,X,X,X,X,X,m,d,d},
     {o,o,o,m,X,X,X,X,X,X,m,d,d},
     {o,o,o,d,m,m,m,m,m,d,o,o,o},
     {o,o,o,o,o,d,d,o,o,d,d,o,o}},

    // 2 — glide again, so the kick returns rather than jumping.
    {{o,o,o,o,o,o,o,o,o,o,o,o,o},
     {o,o,o,o,m,X,X,X,m,o,o,o,o},
     {o,o,o,m,o,X,X,X,X,m,o,o,o},
     {d,m,m,X,X,X,X,X,X,X,m,d,d},
     {d,m,m,X,X,X,X,X,X,X,m,d,d},
     {o,o,o,m,X,X,X,X,X,X,m,d,d},
     {o,o,o,d,m,m,m,m,m,d,o,o,o},
     {o,o,o,o,d,d,o,o,d,d,o,o,o}},

    // 3 — blink: the eye closes to an eyelid (mid) for one beat.
    {{o,o,o,o,o,o,o,o,o,o,o,o,o},
     {o,o,o,o,m,X,X,X,m,o,o,o,o},
     {o,o,o,m,m,X,X,X,X,m,o,o,o},
     {d,m,m,X,X,X,X,X,X,X,m,d,d},
     {d,m,m,X,X,X,X,X,X,X,m,d,d},
     {o,o,o,m,X,X,X,X,X,X,m,d,d},
     {o,o,o,d,m,m,m,m,m,d,o,o,o},
     {o,o,o,o,d,d,o,o,d,d,o,o,o}},
};
// clang-format on

Arduino_LED_Matrix matrix;

/// The only per-frame vendor-API call. Verified against arduino:zephyr 1.0.0
/// (Arduino_LED_Matrix 0.1.3): draw() takes a row-major 8x13 byte buffer of
/// 0..255 brightness values. Frames live in static storage, so it is safe
/// whether the driver copies the buffer or keeps a pointer to it. If your core
/// names this differently, adapt here and the frames above stay as they are.
void showFrame(const uint8_t (&frame)[kRows][kCols]) {
    matrix.draw(&frame[0][0]);
}

}  // namespace

void setup() {
    matrix.begin();
    matrix.setGrayscaleBits(8);  // frame values span 0..255
}

void loop() {
    for (uint8_t i = 0; i < kFrameCount; ++i) {
        showFrame(kPlatypus[i]);
        delay(kFrameHoldMs);
    }
}
