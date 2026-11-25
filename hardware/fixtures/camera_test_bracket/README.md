# IMX219 camera test bracket

Bench fixture for the #40 camera bench-off (B0394 / B0393 / B0390). A tilting
camera plate in a yoke stand: 0° looks level, 90° looks straight down. The base
has a captive 1/4"-20 nut, so it also mounts on a tripod or articulating arm
for the 150–600 mm study distances.

| File | What |
|---|---|
| `camera_test_bracket.scad` | Parametric source (all dims at the top) |
| `plate.stl` | Camera plate (print flat, standoffs up) |
| `yoke.stl` | Stand (print base-down) |
| `preview_*.png` | Renders |

**Hole pattern is PROVISIONAL**: Pi-camera style 21 × 12.5 mm, top row 2 mm
from the board's top edge, 25 × 24 mm board. No module has been measured
([MECHANICAL_FUSION_INPUTS.md §5](../../pcb/MECHANICAL_FUSION_INPUTS.md)).
Caliper each module before printing and edit `hole_dx`, `hole_dy`, `hole_top`;
`standoff_h` (5 mm) must clear the back-side FFC connector. Not printed or
fit-checked yet.

## Print

PLA or PETG, 0.2 mm layers, 3 perimeters, 20 % infill, no supports.

## Hardware

- 4× M2 × 6–8 mm screws (self-tap into the 1.75 mm pilots; don't overtighten)
- 2× M3 × 12 mm screws, 2× M3 hex nuts (drop into the cheek slots), 2× M3 washers
- 1× 1/4"-20 hex nut, press into the base (tripod use only)

The M3 screws pass through the yoke arms into the captive nuts. Tighten until
the plate holds its angle (friction lock). Set the angle with a phone
inclinometer on the plate back. "TOP" on the back marks the edge away from the
FFC, which exits through the bottom notch.

## Bench-off use

- The four screws locate the board. For calibration set B, back them out, lift
  the board, reseat it and record a new `mount_id` (capture matrix rule 7).
- Don't touch the tilt joint inside a dataset. Re-zero the distance whenever it
  moves.
