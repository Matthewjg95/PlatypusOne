# tab5_display_client (M5Stack Tab5)

The display-client half of the
[presentation link](../../docs/protocols/presentation.md): receives pixel
tiles over USB CDC, blits them, reports touch. Deliberately dumb — no app
knowledge, per ADR-0001.

Shares `platform/display/include/platypus/hal/link/Framing.hpp` verbatim;
build with the include path, never a copied header (see the sketch header for
the `arduino-cli` line).

## Verification

Per STATUS.md protocol this sketch is **NOT COMPILED** until it builds against
the M5Stack Tab5 core. Bench questions to settle in the first session
(status board steps 5–6 in
[TEST_CHECKLISTS.md](../../docs/hardware/TEST_CHECKLISTS.md)):

- USB topology/role: which end hosts the CDC link (UNO Q USB-C ↔ Tab5 USB-C),
  and the resulting device path on the UNO Q (default assumption
  `/dev/ttyACM0`).
- RGB565 byte order into `pushImage` — flip `setSwapBytes` if colors invert.
- Touch coordinate orientation vs the negotiated geometry (rotation is the
  client's job, spec §5).
- Measure real tile throughput and replace the protocol's §2 estimates.
