# Platypus One — Bill of Materials (rev B, planning — merged)

Status date: **2026-08-24**. Merges the original planning BOM with the
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
| 4a | **4.3–5" MIPI-DSI 800×480 capacitive** ✅ SELECTED | $30–45 | ~120×75×5 (4.3") / 165×100×8 (5") | Product path — native Linux DSI, USB-C stays free for charging. Exact panel: TARS research due, gate Sep 30 | PLANNED |
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
| 7a | Camera: USB UVC OV5640-class autofocus ✦ | 1 | $25 | 25×25×10 | Amazon | Schedule baseline. Must prove repeatable 10–30 cm focus, calibration, and exposure/focus lock or reproducible state reporting over V4L2 | PLANNED |
| 7b | Camera: 13 MP autofocus module | 1 | $40 | 25×25×10 | TBD | Select only if UNO Q path plus the same calibration/control requirements as 7a are verified; resolution alone is not sufficient | PLANNED |
| 8a | ToF: VL53L1X (single-zone, 4 m) | 1 | $12 | 13×18×2 (breakout) | Pololu/Adafruit | Schedule fallback: valid scale/range evidence, but no coarse depth map | PLANNED |
| 8b | ToF: VL53L8CX (8×8 multizone) ✦ | 1 | $20 | 6×6×3 | ST/Sparkfun | Geometry-evidence preference: coarse depth sanity map plus range metadata; retain 8a if driver/bring-up threatens schedule | PLANNED |
| 9a | IMU: BNO055 (fused orientation on-chip) ✦ | 1 | $25 | 20×27×4 (breakout) | Adafruit | No sensor-fusion code needed — fastest to a working level/angle app | PLANNED |
| 9b | IMU: BMI270 | 1 | $8 | 10×10×3 | DigiKey | Cheaper/smaller; fusion runs on our side | PLANNED |
| 10 | Color: TCS34725 | 1 | $8 | 20×20×3 | Adafruit | Multi-measure lineage feature | PLANNED |
| 11 | Radar: Grove BGT24LTR11 Doppler | 1 | $30 | 40×20×12 | Seeed | **V2 candidate** (machine-health app per utilities roadmap) — envelope reserved, not in Rev A build | DEFERRED |
| 12 | White LED high-CRI + independent drivers | 2 | $4 | — | DigiKey | Fixed, documented geometry relative to camera; capture manifest records which source was active | PLANNED |

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
| 22 | Printed fiducial/checkerboard scale target | 1 set | ~$5 | Local print + rigid backing | Required evidence accessory; at least one known dimension in capture plane | PLANNED |
| 23 | 1/4-20 threaded insert + mounting hardware | 1 | ~$3 | McMaster/Amazon | Stable three-view capture and repeatability; integrate with kickstand/tripod feature | PLANNED |
| 24 | Rigid camera/ToF/illumination datum bracket | 1 | included in enclosure/carrier | Fusion/PCBWay | Calibration only remains valid if relative sensor geometry survives handling/service | PLANNED |

## Mesh2CAD handoff capability gate

Research traceability and the cross-repository boundary are documented in
[MESH2CAD_HANDOFF_AND_BOM.md](../MESH2CAD_HANDOFF_AND_BOM.md).

The PlatypusOne BOM is selected to create a calibrated engineering-evidence
bundle, not to perform full CAD reconstruction on the handheld. Rev A therefore:

- requires repeatable camera calibration and capture controls;
- treats ToF and IMU values as timestamped provenance for deliberate views;
- requires controlled illumination, a scale target and a rigid sensor datum;
- preserves raw observations separately from derived silhouettes/meshes;
- exports the bundle for Mesh2CAD optimization and CAD-kernel validation; and
- adds no dedicated reconstruction GPU/NPU.

Before camera, ToF or enclosure freeze, the candidate must pass the evidence
quality gates in the handoff document. A component being electrically
compatible is not sufficient.

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

1. **Display option (4a/4b/4c)** — see DISPLAY_COMPARISON.md. Blocks battery
   sizing, enclosure, renderer target.
2. Camera 7a vs 7b — blocked on UNO Q path plus the calibration, focus/exposure
   and repeatability gates in MESH2CAD_HANDOFF_AND_BOM.md.
3. ToF 8a vs 8b — 8b is now the geometry-evidence preference; retain 8a if
   VL53L8CX driver/bring-up threatens schedule. Both remain carrier-compatible.
4. IMU 9a vs 9b — BNO055 remains schedule-first because guided multi-view
   capture needs orientation metadata without adding sensor-fusion work.
5. Exact placement of camera, ToF and both illumination sources — must be
   dimensioned as one rigid calibration datum before enclosure freeze.
