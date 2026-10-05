# Perception Head Rev A — mechanical constraints and Fusion inputs

The board outline is **not** invented here. This page states the constraints
the head imposes and exactly what Fusion must hand back before layout.
Companion: [REV_A_SM1 mechanical requirements (PR #43)](https://github.com/Matthewjg95/PlatypusOne/pull/43)
— its R4 "sensor datum bracket" is the parent of this head.

Tags: **VENDOR** (manufacturer drawing), **PROVISIONAL** (study value),
**MEASURE** (calipers on the real part), **FUSION** (Matthew decides in CAD).

## 1. Datum concept

- The **head PCB is the rigid camera↔ToF datum**: the camera module is
  screwed to the head on standoffs (H4–H7); the Pololu ToF carrier is soldered
  on headers and screwed (2× M2). The camera↔ToF extrinsic is a property of the
  head assembly and is stored in its EEPROM.
- The head locates on the chassis' datum bracket by **H1 (round, locating) +
  H2 (slot, anti-rotation) + H3 (clamp only)**. Fasteners clamp, pins locate.
  The rear service cover never touches the head (SM1 R4.3).
- Alternative still open (M4): a machined bracket carries camera and ToF, and
  the head carries only electronics. Decide with the Fusion study + C3.

## 2. Component geometry the head must respect

| Item | Value | Tag |
|---|---|---|
| Pololu #3419 board | 12.7 × 22.9 × 1.02 mm; sensor top 1.8 mm above board | VENDOR (Pololu drawing 2024-02-14) |
| Pololu holes | 2× Ø2.18 mm, 2.5 mm from the 1×4-row edge and 2.5 mm from top/bottom | VENDOR |
| ToF optical centre on carrier | ≈6.3 mm from 1×9-row edge, ≈11.4 mm from bottom edge (top view) | VENDOR callout — **confirm with Pololu STEP** |
| ToF FoV (detection) | 45° × 45° (65° diag) | VENDOR (ST DS) |
| ToF **exclusion cone** (no obstruction / window frame inside) | 57.9° × 57.9° (86.6° diag) | VENDOR (ST DS §2.2) |
| Carrier mounting height above head | header spacer height (2.5 mm class) | **FUSION/MEASURE** |
| Camera board B0394 | ~24 × 25 mm; lens stack UNRESOLVED | VENDOR (Arducam) / MEASURE |
| Camera board B0390 | ~25 × 24 mm, fixed focus, 62.2° H | VENDOR / MEASURE |
| Camera board B0393 | autofocus (VCM) module, dims UNRESOLVED | MEASURE |
| Camera hole pattern (H4–H7) | Pi-camera-style 21 × 12.5 mm | **PROVISIONAL — MEASURE each module** |
| Camera FOV cones | B0394 75° H × 60° V; B0390 62.2° H | VENDOR |
| BMI270 | 2.5 × 3.0 × 0.8 mm | VENDOR |
| J1 JST GH 14 horizontal | ≈21 mm wide incl. mounting pads; cable exits board edge; latch access | VENDOR (KiCad footprint) |
| Head PCB thickness | 1.6 mm FR-4 | PROVISIONAL (M5) |

## 3. Placement requirements

| # | Requirement | Reason |
|---|---|---|
| P1 | Camera and ToF optical axes parallel, both normal to the head PCB, on the **same horizontal line** | Parallax becomes 1-D along a known baseline |
| P2 | Camera↔ToF optical-centre baseline **as small as the two bodies and the ToF exclusion cone allow**; PROVISIONAL target 18–30 mm | Minimises parallax/occlusion at 100–500 mm working distance while keeping the camera lens out of the ToF exclusion cone |
| P3 | ToF exclusion cone and camera FOV cone are modelled solids; nothing (window frame, bezel, LED, screw head) intersects them | ST requirement + image quality |
| P4 | IMU within ~20 mm of the camera optical centre, on a stiff region, ≥10 mm from LDO U1, BJTs Q1/Q2 and mounting holes; axes aligned to camera axes | Thermal drift, stress, meaningful orientation |
| P5 | Light boards: two, symmetric about the camera axis, PROVISIONAL ±25–35 mm off-axis, LEDs recessed/baffled so no direct path into the camera lens or the ToF receiver | Glare/flare control (L3) |
| P6 | Q1/Q2/U1/U3 (heat) on the side away from the IMU, with copper pour to the chassis contact if possible | Thermal |
| P7 | J1 on the rear edge, cable exiting away from the optical face; ≥5 mm finger/tool clearance for the latch | Service |
| P8 | All test points on the rear face, reachable with the head mounted and the rear cover off | Bring-up without disassembly |
| P9 | Camera FFC exit and bend toward the Media Carrier respects FFC minimum bend radius (UNRESOLVED) | Reliability |
| P10 | Pin-1 marks on J1–J4 and board revision text "PERCEPTION HEAD REV A" visible when assembled | Service, traceability |

## 4. What Fusion must supply before layout freeze

1. **Head outline envelope** (max width × height, corner radii) as DXF — PROVISIONAL bound until then: ≤60 × 35 mm (fits the 75–85 mm wide head band of SM1).
2. **Datum-hole positions** H1/H2/H3 relative to the camera optical centre, and the bracket's locating features.
3. **Camera optical centre position** and module orientation (FFC exit direction) for the selected module (after #40).
4. **ToF optical centre position** (sets the baseline, P2) and carrier stand-off height.
5. **Keep-out solids**: camera FOV cone, ToF exclusion cone, window/bezel, light-board volumes, screw heads, FFC bend volume.
6. **Connector exit directions** and cable route lengths to the host (H7) incl. minimum bend radii.
7. **Max component height** on front and rear faces (window clearance in front, cover clearance behind).
8. **Board thickness/stiffness decision** (M5) and any chassis thermal contact pad location.

Return format: Fusion sketch → DXF for the outline/holes, STEP for keep-outs;
KiCad will export a board STEP back for fit-check before fabrication.

## 5. Measurements only Matthew can take (from the camera bench-off)

Per module (B0394, B0393, B0390): board W × H × thickness, hole centres and
diameter, lens/holder height above board, lens diameter, optical centre offset
from board centre (photo over a grid), FFC exit side and connector type.
Record in [CAMERA_EVIDENCE_INTAKE.md](CAMERA_EVIDENCE_INTAKE.md).
