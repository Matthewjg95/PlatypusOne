"""Headless STEP export of the Platypus One beef-up assembly (build123d).

Rev 4 - review notes applied:
  * back closed: mid-frame is a tub with a 2.5 mm back wall, shell walls are
    continuous offset shells (fillets connect at the corners)
  * DISPLAY_GLASS panel closes the window (no more see-through screen)
  * trigger peg removed - the grip silhouette now carries a finger hook
  * camera lens stack moved up the corner block
  * grip rebuilt from a splined side profile pulled 8 mm into the body:
    hook, palm swell, tapered base - the PlatypusOne curve
  * parts carry display colors for viewer legibility

Layout numbers match PlatypusOneBeefUp.py rev 3+ (35.9 mm total thickness,
battery 90x60x10, display slid -9 mm).

Usage:  python3 generate_step.py [out.step]
Coordinates (mm): x = across the width (+x = grip edge), y = long axis
(+y = sensor band end), z = display normal (+z = front face). z = 0 is the
centre of the ORIGINAL 27.94 mm shell; the mid-frame extends behind it.
"""

import sys

from build123d import (Axis, Box, Color, Compound, Cylinder, Pos, Rot,
                       Spline, export_step, extrude, fillet, make_face)

L, W, H = 165.1, 96.52, 27.94
MID = 8.0
WALL = 2.5
R_OUT = 13.0   # outer corner radius
R_IN = 10.5    # matching inner-cavity radius keeps walls continuous


def at(l, s, u, shape):
    """Place a shape by device-frame coords (long, side, up) -> (y, x, z)."""
    return Pos(s, l, u) * shape


def cyl_s(d, length):
    """Cylinder along the side axis (x)."""
    return Rot(0, 90, 0) * Cylinder(d / 2.0, length)


def cyl_u(d, length):
    """Cylinder along the display normal (z)."""
    return Cylinder(d / 2.0, length)


def rslab(w, l, depth, r):
    """Rounded-corner slab centred at the origin (w across x, l along y)."""
    shape = Box(w, l, depth)
    try:
        shape = fillet(shape.edges().filter_by(Axis.Z), r)
    except Exception:
        pass
    return shape


def soften(shape, radius):
    try:
        return fillet(shape.edges(), radius)
    except Exception:
        return shape


def part(label, shape, rgb):
    shape.label = label
    shape.color = Color(*rgb)
    return shape


SILVER = (0.72, 0.74, 0.77)
CHARCOAL = (0.16, 0.17, 0.19)
GRIPCOL = (0.20, 0.22, 0.25)
GLASS = (0.05, 0.06, 0.08)
TOUCH = (0.88, 0.56, 0.27)
STEEL = (0.60, 0.63, 0.67)

solids = []

# ENCLOSURE_REF - continuous offset walls, window (slid -9 mm) with rounded
# corners, open only toward the mid-frame at the back.
enclosure = rslab(W, L, H, R_OUT)
enclosure -= at(0, 0, -WALL / 2 - 0.5,
                rslab(W - 2 * WALL, L - 2 * WALL, H - WALL + 1.0, R_IN))
enclosure -= at(-9, 0, H / 2 - 1, rslab(71.1, 139.7, 6, 8.0))
solids.append(part("ENCLOSURE_REF", enclosure, SILVER))

# DISPLAY_GLASS - closes the window, recessed 0.8 mm below the front face.
glass = at(-9, 0, H / 2 - 1.6, rslab(70.3, 138.9, 1.6, 7.6))
solids.append(part("DISPLAY_GLASS", glass, GLASS))

# ENCLOSURE_MID_FRAME - tub with a 2.5 mm back wall: closes the device.
mid = rslab(W, L, MID, R_OUT)
mid -= at(0, 0, WALL / 2 + 0.5,
          rslab(W - 2 * WALL, L - 2 * WALL, MID - WALL + 1.0, R_IN))
mid = at(0, 0, -H / 2 - MID / 2, mid)
solids.append(part("ENCLOSURE_MID_FRAME", mid, CHARCOAL))

