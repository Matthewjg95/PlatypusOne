# Platypus One — Bill of Materials (rev B, planning — merged)

Status date: **2026-08-25**. Merges the original planning BOM with the
envelope BOM from `industrial_design/bounding_boxes/BOM_v0_1.md` (which stays
as the packaging-envelope tracker). **Alternates are kept deliberately** —
selection analysis is pending; do not delete an option without a decision
note. Prices are indicative USD; refresh at order time.

Legend: ACQ = PLANNED → ORDERED → RECEIVED → TESTED. ✦ = primary candidate.

## Core compute

| # | Item | Qty | Est. | Envelope (mm) | Source | Notes | ACQ |
|---|---|---|---|---|---|---|---|
| 1 | **Arduino UNO Q (4G)** ✦ | 1 | $59 (or free) | 85×54×20 | Contest hw / store.arduino.cc | Required by contest. Hardware recipients announced Sep 18 | PLANNED |
| 2 | microSD 32 GB A1 | 1 | $12 | — | Amazon/DigiKey | OS + data partition | PLANNED |
| 3 | USB-C PD supply 5V/3A + cable | 1 | $15 | — | Amazon | Bench power | PLANNED |

## Display — **DECIDED: 4a (DSI)** per [ADR-0002](../adr/0002-dsi-production-display.md); analysis in [DISPLAY_COMPARISON.md](DISPLAY_COMPARISON.md)

| # | Option | Est. | Envelope (mm) | Notes | ACQ |
|---|---|---|---|---|---|
| 4a | **Waveshare 5-DSI-TOUCH-A + UNO Media Carrier** ◐ CONDITIONAL | Panel $34.95–36.99 + carrier €19.89 | Panel CAD TBD; carrier 68.58×53.34 | Only 4.3–5″ panel with a shipped UNO Q profile; 720×1280 native portrait. Carrier was Coming Soon / sold out on 2026-08-25. Proof plan: [DSI_PANEL_CANDIDATES.md](DSI_PANEL_CANDIDATES.md) | PLANNED |
| 4b | 4.3" parallel-RGB 800×480 cap. (SUB3-class, ST7262E43 + GT1151) + ESP32-S3 bridge | ~$35+bridge | ~121×76×7 | FALLBACK ONLY (activated if 4a sourcing fails by Sep 30) | HOLD |
| 4c | 3.2" ILI9341 320×240 SPI, resistive (XPT2046) | $16 | ~90×55×10 | DESCOPE NET only; matches current renderer/sim as built | HOLD |
| 4d | USB-C/HDMI portable monitor or Waveshare 5" HDMI (H) | $40 | dev only | Prototyping fixture, never the product (consumes USB-C) | PLANNED |

## Input

| # | Item | Qty | Est. | Envelope (mm) | Source | Notes | ACQ |
|---|---|---|---|---|---|---|---|
| 5 | Rotary encoder, EC11-style w/ push | 1 | $3 | 24×24×30 | DigiKey | Primary nav control, at left thumb per ID doc | PLANNED |
| 6 | Tactile buttons 6×6 mm | 4 | $2 | — | DigiKey | Trigger + function keys | PLANNED |

## Sensing (alternates kept — analysis pending)

| # | Item | Qty | Est. | Envelope (mm) | Source | Notes | ACQ |
|---|---|---|---|---|---|---|---|
| 7a | Camera: USB UVC OV5640-class autofocus ✦ | 1 | $25 | 25×25×10 | Amazon | Simplest Linux path (V4L2) | PLANNED |
| 7b | Camera: 13 MP autofocus module | 1 | $40 | 25×25×10 | TBD | Higher fidelity for ShadowScan; confirm UVC or MIPI-CSI availability on UNO Q before choosing | PLANNED |
| 8a | ToF: VL53L1X (single-zone, 4 m) ✦ | 1 | $12 | 13×18×2 (breakout) | Pololu/Adafruit | Proven, cheap, fine for distance app | PLANNED |
| 8b | ToF: VL53L8CX (8×8 multizone) | 1 | $20 | 6×6×3 | ST/Sparkfun | Coarse depth grid — assists ShadowScan; more driver work | PLANNED |
| 9a | IMU: BNO055 (fused orientation on-chip) ✦ | 1 | $25 | 20×27×4 (breakout) | Adafruit | No sensor-fusion code needed — fastest to a working level/angle app | PLANNED |
| 9b | IMU: BMI270 | 1 | $8 | 10×10×3 | DigiKey | Cheaper/smaller; fusion runs on our side | PLANNED |
| 10 | Color: TCS34725 | 1 | $8 | 20×20×3 | Adafruit | Multi-measure lineage feature | PLANNED |
| 11 | Radar: Grove BGT24LTR11 Doppler | 1 | $30 | 40×20×12 | Seeed | **V2 candidate** (machine-health app per utilities roadmap) — envelope reserved, not in Rev A build | DEFERRED |
| 12 | White LED high-CRI + driver | 2 | $2 | — | DigiKey | Controlled shadow casting + illumination | PLANNED |

