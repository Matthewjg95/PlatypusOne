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
// written as literal 0/1 rows so the shape is editable by eye — what you see
// in the source is what lights up.
//
// PORTING NOTE: the UNO Q's matrix is 8x13 (104 LEDs), unlike the UNO R4
// WiFi's 12x8. Only showFrame() below touches the vendor API; if your core
// exposes a different call, that is the single function to adapt.

#include "Arduino_LED_Matrix.h"

namespace {

constexpr uint8_t kRows = 8;
constexpr uint8_t kCols = 13;
constexpr uint8_t kFrameCount = 4;

/// Hold time per frame. Slow enough to read as paddling, not flicker.
constexpr unsigned long kFrameHoldMs = 220;

// clang-format off
// Proportions carry the read at this size: the bill is only two rows tall and
// the tail three, against a body six rows tall — so the thin things sticking
// out the sides are unmistakably a bill (left) and a paddle tail (right).
const uint8_t kPlatypus[kFrameCount][kRows][kCols] = {
    // 0 — glide: feet tucked under the body.
    {{0,0,0,0,0,0,0,0,0,0,0,0,0},
     {0,0,0,0,1,1,1,1,1,0,0,0,0},   // crown of the head
     {0,0,0,1,0,1,1,1,1,1,0,0,0},   // the dark pixel is the eye
     {1,1,1,1,1,1,1,1,1,1,1,1,1},   // bill | body | tail
     {1,1,1,1,1,1,1,1,1,1,1,1,1},
     {0,0,0,1,1,1,1,1,1,1,1,1,1},   // body + tail (bill has ended)
     {0,0,0,1,1,1,1,1,1,1,0,0,0},   // belly
     {0,0,0,0,1,1,0,0,1,1,0,0,0}},  // webbed feet

    // 1 — kick: the feet sweep back.
    {{0,0,0,0,0,0,0,0,0,0,0,0,0},
     {0,0,0,0,1,1,1,1,1,0,0,0,0},
     {0,0,0,1,0,1,1,1,1,1,0,0,0},
     {1,1,1,1,1,1,1,1,1,1,1,1,1},
     {1,1,1,1,1,1,1,1,1,1,1,1,1},
     {0,0,0,1,1,1,1,1,1,1,1,1,1},
     {0,0,0,1,1,1,1,1,1,1,0,0,0},
     {0,0,0,0,0,1,1,0,0,1,1,0,0}},

    // 2 — glide again, so the kick returns rather than jumping.
    {{0,0,0,0,0,0,0,0,0,0,0,0,0},
     {0,0,0,0,1,1,1,1,1,0,0,0,0},
     {0,0,0,1,0,1,1,1,1,1,0,0,0},
     {1,1,1,1,1,1,1,1,1,1,1,1,1},
     {1,1,1,1,1,1,1,1,1,1,1,1,1},
     {0,0,0,1,1,1,1,1,1,1,1,1,1},
     {0,0,0,1,1,1,1,1,1,1,0,0,0},
     {0,0,0,0,1,1,0,0,1,1,0,0,0}},

    // 3 — blink: the eye notch fills in for one beat.
    {{0,0,0,0,0,0,0,0,0,0,0,0,0},
     {0,0,0,0,1,1,1,1,1,0,0,0,0},
     {0,0,0,1,1,1,1,1,1,1,0,0,0},
     {1,1,1,1,1,1,1,1,1,1,1,1,1},
     {1,1,1,1,1,1,1,1,1,1,1,1,1},
     {0,0,0,1,1,1,1,1,1,1,1,1,1},
     {0,0,0,1,1,1,1,1,1,1,0,0,0},
     {0,0,0,0,1,1,0,0,1,1,0,0,0}},
};
// clang-format on

ArduinoLEDMatrix matrix;

/// The only vendor-API call. If your UNO Q core names this differently, adapt
/// here and the frames above stay exactly as they are.
void showFrame(const uint8_t (&frame)[kRows][kCols]) {
    matrix.renderBitmap(frame, kRows, kCols);
}

}  // namespace

void setup() {
    matrix.begin();
}

void loop() {
    for (uint8_t i = 0; i < kFrameCount; ++i) {
        showFrame(kPlatypus[i]);
        delay(kFrameHoldMs);
    }
}
