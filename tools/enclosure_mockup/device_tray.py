"""Open-top hand-feel tray for PlatypusOne: lay the parts in.

    python device_tray.py OUT.stl

Form factor per Matthew's render (2026-10-17): left-hand side grip, trigger
and barrel camera pointing out of the grip's outer edge, 4" portrait display
on the face. Front view (looking at the screen), +x toward the grip, +y toward
the top. Printed floor down; the floor is the device's back, the open top is
the screen face.

v4 (2026-10-20):
  - the grip's outer edge below the trigger follows Matthew's sketched curve:
    it scoops in to a waist, swells slightly for the palm, and sweeps into
    the base at the display/grip rib;
  - the trigger is moved in by its radius: its tip is level with the barrel
    face, and it stands proud only of the scooped grip;
  - the battery stand-ins (two M5Stack A068, 48 x 24 x 12) move under the
    display, since the narrowed grip is now about 15 mm wide inside;
  - the charging-port slot moves along the base to where the grip meets it.

Sections, sensor end first:
  sensor band   above the display: UNO Q + UNO Media Carrier shield, UNO
                footprint 68.58 x 53.34 (VENDOR), stack ESTIMATED
  barrel bay    top of the grip: B0394 (25 x 24 board) and Pololu #3419
                (12.7 x 22.9 board; ~18 deep with headers and the cable's
                sockets) side by side on Matthew's bracket, looking out
                through a notch open to the rim
  display bay   4-DSI-TOUCH-A 65.10 x 108.30, body 5.80, bosses to 10.80
                (VENDOR drawing) on ledges, glass flush with the rim; 14 mm
                under it for the battery stand-ins
  grip          below the barrel: trigger, then the sketched curve
"""

import math
import struct
import sys

EPS = 0.01
WALL, FLOOR, R_OUT, SEG = 2.0, 2.0, 8.0, 12

DISP_W, DISP_H, DISP_BODY = 65.10, 108.30, 5.80
CLEAR, LEDGE = 0.5, 1.5
UNO_L, UNO_W = 68.58, 53.34
UNO_STACK, CABLE_HEADROOM = 22.0, 3.0  # stack ESTIMATED; MEASURE
RIB = 2.0
GRIP_IN = 30.0  # grip column interior width at the barrel
BARREL_L = 50.0  # camera 25 + gap 4 + ToF 12.7 + margins, below the corner
NOTCH_L, NOTCH_Z0 = 42.0, 4.0
TRIGGER_L, TRIGGER_OUT = 24.0, 10.0
TRIGGER_IN = TRIGGER_OUT  # moved in by its radius: tip level with the barrel face
USB_W, USB_H, USB_Z = 12.0, 7.0, 9.0
NOTCH = 22.0

DISP_IN_W, DISP_IN_L = DISP_W + 2 * CLEAR, DISP_H + 2 * CLEAR
BAND_IN_L = UNO_W + 2.66
BAND_IN_W = UNO_L + 2.0
IN_W = DISP_IN_W + RIB + GRIP_IN
IN_L = DISP_IN_L + RIB + BAND_IN_L
H = FLOOR + UNO_STACK + CABLE_HEADROOM
OUT_W, OUT_L = IN_W + 2 * WALL, IN_L + 2 * WALL
hw, hl = OUT_W / 2, OUT_L / 2

X0, Y0 = -hw + WALL, -hl + WALL
XT, YT = hw - WALL, hl - WALL
x_disp1 = X0 + DISP_IN_W
y_disp1 = Y0 + DISP_IN_L
x_grip0 = x_disp1 + RIB
x_band1 = X0 + BAND_IN_W
x_barrel0 = x_band1 + RIB
y_barrel0 = YT - BARREL_L
ty1 = y_barrel0 - RIB - 4.0
ty0 = ty1 - TRIGGER_L

