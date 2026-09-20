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
view facing left, shaded in four tones using the matrix's grayscale mode:

```
        ##########              crown of the head   (mid edges, full centre)
      ##  ##########            eye notch (off)
##########################      bill (dim→mid) | body (full) | tail (mid→dim)
##########################
      ####################      body + tail
      ##############            belly (mid, in shadow)
        ####    ####            webbed feet (dim, under the water)
```

Proportion carries the read at this size: the bill is two rows tall and the
tail three, against a six-row body, so the thin shapes either side are
unmistakably a bill and a paddle tail. The tones give it volume: lit from
above, darker underneath, extremities fading out.

## Editing the mascot

Frames are literal brightness tokens in `kPlatypus[][8][13]` — `o` off, `d`
dim, `m` mid, `X` full — so what you see in the source is what lights up. Edit
by eye; no bitmap tooling needed. The driver quantises to 8 hardware levels, so
the token values are chosen to land on levels 1 / 3 / 7.

## Porting note

The UNO Q matrix is **8 × 13**, unlike the UNO R4 WiFi's 12 × 8, so R4 examples
and frame-packing tools do not transfer. Only `showFrame()` and two lines in
`setup()` touch the vendor API:

```cpp
matrix.begin();
matrix.setGrayscaleBits(8);   // setup
matrix.draw(&frame[0][0]);    // showFrame — row-major 8x13 bytes, 0..255
```

**Verified on hardware** (2026-09-19) against `arduino:zephyr` 1.0.0 /
`Arduino_LED_Matrix` 0.1.3 via `arduino-cli compile --fqbn
arduino:zephyr:unoq` and `arduino-cli upload -p COM7`. The upload goes through
the board's Linux side, which drives OpenOCD over SWD to the STM32U585. Note
the 1-bit `renderBitmap()` macro on this core takes a non-`const` pointer, which
is why the first (unshaded) revision needed a `const_cast`; the grayscale
`draw()` path takes `const` directly.