# GRIP - splined side silhouette (s, l): pulled into the body at s ~40,
# finger hook at the top, palm swell, tapered base. Extruded 30 mm thick.
profile_pts = [
    (40, 26), (56, 24), (72, 15),          # over the top to the hook tip
    (68, 5), (56, 1),                       # hook underside (finger notch)
    (62, -8), (75, -18), (80, -30),         # out to the palm swell
    (78, -46), (74, -62), (69, -74),        # taper
    (60, -80), (50, -77), (44, -60),        # base round-off
    (41, -40), (40, -8), (40, 10),          # inner edge, inside the body
    (40, 26),
]
grip = extrude(make_face(Spline(*profile_pts)), 30)
grip = Pos(0, 0, -15) * grip
for r in (6.0, 3.5):
    try:
        grip = fillet(grip.edges().group_by(Axis.Z)[0], r)
        grip = fillet(grip.edges().group_by(Axis.Z)[-1], r)
        break
    except Exception:
        continue
grip -= at(-76, 58, 0, Box(3.4, 14, 9.2))   # USB-C slot through the base
solids.append(part("GRIP_BODY", grip, GRIPCOL))

encoder = at(36, W / 2 - 8, 0, Box(12, 24, 24))
encoder += at(36, W / 2 - 4, 0, cyl_s(7, 24.5))   # shaft to edge + 8
encoder += at(36, W / 2 + 13, 0, cyl_s(18, 10))   # knob
solids.append(part("ROTARY_ENCODER", encoder, TOUCH))

solids.append(part("USB_C_PORT", at(-71, 58, 0, Box(3.26, 7.35, 8.94)),
                   TOUCH))

# Sensor band across the top of the front face.
band_l = L / 2 - 18              # 64.55
blk_l, blk_s = L / 2 - 18, W / 2 - 18
cam_l = 70.0                     # lens stack riding high on the block
block = soften(rslab(32, 32, 34, 9.0), 3.0)
block = at(blk_l, blk_s, 3, block)
block -= at(cam_l, blk_s, 14.5, cyl_u(20, 14))    # lens bore
solids.append(part("CORNER_CAMERA_BLOCK", block, (0.24, 0.27, 0.32)))
solids.append(part("CAMERA_LENS", at(cam_l, blk_s, 12.5, cyl_u(12, 9)),
                   (0.11, 0.17, 0.27)))
solids.append(part("CAMERA_ENVELOPE", at(67, blk_s, 2, Box(25, 25, 10)),
                   (0.55, 0.44, 0.79)))

pill = at(band_l, -33, 15, rslab(30, 22, 2, 10.0))
solids.append(part("SENSOR_PILL", pill, (0.17, 0.19, 0.23)))
solids.append(part("TOF_WINDOW", at(band_l, -40, 16.5, cyl_u(9, 1)), GLASS))
solids.append(part("LED_WINDOW", at(band_l, -26, 16.5, cyl_u(10, 1)),
                   (0.91, 0.89, 0.85)))
solids.append(part("TOF_ENVELOPE", at(band_l, -40, 10, Box(6, 6, 3)),
                   (0.79, 0.36, 0.39)))

solids.append(part("RADAR_GRILLE", at(band_l, -6, 14.97, cyl_u(24, 2)),
                   STEEL))
solids.append(part("RADAR_ENVELOPE", at(band_l, -6, 5.47, Box(40, 20, 12)),
                   (0.35, 0.63, 0.72)))
solids.append(part("SENSOR_CARRIER", at(band_l, -14, -2, Box(55, 20, 2)),
                   (0.42, 0.48, 0.56)))

# Internal packaging envelopes (back bay sits inside the mid-frame depth).
solids.append(part("UNO_Q_CLEARANCE",
                   at(45, -10, -11.47, Box(53.34, 68.58, 16)),
                   (0.12, 0.54, 0.44)))
solids.append(part("DISPLAY", at(-9, 0, 5.47, Box(77.93, 121.11, 13)),
                   (0.18, 0.44, 0.86)))
solids.append(part("BATTERY_PACK", at(-35, 0, -14.47, Box(60, 90, 10)),
                   (0.85, 0.64, 0.25)))

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
