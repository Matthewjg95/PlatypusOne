# Four-inch display gate checklist

Companion to `DISPLAY_4IN_GATE.md`. As of 2026-10-21, all gates below are OPEN.

- [ ] Verify panel and carrier connector pinouts, five-volt supply and ground.
- [ ] Compile the ST7701S driver against the UNO Q kernel and validate the overlay without installing it.
- [ ] Preserve a working five-inch display recovery path.
- [ ] Record display pixels and touch mapping on the actual hardware.
- [ ] Repeat cold and warm boots; capture simultaneous CSI camera operation.
- [ ] Confirm Fusion fit, cable routing and left-hand grip geometry.

Software build success is not physical compatibility. Keep the existing display working until a candidate passes.
