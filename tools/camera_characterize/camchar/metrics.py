"""Per-frame image diagnostics: exposure, region sharpness, edge rise.

All are RELATIVE engineering diagnostics. None is an optical-resolution claim:
  - region sharpness (normalised Tenengrad / Laplacian variance) depends on
    scene content, so compare only frames of the same scene and placement;
  - edge rise (10-90 % width of a dark-square edge, in pixels) measures blur
    of the whole chain (focus + lens + sensor + processing) at that pixel
    grid; it is comparable across distances but only at the same resolution
    and format.
"""

from __future__ import annotations

from typing import Any

import cv2
import numpy as np

from .images import Loaded

# Fraction of the normalised range at the bottom that counts as crushed black.
CRUSH_FRACTION = 0.005

REGION_NAMES = {
    (0, 0): "top_left",
    (0, 1): "top",
    (0, 2): "top_right",
    (1, 0): "left",
    (1, 1): "center",
    (1, 2): "right",
    (2, 0): "bottom_left",
    (2, 1): "bottom",
    (2, 2): "bottom_right",
}
CORNERS = ("top_left", "top_right", "bottom_left", "bottom_right")


def exposure(img: Loaded) -> dict[str, Any]:
    lum = img.lum
    p = np.percentile(lum, [1, 50, 99])
    return {
        "mean": float(lum.mean()),
        "median": float(p[1]),
        "p01": float(p[0]),
        "p99": float(p[2]),
        "rms_contrast": float(lum.std()),
        "p99_minus_p01": float(p[2] - p[0]),
        "clipped_pct": float(100.0 * img.clipped.mean()),
        "crushed_pct": float(100.0 * (lum <= CRUSH_FRACTION).mean()),
        "linear_signal": img.linear,
    }


