"""UNO Q hole-pattern test stands (printable STL).

    python unoq_test_stand.py OUT_DIR

Hole centres measured from CAD/arduino_q.step (four R1.6 cylinders); they match
the standard UNO pattern seen from the component side, origin at the board's
lower-left corner:
    (13.97, 2.54) (15.24, 50.80) (66.04, 7.62) (66.04, 35.56); board 68.58 x 53.34.

  upright: component side UP on short posts  -> checks the pattern itself.
  flipped: component side DOWN on 11 mm posts (the enclosure mounting) ->
           the pattern seen from above is mirrored (y -> 53.34 - y).
Both: Ø6 posts, Ø2.8 x 3 mm pins, a 0.6 mm raised outline of the board on the
plate, and a second outline line along one short edge.

Bench result 2026-11-01: the flipped stand fits (mirrored pattern confirmed).
The Ø6 post at (15.24, 50.80) hits the long header's body, which starts
2.05 mm from that hole centre (CAD): give that post a flat 1.8 mm from the
centre on the header side. CAD also shows a ~2 mm tall part ~1.8 mm from the
(66.04, 35.56) hole, toward the Wi-Fi module corner: check that corner seats.
"""

import math
import struct
import sys
from pathlib import Path

L, W = 68.58, 53.34
HOLES = [(13.97, 2.54), (15.24, 50.80), (66.04, 7.62), (66.04, 35.56)]
MARGIN, PLATE_T = 5.0, 2.0
POST_R, PIN_R, PIN_H = 3.0, 1.4, 3.0
LINE_W, LINE_H = 0.8, 0.6
EPS = 0.01


class Mesh:
    def __init__(self):
        self.tris = []

    def quad(self, a, b, c, d):
        self.tris += [(a, b, c), (a, c, d)]

    def box(self, x0, x1, y0, y1, z0, z1):
        p = [(x, y, z) for z in (z0, z1) for y in (y0, y1) for x in (x0, x1)]
        self.quad(p[0], p[2], p[3], p[1])
        self.quad(p[4], p[5], p[7], p[6])
        self.quad(p[0], p[1], p[5], p[4])
        self.quad(p[2], p[6], p[7], p[3])
        self.quad(p[0], p[4], p[6], p[2])
        self.quad(p[1], p[3], p[7], p[5])

    def cyl(self, cx, cy, z0, z1, r, n=48):
        for i in range(n):
            a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
            p0 = (cx + r * math.cos(a0), cy + r * math.sin(a0))
            p1 = (cx + r * math.cos(a1), cy + r * math.sin(a1))
            self.quad((*p0, z0), (*p1, z0), (*p1, z1), (*p0, z1))
            self.tris.append(((cx, cy, z1), (*p0, z1), (*p1, z1)))
            self.tris.append(((cx, cy, z0), (*p1, z0), (*p0, z0)))

    def write(self, path, title):
        with open(path, "wb") as f:
            f.write(title.encode()[:80].ljust(80, b" "))
            f.write(struct.pack("<I", len(self.tris)))
            for t in self.tris:
                f.write(struct.pack("<3f", *normal(*t)))
                for p in t:
                    f.write(struct.pack("<3f", *p))
                f.write(b"\0\0")


def normal(a, b, c):
    u = [b[i] - a[i] for i in range(3)]
    v = [c[i] - a[i] for i in range(3)]
    n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    m = math.sqrt(sum(k * k for k in n)) or 1.0
    return tuple(k / m for k in n)


def stand(post_h, flipped):
    m = Mesh()
    m.box(-MARGIN, L + MARGIN, -MARGIN, W + MARGIN, 0, PLATE_T)
    z0, z1 = PLATE_T - EPS, PLATE_T + LINE_H
    h = LINE_W / 2
    m.box(-h, L + h, -h, h, z0, z1)  # outline of the board
    m.box(-h, L + h, W - h, W + h, z0, z1)
    m.box(-h, h, -h, W + h, z0, z1)
    m.box(L - h, L + h, -h, W + h, z0, z1)
    m.box(-2.5 - h, -2.5 + h, 0, W, z0, z1)  # second line along the x = 0 edge
    for x, y in HOLES:
        if flipped:
            y = W - y
        m.cyl(x, y, PLATE_T - EPS, PLATE_T + post_h, POST_R)
        m.cyl(x, y, PLATE_T + post_h - EPS, PLATE_T + post_h + PIN_H, PIN_R)
    return m


if __name__ == "__main__":
    out = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    out.mkdir(parents=True, exist_ok=True)
    stand(5.0, False).write(out / "unoq_stand_upright_5mm.stl", "UNO Q stand, component side up")
    stand(11.0, True).write(out / "unoq_stand_flipped_11mm.stl", "UNO Q stand, component side down")
    print("wrote", out)
