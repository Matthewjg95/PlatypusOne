"""Hand-feel dummies of the compact display candidates, as printable STLs.

    python display_dummies.py OUT_DIR

Each dummy is printed front face down: z = 0 is the glass front, +z is the
rear, and +y is the panel's top edge in portrait. The active area is
outlined by a groove rather than sunk, because a sunk area printed face
down would be a large bridge over air. The meshes are overlapping closed
shells; slicers union them (Cura: "Union Overlapping Volumes", on by default).
"""

import math
import os
import struct
import sys

EPS = 0.01
GROOVE_W, GROOVE_D = 0.6, 0.4


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

    def tube(self, cx, cy, z0, z1, r_out, r_in, n=40):
        for i in range(n):
            a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
            o0 = (cx + r_out * math.cos(a0), cy + r_out * math.sin(a0))
            o1 = (cx + r_out * math.cos(a1), cy + r_out * math.sin(a1))
            i0 = (cx + r_in * math.cos(a0), cy + r_in * math.sin(a0))
            i1 = (cx + r_in * math.cos(a1), cy + r_in * math.sin(a1))
            self.quad((*o0, z0), (*o1, z0), (*o1, z1), (*o0, z1))
            self.quad((*i0, z0), (*i0, z1), (*i1, z1), (*i1, z0))
            self.quad((*o0, z1), (*o1, z1), (*i1, z1), (*i0, z1))
            self.quad((*o0, z0), (*i0, z0), (*i1, z0), (*o1, z0))

    def front(self, w, h, depth, va_w, va_h, va_cy=0.0):
        """Glass block w x h x depth centred on the origin; active area
        va_w x va_h centred across the width, its centre at y = va_cy."""
        lx, ly, g = w / 2, h / 2, GROOVE_W
        x0, x1 = -va_w / 2, va_w / 2
        y0, y1 = va_cy - va_h / 2, va_cy + va_h / 2
        self.box(-lx, lx, -ly, ly, GROOVE_D, depth + EPS)
        self.box(x0, x1, y0, y1, 0, GROOVE_D + EPS)
        self.box(-lx, x0 - g + EPS, -ly, ly, 0, GROOVE_D + EPS)
        self.box(x1 + g - EPS, lx, -ly, ly, 0, GROOVE_D + EPS)
        self.box(-lx, lx, y1 + g - EPS, ly, 0, GROOVE_D + EPS)
        self.box(-lx, lx, -ly, y0 - g + EPS, 0, GROOVE_D + EPS)

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


def waveshare_35h():
    """3.5inch DSI LCD (H), SKU 33087. VENDOR 2D drawing + STEP values from
    docs/hardware/compact_display_mechanical.csv. Assumed: PCB 1.6 thick and
    centred on the lens; FFC connector block size."""
    m = Mesh()
    lens_h, va_h, va_top = 88.87, 75.80, 3.63
    m.front(52.56, lens_h, 6.7, 45.36, va_h, va_cy=lens_h / 2 - va_top - va_h / 2)
    px, py, z0, z1 = 28.0, 43.25, 6.7, 8.3  # PCB 56 x 86.5
    m.box(-px, px, -py, py, z0, z1)
    hx, hy = px - 3.5, py - 3.5  # M2.5 at 49 x 58, 3.5 from the top
    for x in (-hx, hx):
        for y in (hy, hy - 58.0):
            m.tube(x, y, z1 - EPS, z1 + 4.0, 2.3, 1.25)
    fy = py - 5.6  # 15-pin FFC connector, top short edge
    m.box(-10.0, 10.0, fy - 2.75, fy + 2.75, z1 - EPS, z1 + 2.5)
    return m


def waveshare_40a():
    """4-DSI-TOUCH-A, SKU 34354, from Waveshare's dimension drawing
    (docs.waveshare.com/4-DSI-TOUCH-A#dimensions), turned to portrait.
    Drawn landscape as seen from the rear: glass 108.30 x 65.10, active
    86.40 x 51.84 (centred, scaled from the drawing), body 5.80 deep,
    rear plate 67.00 x 40.00 to 7.90 (18.65 from the edge), 8x M2.5:
    outer 100 x 56, inner 58 x 49 with 35.15 to the edge. Read from the
    side view: corner bosses 3.00 and inner bosses 5.00 behind the body.
    Not modelled: rounded glass corners, the FPC cut-out beside the plate."""
    m = Mesh()
    W, H = 108.30, 65.10  # landscape, as drawn
    body = 5.80

    # Landscape (x, y) from the rear view -> portrait: the long side vertical.
    def pt(x, y):
        return -y, x

    m.front(H, W, body, 51.84, 86.40)
    # Rear plate: x from W/2 - 18.65 - 67 to W/2 - 18.65, y +/- 20.
    px1 = W / 2 - 18.65
    px0 = px1 - 67.0
    a, b = pt(px0, -20.0), pt(px1, 20.0)
    m.box(min(a[0], b[0]), max(a[0], b[0]), min(a[1], b[1]), max(a[1], b[1]), body - EPS, 7.90)
    for x in (-50.0, 50.0):
        for y in (-28.0, 28.0):
            m.tube(*pt(x, y), body - EPS, body + 3.0, 2.5, 1.25)
    ix1 = W / 2 - 35.15
    for x in (ix1 - 58.0, ix1):
        for y in (-24.5, 24.5):
            m.tube(*pt(x, y), body - EPS, body + 5.0, 2.5, 1.25)
    return m


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    for name, build, title in [
        ("waveshare_35h_dummy.stl", waveshare_35h, "Waveshare 3.5in DSI LCD (H) SKU 33087 dummy"),
        ("waveshare_40a_dummy.stl", waveshare_40a, "Waveshare 4-DSI-TOUCH-A SKU 34354 dummy"),
    ]:
        mesh = build()
        path = os.path.join(out, name)
        mesh.write(path, title)
        print(path, len(mesh.tris), "triangles")