# Matthew's sketched grip edge (2026-10-20), traced from the plan view in mm,
# from the trigger's bottom on the outer wall down to the base at the rib.
GRIP_CURVE = [
    (hw, ty0),
    (44.0, -4.0),
    (41.0, -15.0),
    (37.5, -30.0),
    (35.8, -48.0),
    (37.5, -62.0),
    (39.8, -72.0),
    (38.0, -80.0),
    (32.0, -84.6),
    (x_grip0 + 1.0, -hl),
]

tris = []


def quad(a, b, c, d):
    tris.extend([(a, b, c), (a, c, d)])


def box(x0, x1, y0, y1, z0, z1):
    assert x0 < x1 and y0 < y1 and z0 < z1, (x0, x1, y0, y1, z0, z1)
    p = [(x, y, z) for z in (z0, z1) for y in (y0, y1) for x in (x0, x1)]
    quad(p[0], p[2], p[3], p[1])
    quad(p[4], p[5], p[7], p[6])
    quad(p[0], p[1], p[5], p[4])
    quad(p[2], p[6], p[7], p[3])
    quad(p[0], p[4], p[6], p[2])
    quad(p[1], p[3], p[7], p[5])


def catmull_rom(pts, per=10):
    """Smooth curve through the points (ends included)."""
    out = []
    ext = [pts[0]] + pts + [pts[-1]]
    for i in range(1, len(ext) - 2):
        p0, p1, p2, p3 = ext[i - 1], ext[i], ext[i + 1], ext[i + 2]
        for k in range(per):
            t = k / per
            t2, t3 = t * t, t * t * t
            out.append(
                tuple(
                    0.5
                    * (
                        (2 * p1[j])
                        + (-p0[j] + p2[j]) * t
                        + (2 * p0[j] - 5 * p1[j] + 4 * p2[j] - p3[j]) * t2
                        + (-p0[j] + 3 * p1[j] - 3 * p2[j] + p3[j]) * t3
                    )
                    for j in range(2)
                )
            )
    out.append(pts[-1])
    return out


def arc(cx, cy, r, a0):
    return [
        (
            cx + r * math.cos(math.radians(a0 + 90 * i / SEG)),
            cy + r * math.sin(math.radians(a0 + 90 * i / SEG)),
        )
        for i in range(SEG + 1)
    ]


# --- outline (CCW), with the straight runs that carry openings split out so
# those runs can be rebuilt as boxes around the opening.
wy1 = hl - R_OUT - 1.0  # sensor notch, on the straight barrel face
wy0 = wy1 - NOTCH_L
assert y_barrel0 - RIB <= wy0 and ty1 < wy0, (wy0, ty1)
usb_cx = x_grip0 - 8.0  # charging port where the grip meets the base
sx0, sx1 = usb_cx - USB_W / 2, usb_cx + USB_W / 2

R = R_OUT
outline = []
outline += arc(hw - R, hl - R, R, 0)  # top-right corner (starts on the right face)
outline += arc(-hw + R, hl - R, R, 90)  # top-left
outline += arc(-hw + R, -hl + R, R, 180)  # bottom-left (ends on the base)
i_base_slot = len(outline)  # base run: ... -> slot start -> slot end -> ...
outline += [(sx0 - 2.0, -hl), (sx1 + 2.0, -hl)]
curve = catmull_rom(list(reversed(GRIP_CURVE)))  # base -> trigger bottom
outline += curve
outline.append((hw, wy0 - 1.0))  # right face below the notch
i_notch = len(outline) - 1  # segment from here to vertex 0 holds the notch
SKIP = {i_base_slot, i_notch}

for i in range(len(outline)):  # no zero-length edges
    assert math.dist(outline[i], outline[(i + 1) % len(outline)]) > 1e-6, i


def signed_area(poly):
    return 0.5 * sum(
        poly[i][0] * poly[(i + 1) % len(poly)][1] - poly[(i + 1) % len(poly)][0] * poly[i][1]
        for i in range(len(poly))
    )


assert signed_area(outline) > 0, "outline must be CCW"


