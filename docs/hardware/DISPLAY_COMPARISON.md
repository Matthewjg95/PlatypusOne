# Platypus One — Display Decision Comparison

Status date: **2026-08-24**. Decision owner: Matthew. This consolidates the
three display directions currently in the repo (3.2" SPI in
[BOM.md](BOM.md), 4.3" 800×480 in the
[SUB3 eval](ESP32-S3-LCD-EV-BOARD-SUB3_V1.3.md), 5–7" in the
[envelope BOM](../../industrial_design/bounding_boxes/BOM_v0_1.md)) into one
decision with explicit options.

## The fact that changes the picture

The UNO Q routes the QRB2210's MIPI-DSI output through an on-board **ANX7625**
to **USB-C DisplayPort Alt-Mode**, and also exposes a **MIPI-DSI connector**;
the two are **multiplexed — only one display output active at a time**
(Arduino ABX00162 datasheet / UNO Q forum). Consequences:

- **Dev/prototyping**: any USB-C (or DP/HDMI via adapter) monitor works with
  zero driver work — instant code→screen pipeline on the Linux side.
- **Product**: a DSI panel on the internal connector leaves USB-C free for
  charging and USB host (camera) — exactly what a handheld needs. Using USB-C
  video in the product would consume the only charging port: not viable.

## Decision 1 — product reference display target

| | A. 3.2" SPI (current BOM) | B. 4.3–5" MIPI-DSI panel | C. 4.3" RGB + ESP32-S3 bridge | D. 5" HDMI/USB-C panel |
|---|---|---|---|---|
| Panel | ILI9341 320×240, resistive (XPT2046) | 800×480-class, capacitive | SUB3-class 800×480 RGB, capacitive (GT1151) | Waveshare 5" HDMI (H), 800×480, cap. touch over USB |
| Drive path | SPI from MCU or Linux spidev | **Native DSI from Linux (DRM)** | ESP32-S3 renders; UNO Q sends UI protocol | USB-C DP alt-mode |
| Driver effort | Low (proven, matches current renderer/sim) | **Medium-high**: device-tree + panel driver on the Debian image; panel sourcing for the UNO Q connector is the open risk | High: display co-processor firmware + versioned UI protocol + second toolchain | None |
| UX quality | Weak: small, low-res, resistive | **Product-grade** | Product-grade panel, but UI latency bounded by link | Product-grade |
| Handheld fit | Excellent (tiny, low power) | Good — panel only, thin FPC | Poor-to-OK: adds a second board + power | **Not integrable** (bulky, consumes USB-C) |
| Power | ~0.1–0.3 W | ~1–1.5 W w/ backlight | panel + ESP32-S3 (~+0.5 W) | ~1.5 W + kills charging port |
| BOM cost | ~$16 | ~$30–45 | ~$35 + bridge board | ~$40 |
| Renderer impact | None (built for it) | Bump target to 800×480; Renderer already reads DisplayInfo, so mostly sim-window + font work | New "remote display" IDisplay transport | None (dev only) |
| Contest optics | Looks like a toy next to 2025's multi-measure | **Best: real product feel** | Interesting engineering story, more failure modes | n/a |
| Risk | Locks in weak UX | Panel/driver sourcing unknown → prototype fallback exists (dev monitor) | Most moving parts before Dec 20 | n/a |

**Recommendation:**
- Adopt **800×480 capacitive as the logical reference UI target** (agreeing
  with the SUB3 eval), sized 4.3–5".
- **Primary physical path: Option B (MIPI-DSI panel)** — the only path that is
  both product-grade and handheld-viable while keeping USB-C for charging.
- **Fallback: Option C (ESP32-S3 bridge)** if no DSI panel compatible with the
  UNO Q connector can be sourced in September.
- **Descope safety net: Option A stays in the BOM** — it is the guaranteed-
  to-ship path if both B and C stall by the Nov 24 hardware freeze.
- Option D is a **dev fixture only** (see Decision 2), never the product.

## Decision 2 — prototyping fixture (code→screen pipeline)

| Fixture | What it exercises | Effort to first pixel | Verdict |
|---|---|---|---|
| **USB-C/HDMI monitor** (any portable monitor or the Waveshare 5") | Real PlatypusOS renderer + full Linux stack at 800×480 | **Zero** — plug in | **Do this first.** Fastest possible pipeline |
| Espressif LCD-EV-Board + SUB3 4.3" (the "Korvo-class" board) | The Option-C bridge architecture; panel identical to a B/C candidate | Days (BSP exists, but it's a rendering *client*, not a UNO Q display) | Use **only if** Option C becomes the path |
| M5Stack Tab5 | Same client architecture, non-candidate panel | Days | Superseded — same lesson as above with less panel relevance |

Note on naming: the board TARS evaluated is the **ESP32-S3-LCD-EV-Board-SUB3**
(4.3", 800×480, GT1151 touch). The Korvo-2 is the related Espressif audio dev
board (2.4" LCD option). Both would play the same role: an ESP32-S3 rendering
client — which is Option C's architecture, not a direct UNO Q display. If the
UNO Q's native video path works as documented, **neither is needed to start**:
a plain monitor exercises more of the real product stack sooner.

## What this unblocks once decided

1. Renderer/sim: retarget Win32 sim window + launcher metrics to 800×480.
2. Enclosure: front-face aperture and depth around a 4.3" vs 5" panel.
3. BOM: collapse display alternates to one primary + one fallback.
4. Battery sizing: display is the dominant consumer.

## Open items (owner: TARS on new machine, unless claimed)

- [ ] Identify the UNO Q DSI connector part/pinout and ≥2 candidate DSI
      panels with Linux driver support (check Arduino docs + schematic)
- [ ] Confirm Debian image supports panel/device-tree overlays without
      rebuilding the kernel
- [ ] Bench: USB-C DP monitor + PlatypusOS launcher at 800×480 (first pixel
      the day the UNO Q arrives)
- [ ] Price/lead-time check on 4.3" vs 5" DSI capacitive panels
