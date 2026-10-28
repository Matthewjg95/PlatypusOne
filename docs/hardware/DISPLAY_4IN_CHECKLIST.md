# Four-inch display gate checklist

Companion to [DISPLAY_4IN_GATE.md](DISPLAY_4IN_GATE.md). Updated 2026-10-28. **No physical display, electrical, mechanical or CSI coexistence gate has passed.**

## Non-physical software gate

- [x] Identify the 4-DSI-TOUCH-A controller's upstream compatible, mode, init sequence, lane count and source provenance (Git blob c6cc003a35337971eef670587b200928ca1c1814).
- [x] Add a fail-closed source preflight and compile-only (no installation) harness with synthetic tests. This does **not** mean the module compiled against the UNO Q kernel.
- [ ] Run the compile-only probe on the **UNO Q's matching kernel headers** and archive source blob, kernel release, patch.log, build.log and result.json.
- [ ] Validate generated overlay/DTB offline against the actual board's base DTB and Media Carrier overlay without installing it. Check GPIO polarity, regulator/power assumptions, and private compatible.
- [ ] Preserve an executable recovery path for the known working five-inch display before any panel configuration changes.

## Physical gates — remain OPEN

- [ ] Verify panel/Media Carrier connector pinout, 5 V supply polarity/current capability and shared ground.
- [ ] Confirm screen pixels and touch axes on the actual panel.
- [ ] Repeat cold/warm boots and capture simultaneous CSI camera operation.
- [ ] Confirm Fusion envelope, cable routing, left-hand grip and optical alignment.
- [ ] Record a final procurement/fit decision, distinguishing 4.0 A, 3.5 H and older 4.3 ICN6211 alternatives.

A source audit, synthetic test, CI success or successful module compile is **not** proof that a panel lights, responds to touch, or fits.