def offset_inward(poly, d):
    """Mitered inward offset of a CCW polygon (interior on the left)."""
    n = len(poly)
    out = []
    for i in range(n):
        a, b, c = poly[i - 1], poly[i], poly[(i + 1) % n]
        e1 = (b[0] - a[0], b[1] - a[1])
        e2 = (c[0] - b[0], c[1] - b[1])
        l1, l2 = math.hypot(*e1), math.hypot(*e2)
        n1 = (-e1[1] / l1, e1[0] / l1)
        n2 = (-e2[1] / l2, e2[0] / l2)
        k = 1.0 + n1[0] * n2[0] + n1[1] * n2[1]
        out.append((b[0] + d * (n1[0] + n2[0]) / k, b[1] + d * (n1[1] + n2[1]) / k))
    return out


inner = offset_inward(outline, WALL)


def ring(outer, inner, z0, z1, skip=()):
    """Walls between two CCW outlines with matching vertices; segments in
    `skip` (vertex i -> i+1) are left out and the open ends capped."""
    n = len(outer)
    for i in range(n):
        if i in skip:
            continue
        o0, o1 = outer[i], outer[(i + 1) % n]
        i0, i1 = inner[i], inner[(i + 1) % n]
        quad((*o0, z0), (*o1, z0), (*o1, z1), (*o0, z1))
        quad((*i0, z0), (*i0, z1), (*i1, z1), (*i1, z0))
        quad((*o0, z1), (*o1, z1), (*i1, z1), (*i0, z1))
        quad((*o0, z0), (*i0, z0), (*i1, z0), (*o1, z0))
    for i in skip:
        o, i_ = outer[i], inner[i]
        quad((*o, z0), (*i_, z0), (*i_, z1), (*o, z1))
        j = (i + 1) % n
        o, i_ = outer[j], inner[j]
        quad((*o, z0), (*o, z1), (*i_, z1), (*i_, z0))


def triangulate(poly):
    """Ear clipping for a simple CCW polygon."""
    idx = list(range(len(poly)))

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    def inside(p, a, b, c):
        return cross(a, b, p) >= 0 and cross(b, c, p) >= 0 and cross(c, a, p) >= 0

    out = []
    while len(idx) > 3:
        for k in range(len(idx)):
            ia, ib, ic = idx[k - 1], idx[k], idx[(k + 1) % len(idx)]
            a, b, c = poly[ia], poly[ib], poly[ic]
            if cross(a, b, c) <= 1e-12:
                continue
            if any(inside(poly[j], a, b, c) for j in idx if j not in (ia, ib, ic)):
                continue
            out.append((ia, ib, ic))
            idx.pop(k)
            break
        else:
            raise RuntimeError("triangulation stalled")
    out.append(tuple(idx))
    return out


def prism_poly(poly, z0, z1):
    for a, b, c in triangulate(poly):
        tris.append(((*poly[a], z0), (*poly[c], z0), (*poly[b], z0)))
        tris.append(((*poly[a], z1), (*poly[b], z1), (*poly[c], z1)))
    n = len(poly)
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        quad((*a, z0), (*b, z0), (*b, z1), (*a, z1))


prism_poly(outline, 0, FLOOR)
ring(outline, inner, FLOOR - EPS, H, skip=SKIP)

# Barrel face around the sensor notch (open to the rim).
wz0 = FLOOR + NOTCH_Z0
yn0, yn1 = wy0 - 1.0, hl - R_OUT
box(hw - WALL, hw, yn0 - EPS, yn1 + EPS, FLOOR - EPS, wz0 + EPS)
box(hw - WALL, hw, yn0 - EPS, wy0, wz0, H)
box(hw - WALL, hw, wy1, yn1 + EPS, wz0, H)

# Base around the charging-port slot.
sz0, sz1 = USB_Z - USB_H / 2, USB_Z + USB_H / 2
xa, xb = sx0 - 2.0, sx1 + 2.0
box(xa - EPS, sx0, -hl, -hl + WALL, FLOOR - EPS, H)
box(sx1, xb + EPS, -hl, -hl + WALL, FLOOR - EPS, H)
box(sx0 - EPS, sx1 + EPS, -hl, -hl + WALL, FLOOR - EPS, sz0)
box(sx0 - EPS, sx1 + EPS, -hl, -hl + WALL, sz1, H)

