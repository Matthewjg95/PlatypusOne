"""Headless STEP export of the Platypus One beef-up assembly (build123d).

Rev 3: matches PlatypusOneBeefUp.py - portrait side-grip layout, 8 mm
mid-frame band (total thickness 35.9 mm), battery re-specced to 90x60x10,
display slid 9 mm toward the grip end so the radar bay clears, and filleted
"ID pass" forms instead of raw boxes.

Includes an approximate ENCLOSURE_REF reconstruction of the existing shell
(with the proposed lowered window) so the assembly can be reviewed or
cloud-converted to a Fusion design without running Fusion. The
authoritative path is still running PlatypusOneBeefUp.py inside Fusion.

Usage:  python3 generate_step.py [out.step]
Coordinates (mm): x = across the width (+x = grip edge), y = long axis
(+y = sensor band end), z = display normal (+z = front face). z = 0 is the
centre of the ORIGINAL 27.94 mm shell; the mid-frame extends behind it.
"""

import sys

from build123d import (Axis, Box, Compound, Cylinder, Pos, Rot, export_step,
                       fillet)

L, W, H = 165.1, 96.52, 27.94
MID = 8.0
WALL = 2.5


def at(l, s, u, shape):
    """Place a shape by device-frame coords (long, side, up) -> (y, x, z)."""
    return Pos(s, l, u) * shape


def cyl_s(d, length):
    """Cylinder along the side axis (x)."""
    return Rot(0, 90, 0) * Cylinder(d / 2.0, length)


def cyl_u(d, length):
    """Cylinder along the display normal (z)."""
    return Cylinder(d / 2.0, length)


def soften(shape, axis, r_main, r_rest=None):
    """Fillet edges along one axis, then optionally everything left.
    Placeholder-grade: skip silently when the kernel refuses a radius."""
    try:
        shape = fillet(shape.edges().filter_by(axis), r_main)
    except Exception:
        pass
    if r_rest:
        try:
            shape = fillet(shape.edges(), r_rest)
        except Exception:
            pass
    return shape


def part(label, shape):
    shape.label = label
    return shape


solids = []

# ENCLOSURE_REF - rounded slab, hollow open at the back, window slid 9 mm
# toward the grip end per the rev 3 proposal.
enclosure = soften(at(0, 0, 0, Box(W, L, H)), Axis.Z, 13.0, 3.0)
enclosure -= at(0, 0, -1.75, Box(W - 2 * WALL, L - 2 * WALL, H - 1.5))
enclosure -= at(-9, 0, H / 2 - 1, Box(71.1, 139.7, 6))
solids.append(part("ENCLOSURE_REF", enclosure))

# ENCLOSURE_MID_FRAME - the 8 mm band behind the shell.
mid = soften(at(0, 0, -H / 2 - MID / 2, Box(W, L, MID)), Axis.Z, 13.0)
mid -= at(0, 0, -H / 2 - MID / 2, Box(W - 6, L - 6, MID + 2))
solids.append(part("ENCLOSURE_MID_FRAME", mid))

# GRIP - side handle, capsule section, USB-C slot cut through the base end.
grip_s = W / 2 - 4 + 16          # 60.26
grip = soften(at(-25, grip_s, 0, Box(32, 105, 30)), Axis.Y, 11.0, 5.0)
grip -= at(-75.5, grip_s, 0, Box(3.4, 18, 9.2))
solids.append(part("GRIP_BODY", grip))

solids.append(part("TRIGGER",
                   soften(at(15, grip_s, -21, cyl_s(14, 20)), Axis.X, 3.0)))

encoder = at(36, W / 2 - 8, 0, Box(12, 24, 24))
encoder += at(36, W / 2 - 4, 0, cyl_s(7, 24.5))   # shaft to edge + 8
encoder += at(36, W / 2 + 13, 0, cyl_s(18, 10))   # knob
solids.append(part("ROTARY_ENCODER", encoder))

solids.append(part("USB_C_PORT", at(-72.8, grip_s, 0, Box(3.26, 7.35, 8.94))))

# Sensor band across the top of the front face.
band_l = L / 2 - 18              # 64.55
blk_l, blk_s = L / 2 - 18, W / 2 - 18
block = soften(at(blk_l, blk_s, 3, Box(32, 32, 34)), Axis.Z, 9.0, 3.0)
block -= at(blk_l, blk_s, 14.5, cyl_u(20, 14))    # lens bore
solids.append(part("CORNER_CAMERA_BLOCK", block))
solids.append(part("CAMERA_LENS", at(blk_l, blk_s, 12.5, cyl_u(12, 9))))
solids.append(part("CAMERA_ENVELOPE", at(blk_l, blk_s, 2, Box(25, 25, 10))))

pill = soften(at(band_l, -33, 15, Box(30, 22, 2)), Axis.Z, 10.0)
solids.append(part("SENSOR_PILL", pill))
solids.append(part("TOF_WINDOW", at(band_l, -40, 16.5, cyl_u(9, 1))))
solids.append(part("LED_WINDOW", at(band_l, -26, 16.5, cyl_u(10, 1))))
solids.append(part("TOF_ENVELOPE", at(band_l, -40, 10, Box(6, 6, 3))))

solids.append(part("RADAR_GRILLE", at(band_l, -6, 14.97, cyl_u(24, 2))))
solids.append(part("RADAR_ENVELOPE", at(band_l, -6, 5.47, Box(40, 20, 12))))
solids.append(part("SENSOR_CARRIER", at(band_l, -14, -2, Box(55, 20, 2))))

# Internal packaging envelopes (back bay sits inside the mid-frame depth).
solids.append(part("UNO_Q_CLEARANCE",
                   at(45, -10, -11.47, Box(53.34, 68.58, 16))))
solids.append(part("DISPLAY", at(-9, 0, 5.47, Box(77.93, 121.11, 13))))
solids.append(part("BATTERY_PACK", at(-35, 0, -14.47, Box(60, 90, 10))))

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
