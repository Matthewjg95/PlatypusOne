# ADR-0002 — MIPI-DSI panel is the production display path

- **Status:** Accepted
- **Date:** 2026-08-24
- **Deciders:** Matthew (owner)

## Context

[DISPLAY_COMPARISON.md](../hardware/DISPLAY_COMPARISON.md) laid out four
options. The UNO Q routes the QRB2210's MIPI-DSI through an on-board ANX7625
to USB-C DP Alt-Mode, multiplexed with a MIPI-DSI connector — so a DSI panel
is the only path that is simultaneously product-grade (800×480-class,
capacitive) and handheld-viable (USB-C stays free for charging and host
duties). The owner has reviewed the comparison and decided.

This decision **refines, not supersedes, ADR-0001**: software still discovers
geometry at runtime and nothing compiles in a resolution. ADR-0002 fixes only
which *physical* display the production enclosure and carrier PCB are
designed around.

## Decision

Platypus One's production display is a **4.3–5″ MIPI-DSI panel, 800×480-class,
capacitive touch**, driven natively by the Linux side through the UNO Q's DSI
connector. The logical reference UI target is 800×480 RGB565 + capacitive
touch, per the SUB3 evaluation.

## Consequences

- Enclosure front face, depth, and carrier PCB can proceed against a 4.3–5″
  DSI envelope; exact aperture waits on the selected panel's active-area spec.
- `display/driver` roadmap work becomes: DSI panel bring-up on the Debian
  image (device tree / panel driver), not SPI.
- USB-C DP Alt-Mode remains the dev/prototyping fixture (zero-driver monitor
  path); the presentation-link / ESP32-S3 bridge architecture is demoted to
  **fallback**, exercised only if panel sourcing fails (decision gate below).
- The 3.2″ SPI option stays in the BOM solely as the descope safety net.
- **Blocking follow-up (owner: TARS): compatible-panel research** —
  1. identify the UNO Q DSI connector part number and pinout (Arduino docs /
     schematic / ABX00162 resources);
  2. find **≥2 purchasable** 4.3–5″ 800×480-class DSI capacitive panels with
     plausible Linux support (existing panel driver or simple-panel timing),
     with price, lead time, FPC/connector details, and active-area drawings;
  3. confirm how the Debian image applies device-tree overlays;
  4. record findings in `docs/hardware/DSI_PANEL_CANDIDATES.md`.
- **Decision gate:** if no viable panel is identified by **Sep 30, 2026**,
  fall back to the ESP32-S3 bridge (Option C) so the Nov 10 carrier-PCB
  freeze holds.

## Alternatives considered

| Option | Why not |
|---|---|
| 4.3″ parallel RGB + ESP32-S3 bridge (C) | Second MCU, protocol, and toolchain before Dec 20; kept as fallback only |
| 3.2″ SPI ILI9341 (A) | Product-grade UX impossible at 320×240 resistive; kept as descope net |
| HDMI/USB-C panel (D) | Consumes the only USB-C port; dev fixture only |
