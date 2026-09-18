# led_matrix_platypus — UNO Q mascot / MCU aliveness check

Draws the Platypus One mascot on the UNO Q's **8 × 13** LED matrix (104 LEDs),
replacing the factory heart/animation demo.

It earns its place in `firmware/` by doubling as the first checklist item's
proof: [TEST_CHECKLISTS.md §1](../../docs/hardware/TEST_CHECKLISTS.md) ends with
"MCU side alive: stock LED matrix demo (or blink) runs". If the platypus
paddles, the MCU is running **our** code, not factory firmware — a stronger
signal than the stock demo, and it photographs well for the contest
documentation score.

## The animation

Four frames at 220 ms: glide → kick → glide → blink. The silhouette is a side
view facing left.

```
        ##########              crown of the head
      ##  ##########            eye notch
##########################      bill | body | tail
##########################
      ####################      body + tail
      ##############            belly
        ####    ####            webbed feet
```

Proportion carries the read at this size: the bill is two rows tall and the
tail three, against a six-row body, so the thin shapes either side are
unmistakably a bill and a paddle tail.

## Editing the mascot

Frames are literal `0`/`1` rows in `kPlatypus[][8][13]` — what you see in the
source is what lights up. Edit by eye; no bitmap tooling needed.

## Porting note

The UNO Q matrix is **8 × 13**, unlike the UNO R4 WiFi's 12 × 8, so R4 examples
and frame-packing tools do not transfer. Only `showFrame()` touches the vendor
API:

```cpp
matrix.renderBitmap(frame, kRows, kCols);
```

**Unverified against hardware.** The exact UNO Q core API could not be
confirmed from this repository's build environment, and the board's MCU runs
Zephyr rather than the classic AVR core. If the call differs, `showFrame()` is
the single function to adapt — the frame data above stays as it is. Record the
result in the checklist when it runs.
