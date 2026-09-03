"""Headless STEP export of the Platypus One beef-up assembly (build123d).

Produces the same portrait side-grip layout as PlatypusOneBeefUp.py, plus an
approximate ENCLOSURE_REF reconstruction of the existing shell (165.1 x 96.5
x 27.94 mm tray with the display window), so the assembly can be reviewed or
cloud-converted to a Fusion design without running Fusion.

The authoritative path is still running PlatypusOneBeefUp.py inside Fusion
against the real enclosure; this export is for review while away from it.

Usage:  python3 generate_step.py [out.step]
Coordinates (mm): x = across the width (+x = grip edge), y = long axis
(+y = sensor band end), z = display normal (+z = front face).
"""

import sys

from build123d import (Box, Compound, Cylinder, Pos, Rot, export_step)

MM = 1.0  # build123d works in mm natively

# Device envelope
L, W, H = 165.1, 96.52, 27.94
WALL = 2.5


def at(l, s, u, shape):
    """Place a shape by device-frame coords (long, side, up) -> (y, x, z)."""
    return Pos(s, l, u) * shape


def cyl_l(d, length):
    """Cylinder along the long axis (y)."""
    return Rot(90, 0, 0) * Cylinder(d / 2.0, length)


def cyl_s(d, length):
    """Cylinder along the side axis (x)."""
    return Rot(0, 90, 0) * Cylinder(d / 2.0, length)


def cyl_u(d, length):
    """Cylinder along the display normal (z)."""
    return Cylinder(d / 2.0, length)


def part(label, shape):
    shape.label = label
    return shape


solids = []

# ENCLOSURE_REF - approximate tray: slab, hollowed open at the back, with
# the display window cut through the front wall.
enclosure = at(0, 0, 0, Box(W, L, H))
enclosure -= at(0, 0, -1.75, Box(W - 2 * WALL, L - 2 * WALL, H - 1.5))
enclosure -= at(0, 0, H / 2 - 1, Box(71.1, 139.7, 6))
solids.append(part("ENCLOSURE_REF", enclosure))

# GRIP - side handle on the +x edge, USB-C slot cut through the base end.
grip_s = W / 2 - 4 + 16          # 60.26
grip = at(-25, grip_s, 0, Box(32, 105, 30))
grip -= at(-75.5, grip_s, 0, Box(3.4, 18, 9.2))
solids.append(part("GRIP_BODY", grip))

solids.append(part("TRIGGER", at(15, grip_s, -21, cyl_s(14, 20))))

encoder = at(36, W / 2 - 8, 0, Box(12, 24, 24))
encoder += at(36, W / 2 - 4, 0, cyl_s(7, 24.5))   # shaft to edge + 8
encoder += at(36, W / 2 + 13, 0, cyl_s(18, 10))   # knob
solids.append(part("ROTARY_ENCODER", encoder))

solids.append(part("USB_C_PORT", at(-72.8, grip_s, 0, Box(3.26, 7.35, 8.94))))

# SENSOR band across the top of the front face.
band_l = L / 2 - 18              # 64.55
blk_l, blk_s = L / 2 - 18, W / 2 - 18
block = at(blk_l, blk_s, 3, Box(32, 32, 34))
block -= at(blk_l, blk_s, 14.5, cyl_u(20, 14))    # lens bore
solids.append(part("CORNER_CAMERA_BLOCK", block))
solids.append(part("CAMERA_LENS", at(blk_l, blk_s, 12.5, cyl_u(12, 9))))
solids.append(part("CAMERA_ENVELOPE", at(blk_l, blk_s, 2, Box(25, 25, 10))))

solids.append(part("SENSOR_PILL", at(band_l, -33, 15, Box(30, 22, 2))))
solids.append(part("TOF_WINDOW", at(band_l, -40, 16.5, Box(8, 8, 1))))
solids.append(part("LED_WINDOW", at(band_l, -26, 16.5, cyl_u(10, 1))))
solids.append(part("TOF_ENVELOPE", at(band_l, -40, 10, Box(6, 6, 3))))

solids.append(part("RADAR_GRILLE", at(band_l, -6, 14.97, cyl_u(24, 2))))
solids.append(part("RADAR_ENVELOPE", at(band_l, -6, 5.5, Box(40, 20, 12))))
solids.append(part("SENSOR_CARRIER", at(band_l, -14, -4, Box(55, 20, 5))))

# Internal packaging envelopes.
solids.append(part("UNO_Q_CLEARANCE", at(44, -10, -3.5, Box(53.34, 68.58, 16))))
solids.append(part("DISPLAY", at(0, 0, 5.5, Box(77.93, 121.11, 13))))
solids.append(part("BATTERY_PACK", at(-28, 0, -7.5, Box(60, 100, 8))))

assembly = Compound(children=solids)
assembly.label = "PlatypusOne_BeefUp"

out = sys.argv[1] if len(sys.argv) > 1 else "PlatypusOne_BeefUp.step"
export_step(assembly, out)
print("wrote", out)
for s in solids:
    bb = s.bounding_box()
    print(f"  {s.label:22s} {bb.size.X:6.1f} x {bb.size.Y:6.1f} x "
          f"{bb.size.Z:6.1f} mm  @ ({bb.center().X:7.2f}, "
          f"{bb.center().Y:7.2f}, {bb.center().Z:6.2f})")