# Ribs, full height, with gaps for cables.
box(x_disp1 - EPS, x_grip0, Y0 - EPS, y_disp1 + RIB, FLOOR - EPS, H)  # display | grip
cx = X0 + DISP_IN_W / 2
box(X0 - EPS, cx - NOTCH / 2, y_disp1, y_disp1 + RIB, FLOOR - EPS, H)  # display | band
box(cx + NOTCH / 2, x_disp1, y_disp1, y_disp1 + RIB, FLOOR - EPS, H)
cy = y_barrel0 + BARREL_L / 2
box(x_band1, x_barrel0, y_barrel0 - RIB, cy - NOTCH / 4, FLOOR - EPS, H)  # band | barrel
box(x_band1, x_barrel0, cy + NOTCH / 4, YT + EPS, FLOOR - EPS, H)
box(x_band1, XT + EPS, y_barrel0 - RIB, y_barrel0, FLOOR - EPS, H)  # barrel | grip

# Display ledges inside its bay: glass flush with the rim.
z_ledge = H - DISP_BODY
box(X0 - EPS, X0 + LEDGE, Y0 - EPS, y_disp1 + EPS, FLOOR - EPS, z_ledge)
box(x_disp1 - LEDGE, x_disp1 + EPS, Y0 - EPS, y_disp1 + EPS, FLOOR - EPS, z_ledge)
box(X0 - EPS, x_disp1 + EPS, Y0 - EPS, Y0 + LEDGE, FLOOR - EPS, z_ledge)
box(X0 - EPS, cx - NOTCH / 2, y_disp1 - LEDGE, y_disp1 + EPS, FLOOR - EPS, z_ledge)
box(cx + NOTCH / 2, x_disp1 + EPS, y_disp1 - LEDGE, y_disp1 + EPS, FLOOR - EPS, z_ledge)

# Trigger: the same hump as before, moved in by its radius so its tip is
# level with the barrel face; below it the grip curve scoops away.
o, s = TRIGGER_OUT, TRIGGER_IN
trig = [
    (hw - 1.0 - s, ty0),
    (hw + 0.6 * o - s, ty0 + 2.5),
    (hw + o - s, ty0 + 9.0),
    (hw + o - s, ty1 - 5.0),
    (hw + 0.6 * o - s, ty1 - 1.0),
    (hw - 1.0 - s, ty1),
]
prism_poly(trig, 0, H)


def normal(a, b, c):
    u = [b[i] - a[i] for i in range(3)]
    v = [c[i] - a[i] for i in range(3)]
    n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    m = math.sqrt(sum(k * k for k in n)) or 1.0
    return tuple(k / m for k in n)


out = sys.argv[1] if len(sys.argv) > 1 else "platypusone_tray_40a.stl"
with open(out, "wb") as f:
    f.write(b"PlatypusOne open tray v4, left-hand side grip, 4in display".ljust(80, b" "))
    f.write(struct.pack("<I", len(tris)))
    for t in tris:
        f.write(struct.pack("<3f", *normal(*t)))
        for p in t:
            f.write(struct.pack("<3f", *p))
        f.write(b"\0\0")
waist = min(p[0] for p in curve if p[1] > -80.0)
print(f"{out}: {OUT_W:.1f} W x {OUT_L:.1f} L x {H:.1f} H mm, {len(tris)} triangles")
print(f"grip: outer edge at the waist x={waist:.1f}, inside {waist - WALL - x_grip0:.1f} mm wide")
print(
    f"trigger x {hw - 1 - s:.2f}..{hw + o - s:.2f} y {ty0:.2f}..{ty1:.2f}; "
    f"notch y {wy0:.2f}..{wy1:.2f}; usb x {sx0:.2f}..{sx1:.2f}"
)
