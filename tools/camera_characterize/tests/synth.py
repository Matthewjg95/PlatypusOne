"""Synthetic fixtures: planar scenes rendered through a KNOWN camera model.

SOFTWARE TEST DATA ONLY. Every manifest written here says
evidence_type = "synthetic_fixture"; the decision gate refuses to assign a
camera role from it. Nothing here describes any real camera.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import cv2
import numpy as np
from camchar import UNKNOWN
from camchar.calibration import Board
from camchar.contract import sha256_file
from camchar.target import raster

SIZE = (820, 616)
BOARD = Board((7, 10), 20.0, 0.7, "DICT_4X4_50")
TARGETS: dict[str, Any] = {
    "charuco": {
        "kind": "charuco",
        "squares": [7, 10],
        "square_mm": 20.0,
        "square_mm_status": "verified",
        "marker_to_square": 0.7,
        "dictionary": "DICT_4X4_50",
        "source": "synthetic",
    },
    "ref20": {
        "kind": "scout_reference",
        "reference_mm": 20.0,
        "reference_mm_status": "verified",
        "source": "synthetic",
    },
    "bar": {
        "kind": "planar_part",
        "description": "synthetic 40 x 8 mm bar",
        "reference_target": "ref20",
        "truth": {
            "length_mm": 40.0,
            "width_mm": 8.0,
            "status": "verified",
            "source": "synthetic geometry",
        },
    },
}


class Camera:
    def __init__(
        self, fx: float, k1: float, k2: float = 0.0, blur_per_m: float = 0.0, focus_m: float = 0.3
    ):
        w, h = SIZE
        self.K = np.array(
            [[fx, 0, (w - 1) / 2 + 3.0], [0, fx, (h - 1) / 2 - 2.0], [0, 0, 1]], np.float64
        )
        self.dist = np.array([k1, k2, 0.0005, -0.0003, 0.0])
        self.blur_per_m = blur_per_m
        self.focus_m = focus_m
        w, h = SIZE
        u, v = np.meshgrid(np.arange(w, dtype=np.float64), np.arange(h, dtype=np.float64))
        pts = np.stack([u.ravel(), v.ravel()], axis=1).reshape(-1, 1, 2)
        crit = (cv2.TERM_CRITERIA_COUNT | cv2.TERM_CRITERIA_EPS, 100, 1e-12)
        self._rays = cv2.undistortPointsIter(pts, self.K, self.dist, None, None, crit).reshape(
            -1, 2
        )

    def render(
        self,
        texture: np.ndarray,
        px_per_mm: float,
        margin_mm: float,
        rvec,
        tvec_mm,
        seed: int = 0,
        noise: float = 1.0,
    ) -> np.ndarray:
        """Image of a planar texture (texture px -> plane mm via px_per_mm, margin)."""
        R, _ = cv2.Rodrigues(np.asarray(rvec, np.float64))
        M = np.column_stack([R[:, 0], R[:, 1], np.asarray(tvec_mm, np.float64)])
        Minv = np.linalg.inv(M)
        rays = np.column_stack([self._rays, np.ones(len(self._rays))])
        plane = rays @ Minv.T
        X = plane[:, 0] / plane[:, 2]
        Y = plane[:, 1] / plane[:, 2]
        # Texture pixel j covers [j, j+1) / px_per_mm, centre at (j + 0.5): remap
        # samples pixel centres at integer coordinates, hence the -0.5.
        mapx = ((X + margin_mm) * px_per_mm - 0.5).astype(np.float32).reshape(SIZE[1], SIZE[0])
        mapy = ((Y + margin_mm) * px_per_mm - 0.5).astype(np.float32).reshape(SIZE[1], SIZE[0])
        img = cv2.remap(
            texture, mapx, mapy, cv2.INTER_LINEAR, borderMode=cv2.BORDER_CONSTANT, borderValue=235
        )
        img = img.astype(np.float64) * 0.82 + 20  # paper/ink contrast, not pure 0/255
        z_m = float(tvec_mm[2]) / 1000.0
        sigma = self.blur_per_m * abs(z_m - self.focus_m)
        if sigma > 0.05:
            img = cv2.GaussianBlur(img, (0, 0), sigma)
        rng = np.random.default_rng(seed)
        img = img + rng.normal(0, noise, img.shape)
        return np.clip(np.round(img), 0, 255).astype(np.uint8)


def scout_texture(px_per_mm: float = 8.0) -> tuple[np.ndarray, float, float]:
    """White sheet 120 x 80 mm: 20 mm square at left, 40 x 8 mm bar at right."""
    margin = 0.0
    W, H = int(120 * px_per_mm), int(80 * px_per_mm)
    t = np.full((H, W), 245, np.uint8)

    def rect(x, y, w, h):
        t[
            int(y * px_per_mm) : int((y + h) * px_per_mm),
            int(x * px_per_mm) : int((x + w) * px_per_mm),
        ] = 15

    rect(15, 30, 20, 20)
    rect(60, 36, 40, 8)
    return t, px_per_mm, margin


def _sheet_pose(
    dist_mm: float, offset_px: tuple[float, float], cam: Camera, sheet_w=120.0, sheet_h=80.0
):
    """Fronto-parallel pose placing the sheet centre at an image offset from the principal point."""
    fx = cam.K[0, 0]
    tx = offset_px[0] * dist_mm / fx - sheet_w / 2
    ty = offset_px[1] * dist_mm / fx - sheet_h / 2
    return [0.0, 0.0, 0.004], [tx, ty, dist_mm]


def _calib_poses(n: int) -> list[tuple[list[float], list[float]]]:
    rng = np.random.default_rng(1234)
    poses = []
    for i in range(n):
        rx, ry = rng.uniform(-0.45, 0.45, 2)
        rz = rng.uniform(-0.2, 0.2)
        z = rng.uniform(330, 420)
        # Alternate the four image corners so corners reach the image edge.
        cx_, cy_ = [(-235, -185), (95, -185), (-235, -15), (95, -15)][i % 4]
        tx = cx_ + rng.uniform(-15, 15)
        ty = cy_ + rng.uniform(-15, 15)
        poses.append(([rx, ry, rz], [tx, ty, z]))
    return poses


def base_manifest(dataset_id: str, sku: str) -> dict[str, Any]:
    return {
        "schema": "platypus.camera_dataset/1",
        "dataset_id": dataset_id,
        "evidence_type": "synthetic_fixture",
        "camera": {
            "sku": sku,
            "sensor": "SYNTHETIC",
            "silkscreen": UNKNOWN,
            "revision": UNKNOWN,
            "unit_id": "synthetic",
            "lens": "synthetic pinhole+distortion",
            "focus_type": "fixed",
        },
        "platform": {
            "host": "synthetic",
            "repo_sha": UNKNOWN,
            "kernel": UNKNOWN,
            "image": UNKNOWN,
            "csi_port": UNKNOWN,
            "enumeration": UNKNOWN,
            "capture_stack": "synthetic render",
        },
        "bringup": {
            "enumerates": True,
            "reboot_repeat": True,
            "dsi_coexistence": True,
            "evidence": "synthetic",
        },
        "physical": {
            "board_w_mm": {"value": 25.0, "status": "measured", "source": "synthetic"},
            "board_h_mm": {"value": 24.0, "status": "measured", "source": "synthetic"},
            "lens_stack_mm": {"value": 9.0, "status": "measured", "source": "synthetic"},
        },
        "targets": TARGETS,
        "defaults": {
            "capture_command": "synthetic",
            "pixel_format": "png",
            "frame_rate": UNKNOWN,
            "exposure_us": UNKNOWN,
            "analogue_gain": UNKNOWN,
            "white_balance": UNKNOWN,
            "focus_mode": "fixed",
            "focus_position": UNKNOWN,
            "timestamp_utc": UNKNOWN,
        },
        "frames": [],
    }


def make_dataset(
    root: Path,
    dataset_id: str,
    sku: str,
    cam: Camera,
    *,
    calib_sets=("A", "B"),
    n_calib: int = 12,
    distances=(250, 400),
    repeats: int = 8,
) -> Path:
    d = root / "datasets" / dataset_id
    (d / "frames").mkdir(parents=True)
    m = base_manifest(dataset_id, sku)
    board_px_mm = 6.0
    board_tex = raster(BOARD, board_px_mm, margin_mm=15.0)
    sc_tex, sc_ppm, sc_margin = scout_texture()

    def save(fid: str, img: np.ndarray, **meta):
        rel = f"frames/{fid}.png"
        cv2.imwrite(str(d / rel), img)
        m["frames"].append(
            {
                "frame_id": fid,
                "path": rel,
                "sha256": sha256_file(d / rel),
                "format": "png",
                "width": SIZE[0],
                "height": SIZE[1],
                **meta,
            }
        )

    seed = 0
    for cset in calib_sets:
        poses = _calib_poses(n_calib)
        for i, (r, t) in enumerate(poses):
            seed += 1
            img = cam.render(board_tex, board_px_mm, 15.0, r, t, seed=seed)
            save(
                f"calib-{cset}-{i:02d}",
                img,
                scene="calibration",
                placement="varied",
                lighting="even",
                targets=["charuco"],
                working_distance_mm=350,
                calibration_set=cset,
                mount_id=f"mount-{cset}",
            )
    for dist in distances:
        for placement, off in (("center", (0, 0)), ("edge", (-250, -170))):
            seed += 1
            r, t = _sheet_pose(dist, off, cam)
            save(
                f"ref-{dist}-{placement}",
                cam.render(sc_tex, sc_ppm, sc_margin, r, t, seed=seed),
                scene="dimensional_reference",
                placement=placement,
                lighting="even",
                targets=["ref20", "bar"],
                working_distance_mm=dist,
            )
        for placement, t in (("center", [-70.0, -100.0, 380.0]), ("edge", [-245.0, -190.0, 380.0])):
            seed += 1
            r = [0.02, -0.03, 0.01]
            save(
                f"geo-{dist}-{placement}",
                cam.render(board_tex, board_px_mm, 15.0, r, t, seed=seed),
                scene="geometry",
                placement=placement,
                lighting="even",
                targets=["charuco"],
                working_distance_mm=dist,
            )
    rep_dist = distances[0]
    r, t = _sheet_pose(rep_dist, (0, 0), cam)
    for i in range(repeats):
        seed += 1
        save(
            f"rep-{i:02d}",
            cam.render(sc_tex, sc_ppm, sc_margin, r, t, seed=seed),
            scene="repeatability",
            placement="center",
            lighting="even",
            targets=["ref20", "bar"],
            working_distance_mm=rep_dist,
            repeat_series="rep",
            repeat_index=i + 1,
        )
    seed += 1
    save(
        "light-low",
        (cam.render(sc_tex, sc_ppm, sc_margin, r, t, seed=seed) * 0.35).astype(np.uint8),
        scene="lighting",
        placement="center",
        lighting="low",
        targets=["ref20", "bar"],
        working_distance_mm=rep_dist,
    )
    m["frames"].append(
        {
            "frame_id": "failed-01",
            "outcome": "capture_failed",
            "failure_reason": "synthetic failure record",
            "scene": "other",
            "placement": UNKNOWN,
            "lighting": UNKNOWN,
            "targets": [],
            "working_distance_mm": UNKNOWN,
        }
    )
    (d / "dataset.json").write_text(json.dumps(m, indent=2) + "\n")
    return d


def make_experiment(root: Path, cameras: dict[str, Camera], **kw) -> Path:
    root.mkdir(parents=True, exist_ok=True)
    rels = []
    for sku, cam in cameras.items():
        make_dataset(root, f"syn-{sku.lower()}", sku, cam, **kw)
        rels.append(f"datasets/syn-{sku.lower()}")
    exp = {
        "schema": "platypus.camera_experiment/1",
        "experiment_id": "synthetic-regression",
        "evidence_type": "synthetic_fixture",
        "representative_distance_mm": kw.get("distances", (250, 400))[0],
        "datasets": rels,
    }
    (root / "experiment.json").write_text(json.dumps(exp, indent=2) + "\n")
    return root
