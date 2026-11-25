# Lid for EnclosurePrototypeTall

Press-fit lid for the 116.1 × 190 × 33 mm L-shaped prototype shell (Fusion
export, not in the repo). Every shell dimension in the `.scad` header was
measured from that STL. A boolean check against it gives zero overlap.

The lid does three things:

- **Locating lip**: 1.6 mm wide, 4 mm deep, 0.3 mm clearance to the inner
  walls. It stops at z 29, clear of the display tray (z 27).
- **Board clamps**: one grooved rib over each floor slot (y 145–185). The
  groove ceiling sits 0.1 mm below the board's top edge, so it presses lightly.
  - Camera slot (x 111.6, by the wall window): board 24 mm tall, top at z 26.
  - ToF slot (x 95.45): Pololu board 22.9 mm tall, top at z 24.9.
- **Display window**: 61.1 × 104.3 mm, bevelled on the outside. A frame
  presses a 2.5 mm border of the glass onto the tray ledge at z 21.2.

## Check before printing

- **Panel thickness** (`panel_t`, ledge to glass top): 7.9 mm assumed, from the
  4-DSI-TOUCH-A drawing. The lid only clamps the display if this is right.
- **Board heights** (`cam`, `tof`): which board goes in which slot, and its
  standing height. If you swapped them, swap the two entries.
- **Window size**: if the active area is off-centre in the glass, adjust
  `border`/`pocket`.
- The grooves clamp the boards top and bottom but not along the slot. They can
  still slide in y.

## Print

The STL is already flipped: lid top on the bed, so the L footprint looks
mirrored. PLA/PETG, 0.2 mm, 3 walls, no supports.

`openscad -D 'part="assembly"' -D 'shell="path/to/EnclosurePrototypeTall.stl"'`
shows it seated on the shell.