## Power

| # | Item | Qty | Est. | Envelope (mm) | Source | Notes | ACQ |
|---|---|---|---|---|---|---|---|
| 13 | Flat Li-ion/LiPo 3.7 V ~2500 mAh protected ✦ | 1 | $15 | 100×60×8 | Adafruit | Flat pack per packaging direction; exact cell TBD after display decision (display dominates power budget) | PLANNED |
| 14 | USB-C charge/boost board 5 V 3 A | 1 | $20 | ~30×20×6 | Adafruit/DigiKey | Stable 5 V under Linux load; port exits grip base | PLANNED |
| 15 | Power switch SPDT slide | 1 | $1 | — | DigiKey | | PLANNED |

## Audio & feedback

| # | Item | Qty | Est. | Source | Notes | ACQ |
|---|---|---|---|---|---|---|
| 16 | Piezo buzzer or 1 W speaker + PAM8302 | 1 | $6 | Adafruit | IAudioOutput | PLANNED |
| 17 | Vibration motor disc (optional) | 1 | $2 | Adafruit | Haptics stretch | PLANNED |

## Enclosure & interconnect (PCBWay scope — $300 credit if selected)

| # | Item | Qty | Est. | Source | Notes | ACQ |
|---|---|---|---|---|---|---|
| 18 | Carrier/breakout PCB (I²C hub, buttons, encoder, power) | 1 | ~$60 asm | **PCBWay** | Fusion Electronics design; freeze gate Nov 10 | PLANNED |
| 19 | Enclosure: CNC alu barrel + printed polymer grip (hybrid per [INDUSTRIAL_DESIGN.md](INDUSTRIAL_DESIGN.md)) | 1 set | ~$120–250 | **PCBWay** | Metal barrel likely $150–250 alone — quote early; RF window required | PLANNED |
| 20 | JST-SH/Qwiic cables, headers, M2/M2.5 standoffs kit | — | $15 | Adafruit/Amazon | | PLANNED |
| 21 | Hookup wire, heat-shrink | — | $10 | Amazon | | PLANNED |

## Tools (if not owned)

| # | Item | Est. | ACQ |
|---|---|---|---|
| T1 | Soldering iron + solder + flux | $40 | PLANNED |
| T2 | Multimeter | $25 | PLANNED |
| T3 | USB-UART 3.3 V adapter | $10 | PLANNED |
| T4 | Digital calipers | $20 | PLANNED |

## Software (rubric: tools count)

| Item | Cost | Notes |
|---|---|---|
| Autodesk Fusion (+ Electronics) | Free personal / contest license | Enclosure + carrier PCB + schematics |
| PlatypusOS (this repo) | — | C++20/CMake, own work |
| Arduino App Lab / UNO Q image | Free | Board OS |

## Totals (primary candidates only, excl. tools & contest-provided)

- With contest hardware + $300 PCBWay credit: **≈ $200–230 out of pocket**
- Fully self-funded worst case: **≈ $450–500 + tools**

## Pending decisions blocking ordering

1. **Display proof (4a)** — architecture is decided, but final selection awaits
   the UNO Media Carrier + 5-DSI-TOUCH-A bench test in
   [DSI_PANEL_CANDIDATES.md](DSI_PANEL_CANDIDATES.md). Blocks battery sizing
   and enclosure freeze; renderer geometry remains runtime-dynamic.
2. Camera 7a vs 7b — blocked on confirming UNO Q camera input paths.
3. ToF 8a vs 8b and IMU 9a vs 9b — Matthew's analysis pending; both pairs are
   I²C and carrier-PCB-compatible either way, so these do NOT block the PCB
   schematic start, only the final placement.
