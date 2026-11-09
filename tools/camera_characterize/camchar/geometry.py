"""Centre vs edge geometry on ChArUco frames.

Per frame (any scene containing the ChArUco target):
  reprojection   pose from solvePnP with the calibrated intrinsics; residual
                 per corner, split by radial zone
  line residual  RMS perpendicular distance of each corner row/column from its
                 best-fit line: as captured (shows distortion) and after
                 undistortion with the calibration (what remains)
  planar residual  homography from undistorted pixels to the board plane;
                 residual in mm. The scale comes from the board itself, so
                 this is SHAPE consistency in mm, not absolute accuracy
                 (that needs independent truth; see repeatability.py)

Zones use the radius from the principal point normalised by the distance to
the farthest image corner: centre < 1/3, edge >= 2/3.
"""

from __future__ import annotations

from typing import Any

import cv2
import numpy as np

from .calibration import Board, Detection

CENTER_MAX = 1 / 3
EDGE_MIN = 2 / 3
MIN_LINE_POINTS = 4


def _zones(pts: np.ndarray, pp: np.ndarray, size: tuple[int, int]) -> np.ndarray:
    w, h = size
    corners = np.array([[0, 0], [w - 1, 0], [0, h - 1], [w - 1, h - 1]], np.float64)
    rmax = float(np.max(np.linalg.norm(corners - pp, axis=1)))
    return np.linalg.norm(pts - pp, axis=1) / rmax


def _rms(v: np.ndarray) -> float | None:
    return float(np.sqrt(np.mean(v * v))) if len(v) else None


def _by_zone(values: np.ndarray, rn: np.ndarray) -> dict[str, Any]:
    c = values[rn < CENTER_MAX]
    e = values[rn >= EDGE_MIN]
    out = {
        "center_rms": _rms(c),
        "edge_rms": _rms(e),
        "all_rms": _rms(values),
        "n_center": int(len(c)),
        "n_edge": int(len(e)),
    }
    if out["center_rms"] and out["edge_rms"] is not None:
        out["edge_to_center"] = out["edge_rms"] / out["center_rms"]
    else:
        out["edge_to_center"] = None
    return out


def _line_residuals(pts: np.ndarray, grid: np.ndarray, axis: int) -> list[tuple[float, np.ndarray]]:
    """[(rms_px, member_index_array)] for each row (axis=1) or column (axis=0)."""
    out = []
    for k in np.unique(grid[:, axis]):
        idx = np.where(grid[:, axis] == k)[0]
        if len(idx) < MIN_LINE_POINTS:
            continue
        p = pts[idx]
        c = p.mean(axis=0)
        _, _, vt = np.linalg.svd(p - c)
        normal = vt[1]
        d = (p - c) @ normal
        out.append((float(np.sqrt(np.mean(d * d))), idx))
    return out


def _lines_by_zone(pts: np.ndarray, grid: np.ndarray, rn: np.ndarray) -> dict[str, Any]:
    lines = _line_residuals(pts, grid, 0) + _line_residuals(pts, grid, 1)
    if not lines:
        return {"lines": 0}
    mean_rn = np.array([rn[idx].mean() for _, idx in lines])
    vals = np.array([v for v, _ in lines])
    c, e = vals[mean_rn < CENTER_MAX], vals[mean_rn >= EDGE_MIN]
    return {
        "lines": len(lines),
        "all_mean_px": float(vals.mean()),
        "all_max_px": float(vals.max()),
        "center_mean_px": float(c.mean()) if len(c) else None,
        "edge_mean_px": float(e.mean()) if len(e) else None,
        "n_center_lines": int(len(c)),
        "n_edge_lines": int(len(e)),
    }


def analyze_frame(
    det: Detection,
    board: Board,
    size: tuple[int, int],
    K: np.ndarray | None,
    dist: np.ndarray | None,
) -> dict[str, Any]:
    grid = det.grid(board)
    w, h = size
    pp = np.array([K[0, 2], K[1, 2]]) if K is not None else np.array([(w - 1) / 2, (h - 1) / 2])
    rn = _zones(det.img, pp, size)
    out: dict[str, Any] = {
        "corners": int(len(det.ids)),
        "corner_radius_norm_max": float(rn.max()),
        "line_residual_as_captured": _lines_by_zone(det.img, grid, rn),
    }
    if K is None or dist is None:
        out["calibrated"] = False
        return out
    out["calibrated"] = True
    ok, rvec, tvec = cv2.solvePnP(det.obj, det.img, K, dist, flags=cv2.SOLVEPNP_ITERATIVE)
    if ok:
        proj, _ = cv2.projectPoints(det.obj, rvec, tvec, K, dist)
        res = np.linalg.norm(proj.reshape(-1, 2) - det.img, axis=1)
        out["reprojection_px"] = _by_zone(res, rn)
    else:
        out["reprojection_px"] = None
    und = cv2.undistortPoints(det.img.reshape(-1, 1, 2), K, dist, P=K).reshape(-1, 2)
    out["line_residual_undistorted"] = _lines_by_zone(und, grid, rn)
    if len(und) >= 8:
        H, _ = cv2.findHomography(und, det.obj[:, :2], 0)
        if H is not None:
            mapped = cv2.perspectiveTransform(und.reshape(-1, 1, 2), H).reshape(-1, 2)
            res_mm = np.linalg.norm(mapped - det.obj[:, :2], axis=1)
            out["planar_residual_mm"] = _by_zone(res_mm, rn)
    return out
