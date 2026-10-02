"""Offline re-analysis of scan-0053 (flat washer). NOT on-device output.

Reads the preserved raw frame and the on-device record, then:
  * segments the washer from the paper (midpoint between metal and paper levels);
  * fits circles (algebraic least squares) to its outer edge and its bore;
  * measures the 6-fold harmonic of the outer edge (round washer ~0, hex nut ~7 %);
using the on-device mm/px (the reference square's area scale) unchanged.

    python washer_circle_fit.py            # from this directory

Dependencies: numpy. Deterministic.
"""

import json
import os

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
CAPTURE = os.path.join(HERE, "..", "scan-0053")


def largest_component(mask):
    h, w = mask.shape
    seen = np.zeros_like(mask, bool)
    best = []
    for sy, sx in zip(*np.nonzero(mask), strict=True):
        if seen[sy, sx]:
            continue
        comp, stack = [], [(sy, sx)]
        seen[sy, sx] = True
        while stack:
            cy, cx = stack.pop()
            comp.append((cy, cx))
            for ny, nx in ((cy + 1, cx), (cy - 1, cx), (cy, cx + 1), (cy, cx - 1)):
                if 0 <= ny < h and 0 <= nx < w and mask[ny, nx] and not seen[ny, nx]:
                    seen[ny, nx] = True
                    stack.append((ny, nx))
        if len(comp) > len(best):
            best = comp
    out = np.zeros_like(mask, bool)
    a = np.array(best)
    out[a[:, 0], a[:, 1]] = True
    return out


def boundary(mask):
    inner = np.roll(mask, 1, 0) & np.roll(mask, -1, 0)
    inner &= np.roll(mask, 1, 1) & np.roll(mask, -1, 1)
    yy, xx = np.nonzero(mask & ~inner)
    return np.stack([xx, yy], 1).astype(float)


def fit_circle(p):
    A = np.c_[2 * p[:, 0], 2 * p[:, 1], np.ones(len(p))]
    cx, cy, k = np.linalg.lstsq(A, (p**2).sum(1), rcond=None)[0]
    r = np.sqrt(k + cx * cx + cy * cy)
    res = np.linalg.norm(p - [cx, cy], axis=1) - r
    return np.array([cx, cy]), r, res


def main():
    raw = np.fromfile(os.path.join(CAPTURE, "source.yuyv"), np.uint8).reshape(480, 1280)
    y = raw[:, 0::2].astype(float)
    with open(os.path.join(CAPTURE, "observation.json"), encoding="utf-8") as f:
        record = json.load(f)
    mmpp = next(c["value"] for c in record["derived"] if c["name"] == "mm_per_pixel")

    roi = (slice(120, 400), slice(40, 300))  # the washer's region of this frame
    metal, paper = np.percentile(y[roi], 10), np.percentile(y[roi], 90)
    threshold = (metal + paper) / 2
    mask = np.zeros_like(y, bool)
    mask[roi] = y[roi] < threshold
    mask = largest_component(mask)

    edge = boundary(mask)
    r0 = np.linalg.norm(edge - edge.mean(0), axis=1)
    split = (r0.min() + r0.max()) / 2
    outer, bore = edge[r0 > split] * mmpp, edge[r0 <= split] * mmpp

    co, ro, reso = fit_circle(outer)
    cb, rb, resb = fit_circle(bore)
    theta = np.arctan2(outer[:, 1] - co[1], outer[:, 0] - co[0])
    six = abs(np.mean((np.linalg.norm(outer - co, axis=1) / ro - 1) * np.exp(6j * theta))) * 2

    print(f"mm_per_pixel (on-device, area scale): {mmpp:.5f}")
    print(f"segmentation threshold: {threshold:.0f} (metal ~{metal:.0f}, paper ~{paper:.0f})")
    rms_o, rms_b = np.sqrt((reso**2).mean()), np.sqrt((resb**2).mean())
    print(f"outer diameter: {2 * ro:.2f} mm (edge rms residual {rms_o:.3f} mm)")
    print(f"bore diameter:  {2 * rb:.2f} mm (edge rms residual {rms_b:.3f} mm)")
    print(f"bore offset from outer centre: {np.linalg.norm(co - cb):.2f} mm")
    print(f"6-fold harmonic of the outer edge: {100 * six:.2f} % of radius")


if __name__ == "__main__":
    main()