def _region_slices(h: int, w: int):
    ys = [0, h // 3, 2 * h // 3, h]
    xs = [0, w // 3, 2 * w // 3, w]
    for (r, c), name in REGION_NAMES.items():
        yield name, slice(ys[r], ys[r + 1]), slice(xs[c], xs[c + 1])


def region_sharpness(img: Loaded) -> dict[str, Any]:
    """Normalised Tenengrad and Laplacian variance on a 3x3 grid.

    Each is divided by the region's mean luminance squared, so a uniformly
    brighter exposure of the same scene does not read as sharper.
    """
    lum = img.lum.astype(np.float64)
    gx = cv2.Sobel(lum, cv2.CV_64F, 1, 0, ksize=3)
    gy = cv2.Sobel(lum, cv2.CV_64F, 0, 1, ksize=3)
    g2 = gx * gx + gy * gy
    lap = cv2.Laplacian(lum, cv2.CV_64F, ksize=3)
    out: dict[str, Any] = {}
    for name, ys, xs in _region_slices(*lum.shape):
        m = max(float(lum[ys, xs].mean()), 1e-6)
        out[name] = {
            "tenengrad_norm": float(g2[ys, xs].mean() / (m * m)),
            "laplacian_var_norm": float(lap[ys, xs].var() / (m * m)),
        }
    for key in ("tenengrad_norm", "laplacian_var_norm"):
        corner = float(np.mean([out[c][key] for c in CORNERS]))
        out[f"corners_mean_{key}"] = corner
        center = out["center"][key]
        out[f"corner_to_center_{key}"] = corner / center if center > 0 else None
    return out


# --- slanted-edge rise on the dark reference square ---------------------------


def find_dark_square(gray8: np.ndarray, min_side_px: float = 20.0):
    """Most square-like dark blob: returns (box 4x2 float, side_px) or None.

    Same scene contract as the Scout analyzer: a dark filled square on light
    paper. Used here only to locate edges for blur measurement.
    """
    _, bw = cv2.threshold(gray8, 0, 255, cv2.THRESH_BINARY_INV + cv2.THRESH_OTSU)
    contours, _ = cv2.findContours(bw, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
    best = None
    h, w = gray8.shape
    for c in contours:
        area = cv2.contourArea(c)
        if area < min_side_px * min_side_px:
            continue
        (cx, cy), (rw, rh), _ = rect = cv2.minAreaRect(c)
        if rw <= 0 or rh <= 0:
            continue
        aspect = max(rw, rh) / min(rw, rh)
        extent = area / (rw * rh)
        x, y, bw_, bh_ = cv2.boundingRect(c)
        if x <= 1 or y <= 1 or x + bw_ >= w - 1 or y + bh_ >= h - 1:
            continue  # cut by the frame edge
        if aspect > 1.15 or extent < 0.9:
            continue
        score = abs(aspect - 1) + (1 - extent)
        if best is None or score < best[0]:
            best = (score, cv2.boxPoints(rect), float(np.sqrt(area)))
    if best is None:
        return None
    return best[1].astype(np.float64), best[2]


def _sample(lum32: np.ndarray, xs: np.ndarray, ys: np.ndarray) -> np.ndarray:
    return cv2.remap(
        lum32,
        xs.astype(np.float32).reshape(1, -1),
        ys.astype(np.float32).reshape(1, -1),
        cv2.INTER_LINEAR,
        borderMode=cv2.BORDER_REPLICATE,
    ).ravel()


def _rise_10_90(profile: np.ndarray, s: np.ndarray, min_contrast: float) -> float | None:
    n = len(profile)
    k = max(3, n // 5)
    dark = float(np.median(profile[:k]))
    bright = float(np.median(profile[-k:]))
    if bright - dark < min_contrast:
        return None
    p = (profile - dark) / (bright - dark)
    mid = int(np.argmin(np.abs(p - 0.5)))

    def cross(level: float, step: int) -> float | None:
        i = mid
        while 0 <= i + step < n:
            a, b = p[i], p[i + step]
            if (a - level) * (b - level) <= 0 and a != b:
                t = (level - a) / (b - a)
                return float(s[i] + t * (s[i + step] - s[i]))
            i += step
        return None

    s10 = cross(0.1, -1)
    s90 = cross(0.9, +1)
    if s10 is None or s90 is None:
        return None
    return s90 - s10


def edge_rise(img: Loaded, min_contrast: float = 0.1) -> dict[str, Any] | None:
    """10-90 % edge width (px) of the dark reference square, median over edges.

    Profiles run along each edge normal at 9 positions over the middle 60 % of
    the edge, sampled every 0.25 px over +/- min(0.2 * side, 20 px).
    """
    found = find_dark_square(img.gray8)
    if found is None:
        return None
    box, side = found
    lum32 = img.lum.astype(np.float32)
    center = box.mean(axis=0)
    half = min(0.2 * side, 20.0)
    s = np.arange(-half, half + 1e-9, 0.25)
    rises: list[float] = []
    per_edge: list[float | None] = []
    for i in range(4):
        a, b = box[i], box[(i + 1) % 4]
        d = (b - a) / np.linalg.norm(b - a)
        n = np.array([-d[1], d[0]])
        if np.dot((a + b) / 2 - center, n) < 0:
            n = -n  # point outward: dark (inside) -> bright (outside)
        edge_vals = []
        for t in np.linspace(0.2, 0.8, 9):
            p0 = a + t * (b - a)
            pts = p0[None, :] + s[:, None] * n[None, :]
            r = _rise_10_90(_sample(lum32, pts[:, 0], pts[:, 1]), s, min_contrast)
            if r is not None:
                edge_vals.append(r)
        per_edge.append(float(np.median(edge_vals)) if edge_vals else None)
        rises.extend(edge_vals)
    if not rises:
        return None
    h, w = img.lum.shape
    c0 = np.array([(w - 1) / 2, (h - 1) / 2])
    rn = float(np.linalg.norm(center - c0) / np.linalg.norm(c0))
    return {
        "edge_rise_px_median": float(np.median(rises)),
        "edge_rise_px_per_edge": per_edge,
        "profiles_used": len(rises),
        "square_side_px": side,
        "square_center_px": [float(center[0]), float(center[1])],
        "square_radius_norm": rn,  # 0 = image centre, 1 = image corner
        "method": "dark-square slanted-edge 10-90% rise, 9 profiles/edge, 0.25 px sampling",
    }
