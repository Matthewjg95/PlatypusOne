# IMX219 slide-in camera test block

Bench fixture for the #40 camera bench-off. One printed block, no base, no
screws: the camera board drops into edge grooves from the top, with the lens
looking out the front window. Lift it out to swap in B0394 / B0393 / B0390.

| File | What |
|---|---|
| `camera_test_block.scad` | Parametric source (all dims at the top) |
| `camera_test_block.stl` | Print file |
| `preview_*.png` | Renders |

Block is 33.6 W × 32 D × 27 H mm. The board's bottom edge sits 8 mm up, and
5 mm of the board stays above the block for gripping.

**Board size is the dev nominal, not measured:** 25 × 24 × 1.0 mm, from the
B0394 vendor drawing (the same nominal as the bench perception plate), with the
FFC on a 25 mm edge. Caliper one board before printing. Edit `board_w`,
`board_h`, and `slot_t` (groove width, 1.5 mm) if the fit is off.

## Print

PLA or PETG, 0.2 mm layers, 3 walls, 20 % infill. Print as modelled, flat
bottom down; no supports (the ribbon tunnel is an 18 mm bridge).

## Use

- Board goes in component/lens side **facing the front window**, FFC edge down.
  The 1.5 mm groove lips only touch the outer board margin. The window and the
  rear cavity clear the lens, M12 holder or VCM, and the back-side connector.
- The ribbon drops through the floor and runs out the back through the tunnel
  under the block.
- Too loose: a strip of tape on the board edge, or reprint with `slot_t = 1.4`.
  Too tight: don't force it. Reprint at 1.6.
- Lifting the board out and dropping it back in counts as a real remount for
  calibration set B (new `mount_id`, capture matrix rule 7). The groove play is
  part of what that set measures.
- Not printed or fit-checked yet.
