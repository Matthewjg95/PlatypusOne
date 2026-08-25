# ADR-0001 — Dynamic display target via a linked external prototype display

- **Status:** Accepted
- **Date:** 2026-08-24
- **Deciders:** Matthew (owner)

## Context

Four display directions accumulated in the repository without a decision
between them:

| Source | Target |
|---|---|
| [BOM.md](../hardware/BOM.md) rev A | 3.2" ILI9341 320×240 SPI, resistive touch |
| `industrial_design/bounding_boxes/BOM_v0_1.md` | 5–7" LCD, 165×100×8 mm envelope |
| [ESP32-S3-LCD-EV-BOARD-SUB3](../hardware/ESP32-S3-LCD-EV-BOARD-SUB3_V1.3.md) | 4.3" 800×480 parallel RGB, capacitive |
| [TAB5_DISPLAY_PROTOTYPE.md](../roadmap/TAB5_DISPLAY_PROTOTYPE.md) | M5Stack Tab5 driven over a link |

Meanwhile the software hardcoded 320×240 in the host simulator, so the one
resolution the code could actually run was the one least likely to ship.

The forcing constraints:

1. **The final panel cannot be chosen yet.** Enclosure depth, connector
   placement, sourcing, and the $300 PCBWay budget all feed that choice, and
   none of them are settled. The AU 2027 contest hardware application (now due
   **Sep 7, 2026** — see [contest snapshot](../contest/autodesk-au2027-contest-snapshot.md))
   does not require a frozen panel.
2. **Software must not wait for it.** The host-first workflow exists precisely
   so that code progresses while hardware is undecided; ~13 weeks from the
   Sep 18 hardware announcement to the Dec 20 submission leaves no slack for a
   UI rewrite triggered by a late panel change.
3. **A display is available now.** Physical prototype hardware exists in the
   form of a linked external display, which gets real UI onto real glass
   immediately.

## Decision

**The display target is deliberately dynamic.** PlatypusOS treats display
geometry as a *runtime property discovered from `IDisplay::info()`*, never as a
compile-time constant, and prototypes against a **linked external display**
until the production panel is selected.

Concretely:

1. **No resolution is baked into the software.** The renderer already sizes its
   framebuffer from `hal::DisplayInfo`; the host simulator now does the same
   instead of hardcoding 320×240 (see [Consequences](#consequences)). Apps lay
   out against `ctx.renderer.displayInfo()`, never against literals.
2. **The prototype display is an `IDisplay` implementation, not a new UI
   stack.** The external panel is reached through a transport-independent
   presentation link specified in
   [docs/protocols/presentation.md](../protocols/presentation.md). Everything
   above the HAL — launcher, ShadowScan, viewer, measurement — is unaware that
   the panel is remote. This is the enforcement mechanism for the constraint
   the Tab5 plan already stated: *the UI must not become Tab5-specific.*
3. **Prototype hardware candidates**, either of which the link must support:
   - **M5Stack Tab5** — already owned; first bring-up target.
   - **A 4.3" 800×480 RGB capacitive panel on an Espressif carrier.** The
     owner refers to this as the "ESP32-S3-Korvo-1" 4.3" LCD; the board
     documented in this repository with that panel is the
     **ESP32-S3-LCD-EV-Board-SUB3 v1.3** (ST7262E43 + GT1151). The exact
     carrier identity is an open item below — the *panel* characteristics are
     what the software targets, and those agree across both names.
4. **800×480 is the UI design reference**, not a commitment. Layouts are
   authored so they degrade to smaller panels and scale to larger ones; 800×480
   is what gets drawn in mockups and what performance budgets assume.
5. **The BOM carries the display as `TBD-LINKED-PROTOTYPE`**, with the 3.2"
   SPI and 4.3" RGB options both retained rather than one silently deleted.

## Consequences

**Easier**

- Panel selection can wait for enclosure and sourcing reality without blocking
  a single line of UI code.
- The contest hardware application proceeds without a frozen display line.
- Any panel swap becomes a driver change plus a layout review, not a rewrite.

**Harder / newly required**

- `renderer/dirty-rects` (ROADMAP M2) is promoted from optimization to
  **requirement**. A full 800×480 RGB565 frame is 768 000 bytes; a USB CDC link
  realistically sustains ~0.5–1 MB/s, i.e. about **one full frame per second**.
  Only partial updates make a linked display usable. The bandwidth analysis
  lives in [presentation.md](../protocols/presentation.md).
- Apps may no longer assume a fixed row height or a screen that fits six
  items. Layout code must be written against `displayInfo()` from the start —
  cheap now, expensive to retrofit across six apps later.
- Two display drivers must be maintained during the prototype phase (linked
  display + eventual integrated panel).
- Touch coordinates arrive in panel space; calibration and any rotation belong
  in the driver, not in apps.

**Explicitly not decided here**

- The production panel (size, interface, vendor, part number).
- Whether the final device drives its panel from the Linux MPU or the STM32.
- The `handedness` / 180° present rotation feature
  ([INDUSTRIAL_DESIGN.md](../hardware/INDUSTRIAL_DESIGN.md)) — still ROADMAP M2.
- Whether the rotary encoder becomes a first-class HAL input event. The
  presentation link reserves an encoder message; `hal::IDisplay` currently
  exposes only touch and buttons. Tracked as an open item.

## Alternatives considered

| Option | Why not (yet) |
|---|---|
| Freeze 3.2" ILI9341 320×240 (BOM rev A) | Cheap and matches the current code, but 320×240 is below what the launcher, viewer, and measurement UIs need, and the industrial-design sheets all draw a large screen. Freezing it would guarantee a rewrite. |
| Freeze 4.3" 800×480 now | Plausible end state, but the SUB3 evaluation lists unresolved mechanical unknowns (standoff thread, active-area dimensions) and no production-panel sources yet. Its own decision gate has not been passed. |
| Freeze a 5–7" panel per the concept sheets | Drives battery, enclosure, and cost decisions that are not ready, and no candidate part has been evaluated. |
| Ship semantic UI messages to a smart client (client renders the UI) | Bandwidth-cheap, and the Tab5 plan sketches such a message set — but it relocates UI logic into the client, which is exactly how the UI becomes device-specific. Retained only as an optional extension for the 3D viewer, where client-side rendering earns its keep. |
| Keep prototyping headless on the host simulator only | Zero hardware learning. The point of the linked display is to surface real latency, touch, and legibility problems now, while there is still time to act on them. |

## Open items

- [ ] Confirm the exact Espressif carrier: ESP32-S3-Korvo family vs
      ESP32-S3-LCD-EV-Board-SUB3 v1.3. Record the answer in
      [ESP32-S3-LCD-EV-BOARD-SUB3_V1.3.md](../hardware/ESP32-S3-LCD-EV-BOARD-SUB3_V1.3.md).
- [ ] Measure achievable link throughput on real hardware and record it in
      [presentation.md](../protocols/presentation.md) (bandwidth table).
- [ ] Decide whether the encoder becomes a `hal` input event or stays an
      MCU-bridge topic.
- [ ] Set the production-panel decision gate date once the Sep 18 hardware
      outcome is known.
