"""ChArUco intrinsic calibration (OpenCV pinhole + k1 k2 p1 p2 k3).

Deterministic given the same frames and OpenCV version. No frame is dropped
after the fact to improve the RMS: a view is either detected (enough corners)
or rejected with a reason, and every per-view error is reported so a human
can judge outliers.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

import cv2
import numpy as np

MODEL = "opencv_pinhole_k1k2p1p2k3"
MIN_CORNERS_PER_VIEW = 12
COVERAGE_GRID = (8, 6)  # cells across, down


@dataclass
class Board:
    squares: tuple[int, int]  # cols, rows
    square_mm: float
    marker_to_square: float
    dictionary: str

    def build(self) -> cv2.aruco.CharucoBoard:
        dic = cv2.aruco.getPredefinedDictionary(getattr(cv2.aruco, self.dictionary))
        return cv2.aruco.CharucoBoard(
            self.squares, self.square_mm, self.square_mm * self.marker_to_square, dic
        )

    @classmethod
    def from_target(cls, t: dict[str, Any]) -> Board:
        return cls(
            (int(t["squares"][0]), int(t["squares"][1])),
            float(t["square_mm"]),
            float(t["marker_to_square"]),
            str(t["dictionary"]),
        )

    @property
    def inner(self) -> tuple[int, int]:
        return self.squares[0] - 1, self.squares[1] - 1


@dataclass
class Detection:
    ids: np.ndarray  # (N,) charuco corner ids
    img: np.ndarray  # (N, 2) pixel coordinates
    obj: np.ndarray  # (N, 3) board coordinates, mm (nominal square size)

    def grid(self, board: Board) -> np.ndarray:
        """(N, 2) integer (col, row) of each inner corner."""
        cols = board.inner[0]
        return np.stack([self.ids % cols, self.ids // cols], axis=1)


def detect(gray8: np.ndarray, board: Board) -> tuple[Detection | None, str]:
    b = board.build()
    detector = cv2.aruco.CharucoDetector(b)
    corners, ids, _, marker_ids = detector.detectBoard(gray8)
    if ids is None or len(ids) == 0:
        n_markers = 0 if marker_ids is None else len(marker_ids)
        return None, f"no ChArUco corners ({n_markers} markers)"
    if len(ids) < MIN_CORNERS_PER_VIEW:
        return None, f"only {len(ids)} corners (< {MIN_CORNERS_PER_VIEW})"
    obj, img = b.matchImagePoints(corners, ids)
    return (
        Detection(
            ids.ravel().astype(int), img.reshape(-1, 2).astype(np.float64), obj.reshape(-1, 3)
        ),
        "ok",
    )


def coverage(points: list[np.ndarray], size: tuple[int, int]) -> dict[str, float]:
    w, h = size
    gx, gy = COVERAGE_GRID
    hit = np.zeros((gy, gx), bool)
    c0 = np.array([(w - 1) / 2, (h - 1) / 2])
    rmax = float(np.linalg.norm(c0))
    max_r = 0.0
    for p in points:
        cx = np.clip((p[:, 0] / w * gx).astype(int), 0, gx - 1)
        cy = np.clip((p[:, 1] / h * gy).astype(int), 0, gy - 1)
        hit[cy, cx] = True
        max_r = max(max_r, float(np.max(np.linalg.norm(p - c0, axis=1)) / rmax))
    return {"grid_cells_hit_fraction": float(hit.mean()), "max_corner_radius_norm": max_r}


def calibrate(
    detections: list[tuple[str, Detection]], size: tuple[int, int], min_views: int
) -> dict[str, Any]:
    """Returns a JSON-able result; status is ok / insufficient / failed."""
    result: dict[str, Any] = {"model": MODEL, "image_size": list(size), "views": len(detections)}
    if len(detections) < min_views:
        result["status"] = "insufficient"
        result["reason"] = f"{len(detections)} accepted views < {min_views} required"
        return result
    obj = [d.obj.astype(np.float32) for _, d in detections]
    img = [d.img.astype(np.float32).reshape(-1, 1, 2) for _, d in detections]
    try:
        rms, K, dist, rvecs, tvecs, std_i, _std_e, per_view = cv2.calibrateCameraExtended(
            obj, img, size, None, None
        )
    except cv2.error as exc:
        result["status"] = "failed"
        result["reason"] = f"calibrateCameraExtended: {exc.msg if hasattr(exc, 'msg') else exc}"
        return result
    dist = dist.ravel()
    std_i = std_i.ravel()
    result.update(
        {
            "status": "ok",
            "rms_reprojection_px": float(rms),
            "camera_matrix": K.tolist(),
            "distortion": {
                k: float(v) for k, v in zip(("k1", "k2", "p1", "p2", "k3"), dist[:5], strict=True)
            },
            "std_dev": {
                k: float(v)
                for k, v in zip(
                    ("fx", "fy", "cx", "cy", "k1", "k2", "p1", "p2", "k3"), std_i[:9], strict=True
                )
            },
            "per_view": [
                {"frame": ref, "rms_px": float(e), "corners": int(len(d.ids))}
                for (ref, d), e in zip(detections, per_view.ravel(), strict=True)
            ],
            "coverage": coverage([d.img for _, d in detections], size),
            "distortion_at_image_points": distortion_profile(K, dist, size),
        }
    )
    result["_rvecs"] = [r.ravel().tolist() for r in rvecs]
    result["_tvecs"] = [t.ravel().tolist() for t in tvecs]
    return result


def distortion_profile(K: np.ndarray, dist: np.ndarray, size: tuple[int, int]) -> dict[str, Any]:
    """How far the model moves image-corner and edge-midpoint pixels.

    Displacement = |undistorted - observed| in px, and as % of the undistorted
    point's distance from the principal point. This is model-derived from OUR
    calibration; it is not the vendor's lens-distortion figure (different
    definition, different lens sample).
    """
    w, h = size
    pts = {
        "corner_top_left": (0, 0),
        "corner_top_right": (w - 1, 0),
        "corner_bottom_left": (0, h - 1),
        "corner_bottom_right": (w - 1, h - 1),
        "edge_mid_left": (0, (h - 1) / 2),
        "edge_mid_right": (w - 1, (h - 1) / 2),
        "edge_mid_top": ((w - 1) / 2, 0),
        "edge_mid_bottom": ((w - 1) / 2, h - 1),
    }
    arr = np.array(list(pts.values()), np.float64).reshape(-1, 1, 2)
    und = cv2.undistortPoints(arr, K, dist, P=K).reshape(-1, 2)
    pp = np.array([K[0, 2], K[1, 2]])
    out: dict[str, Any] = {}
    for (name, p), u in zip(pts.items(), und, strict=True):
        disp = float(np.linalg.norm(u - np.array(p)))
        r = float(np.linalg.norm(u - pp))
        out[name] = {"displacement_px": disp, "displacement_pct": 100 * disp / r if r else None}
    corners = [v["displacement_pct"] for k, v in out.items() if k.startswith("corner")]
    out["max_corner_displacement_pct"] = float(max(c for c in corners if c is not None))
    return out


def undistort_preview(gray8: np.ndarray, K: np.ndarray, dist: np.ndarray) -> np.ndarray:
    """Side-by-side: as captured | undistorted (same camera matrix)."""
    und = cv2.undistort(gray8, K, dist)
    sep = np.full((gray8.shape[0], 8), 128, np.uint8)
    return np.hstack([gray8, sep, und])
