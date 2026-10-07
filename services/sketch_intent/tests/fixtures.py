"""Deterministic synthetic contours with known ground truth.

SOFTWARE TEST DATA ONLY: nothing here describes a real part or camera.

A ground-truth loop is a list of segments:
  ("line", (x0, y0), (x1, y1))
  ("arc", (cx, cy), r, a0_deg, a1_deg)     travelled from a0 to a1
  ("circle", (cx, cy), r)
Sampling walks the segments at a given point spacing, then applies the
perturbations a fixture asks for (Gaussian noise normal to nothing in
particular - isotropic, spikes, missing runs, rotation, scale).

DEV fixtures were used while writing the resolver. HELD-OUT fixtures
(``heldout_cases``) use different shapes and seeds, and most use a different
sampler (fill a mask, trace it with OpenCV: pixel staircase contours) when
OpenCV is available; they were run only after the parameters were frozen.
See docs/architecture/SKETCH_INTENT_BENCHMARK.md.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Any

import numpy as np

Seg = tuple


@dataclass
class Case:
    name: str
    loops: list[list[Seg]]  # ground-truth geometry, px
    expect: dict[str, Any]  # what a correct proposal must show
    seed: int = 0
    noise: float = 0.4
    spacing: float = 1.0
    closed: list[bool] | None = None
    spikes: int = 0
    spike_px: float = 9.0
    missing: tuple[float, float] | None = None  # fraction range of loop 0 removed
    scale_mm_per_px: float | None = 0.1
    roles: list[str] = field(default_factory=list)
    sampler: str = (
        "walk"  # walk: points along the GT; raster: fill a mask, trace its pixel boundary
    )


def _arc_pts(c, r, a0, a1, spacing):
    a0r, a1r = math.radians(a0), math.radians(a1)
    n = max(2, int(abs(a1r - a0r) * r / spacing))
    t = np.linspace(a0r, a1r, n, endpoint=False)
    return np.c_[c[0] + r * np.cos(t), c[1] + r * np.sin(t)]


def sample_loop(segs: list[Seg], spacing: float) -> np.ndarray:
    pts = []
    for s in segs:
        if s[0] == "line":
            p, q = np.array(s[1], float), np.array(s[2], float)
            n = max(2, int(np.linalg.norm(q - p) / spacing))
            t = np.arange(n)[:, None] / n
            pts.append(p + (q - p) * t)
        elif s[0] == "arc":
            pts.append(_arc_pts(s[1], s[2], s[3], s[4], spacing))
        elif s[0] == "circle":
            pts.append(_arc_pts(s[1], s[2], 0, 360, spacing))
    return np.concatenate(pts)


def raster_trace(loops: list[list[Seg]], noise_rng=None) -> list[np.ndarray] | None:
    """Fill the GT region (outer minus holes) into a 1 px mask and trace it with
    OpenCV, as a real silhouette pipeline would: pixel-quantised staircase
    contours, a different error model from ``sample_loop``. None without cv2."""
    try:
        import cv2
    except ImportError:
        return None
    polys = [sample_loop(s, 0.25) for s in loops]
    hi = np.ceil(np.max(np.concatenate(polys), axis=0)).astype(int) + 20
    mask = np.zeros((hi[1], hi[0]), np.uint8)
    shift = 4  # sub-pixel vertices
    for i, P in enumerate(polys):
        pts = np.round((P - 0.5) * (1 << shift)).astype(np.int32)
        cv2.fillPoly(mask, [pts], 255 if i == 0 else 0, lineType=cv2.LINE_8, shift=shift)
    cs, _ = cv2.findContours(mask, cv2.RETR_CCOMP, cv2.CHAIN_APPROX_NONE)
    out = [c[:, 0, :].astype(float) + 0.5 for c in cs if len(c) >= 12]
    out.sort(key=lambda c: (-abs(_shoelace(c)), float(c[:, 0].min()), float(c[:, 1].min())))
    return out


def _shoelace(p: np.ndarray) -> float:
    x, y = p[:, 0], p[:, 1]
    return 0.5 * float(np.dot(x, np.roll(y, -1)) - np.dot(np.roll(x, -1), y))


def realize(case: Case) -> tuple[dict[str, Any], list[np.ndarray]]:
    """Input JSON (shadowscan-outline or contour_observation) + clean GT samples."""
    rng = np.random.default_rng(case.seed)
    if case.sampler == "raster":
        traced = raster_trace(case.loops)
        if traced is not None:
            loops_json = [
                (T + rng.normal(0, case.noise, T.shape)).tolist() if case.noise > 0 else T.tolist()
                for T in traced
            ]
            clean = [sample_loop(s, 0.25) for s in case.loops]
            return {
                "format": "shadowscan-outline",
                "units": "px",
                "scale_mm_per_unit": case.scale_mm_per_px,
                "outlines": loops_json,
                "synthetic_sampler": "raster+trace (OpenCV)",
            }, clean
    loops_json = []
    clean = []
    for li, segs in enumerate(case.loops):
        P = sample_loop(segs, case.spacing)
        clean.append(sample_loop(segs, 0.25))
        Q = P + rng.normal(0, case.noise, P.shape) if case.noise > 0 else P.copy()
        if li == 0 and case.spikes:
            idx = np.linspace(0, len(Q) - 1, case.spikes + 2)[1:-1].astype(int) + 7
            for i in idx:
                c = Q.mean(axis=0)
                v = Q[i] - c
                Q[i] = Q[i] + v / np.linalg.norm(v) * case.spike_px
        if li == 0 and case.missing:
            a, b = (int(f * len(Q)) for f in case.missing)
            Q = np.concatenate([Q[:a], Q[b:]])
        loops_json.append(Q.tolist())
    if case.closed is not None or case.roles:
        data = {
            "schema": "platypus.contour_observation/1",
            "observation_id": f"synthetic-{case.name}",
            "units": "px",
            "loops": [
                {
                    "loop_id": f"L{i}",
                    "role": (case.roles[i] if case.roles else ("outer" if i == 0 else "hole")),
                    "closed": (case.closed[i] if case.closed else True),
                    "points": pts,
                }
                for i, pts in enumerate(loops_json)
            ],
            "calibration": {
                "mm_per_px": case.scale_mm_per_px,
                "provenance": ["synthetic"],
                "method": "synthetic fixture",
            },
            "unresolved": [{"name": "part_identity", "reason": "synthetic"}],
        }
    else:
        data = {
            "format": "shadowscan-outline",
            "units": "px",
            "scale_mm_per_unit": case.scale_mm_per_px,
            "outlines": loops_json,
        }
    return data, clean


def rot(segs: list[Seg], deg: float, about=(0.0, 0.0)) -> list[Seg]:
    a = math.radians(deg)
    R = np.array([[math.cos(a), -math.sin(a)], [math.sin(a), math.cos(a)]])
    o = np.array(about)

    def tp(p):
        return tuple((R @ (np.array(p) - o) + o).tolist())

    out = []
    for s in segs:
        if s[0] == "line":
            out.append(("line", tp(s[1]), tp(s[2])))
        elif s[0] == "arc":
            out.append(("arc", tp(s[1]), s[2], s[3] + deg, s[4] + deg))
        else:
            out.append(("circle", tp(s[1]), s[2]))
    return out


def polygon(pts) -> list[Seg]:
    return [("line", pts[i], pts[(i + 1) % len(pts)]) for i in range(len(pts))]


def rect(x, y, w, h) -> list[Seg]:
    return polygon([(x, y), (x + w, y), (x + w, y + h), (x, y + h)])


def rounded_rect(x, y, w, h, r) -> list[Seg]:
    # image coords (y down); travel clockwise on screen = increasing angle
    return [
        ("line", (x + r, y), (x + w - r, y)),
        ("arc", (x + w - r, y + r), r, -90, 0),
        ("line", (x + w, y + r), (x + w, y + h - r)),
        ("arc", (x + w - r, y + h - r), r, 0, 90),
        ("line", (x + w - r, y + h), (x + r, y + h)),
        ("arc", (x + r, y + h - r), r, 90, 180),
        ("line", (x, y + h - r), (x, y + r)),
        ("arc", (x + r, y + r), r, 180, 270),
    ]


def slot(cx, cy, length, r) -> list[Seg]:
    a, b = cx - length / 2, cx + length / 2
    return [
        ("line", (a, cy - r), (b, cy - r)),
        ("arc", (b, cy), r, -90, 90),
        ("line", (b, cy + r), (a, cy + r)),
        ("arc", (a, cy), r, 90, 270),
    ]


def dev_cases() -> list[Case]:
    C = (300.0, 300.0)
    rect_r = rot(rect(200, 240, 200, 120), 7, C)
    skew = polygon([(200, 240), (400, 240), (412.6, 360), (212.6, 360)])  # 6 deg skew
    return [
        Case(
            "noisy_rectangle",
            [rect_r],
            {"kinds": ["line"] * 4, "perpendicular": "proposed", "closed": True},
            seed=1,
        ),
        Case(
            "skewed_quadrilateral",
            [skew],
            {
                "kinds": ["line"] * 4,
                "perpendicular": "not_proposed",
                "parallel": "proposed",
                "closed": True,
                "corner_angles": [84, 96],
            },
            seed=2,
        ),
        Case(
            "rounded_rectangle",
            [rounded_rect(200, 240, 220, 130, 22)],
            {
                "kinds": ["line", "arc"] * 4,
                "perpendicular": "proposed",
                "tangent": "proposed",
                "equal_radius": "proposed",
                "closed": True,
            },
            seed=3,
            noise=0.3,
        ),
        Case(
            "circle",
            [[("circle", C, 60)]],
            {"kinds": ["circle"], "radius": 60, "closed": True},
            seed=4,
        ),
        Case(
            "partial_arc",
            [[("arc", C, 80, 20, 220)]],
            {"kinds": ["arc"], "radius": 80, "closed": False},
            seed=5,
            closed=[False],
        ),
        Case(
            "missing_segment",
            [rect(200, 240, 200, 120)],
            {"has_gap": True, "gap_vertex": "question", "closed": False},
            seed=6,
            missing=(0.04, 0.12),
        ),
        Case(
            "outliers",
            [rect(200, 240, 200, 120)],
            {"kinds": ["line"] * 4, "outliers": 4, "perpendicular": "proposed", "closed": True},
            seed=7,
            spikes=4,
        ),
        Case(
            "plate_two_holes_and_slot",
            [
                rect(150, 200, 300, 160),
                [("circle", (210, 250), 14)],
                [("circle", (390, 250), 14)],
                slot(300, 320, 80, 12),
            ],
            {
                "kinds_by_loop": [
                    ["line"] * 4,
                    ["circle"],
                    ["circle"],
                    ["line", "arc", "line", "arc"],
                ],
                "equal_radius": "proposed",
                "closed": True,
            },
            seed=8,
            noise=0.3,
        ),
        Case(
            "small_scale_rectangle",
            [rect(100, 100, 50, 30)],
            {"kinds": ["line"] * 4, "perpendicular": "proposed"},
            seed=9,
            noise=0.25,
            spacing=0.5,
            scale_mm_per_px=0.4,
        ),
        Case(
            "large_sparse_rectangle",
            [rot(rect(100, 100, 800, 480), -4, (500, 340))],
            {"kinds": ["line"] * 4, "perpendicular": "proposed"},
            seed=10,
            noise=0.6,
            spacing=3.0,
            scale_mm_per_px=0.025,
        ),
        Case(
            "freeform_blob",
            [[("line", tuple(p), tuple(q)) for p, q in _blob()]],
            {"freeform": True, "no_invented_circle": True},
            seed=11,
            noise=0.3,
        ),
        Case(
            "uncalibrated_rectangle",
            [rect(200, 240, 200, 120)],
            {"kinds": ["line"] * 4, "uncalibrated": True},
            seed=12,
            scale_mm_per_px=None,
        ),
    ]


def _blob():
    t = np.linspace(0, 2 * np.pi, 120, endpoint=False)
    r = 90 + 14 * np.sin(5 * t) + 6 * np.sin(11 * t + 1)
    pts = np.c_[300 + r * np.cos(t), 300 + r * np.sin(t)]
    return list(zip(pts, np.roll(pts, -1, axis=0), strict=True))


def heldout_cases() -> list[Case]:
    """Different shapes / seeds / perturbation levels. Run only after freezing."""
    C = (300.0, 300.0)
    hexagon = polygon(
        [
            (300 + 90 * math.cos(math.radians(a)), 300 + 90 * math.sin(math.radians(a)))
            for a in range(0, 360, 60)
        ]
    )
    L = polygon([(150, 150), (400, 150), (400, 230), (240, 230), (240, 420), (150, 420)])
    trap = polygon([(180, 250), (420, 250), (370, 370), (230, 370)])
    tri = polygon([(200, 400), (420, 400), (260, 180)])
    near_rect = polygon([(200, 240), (400, 240), (402.1, 360), (202.1, 360)])  # 1.0 deg skew
    holes4 = [rect(150, 150, 300, 200)] + [
        [("circle", (190 + 220 * i, 190 + 120 * j), 10)] for i in (0, 1) for j in (0, 1)
    ]
    return [
        Case(
            "H_hexagon",
            [rot(hexagon, 11, C)],
            {"kinds": ["line"] * 6, "perpendicular": "not_proposed", "parallel": "proposed"},
            seed=101,
            noise=0.2,
            sampler="raster",
        ),
        Case(
            "H_L_plate",
            [rot(L, -9, C)],
            {"kinds": ["line"] * 6, "perpendicular": "proposed"},
            seed=102,
            noise=0.2,
            sampler="raster",
        ),
        Case(
            "H_trapezoid",
            [trap],
            {"kinds": ["line"] * 4, "parallel_count": 1, "perpendicular": "not_proposed"},
            seed=103,
            noise=0.2,
            sampler="raster",
        ),
        Case(
            "H_triangle",
            [rot(tri, 23, C)],
            {"kinds": ["line"] * 3, "perpendicular": "not_proposed", "parallel": "not_proposed"},
            seed=104,
            noise=0.2,
            sampler="raster",
        ),
        Case(
            "H_near_rectangle_1deg",
            [near_rect],
            {"kinds": ["line"] * 4, "perpendicular": "question_or_proposed"},
            seed=105,
            noise=0.2,
            sampler="raster",
        ),
        Case(
            "H_slot_plate",
            [rect(150, 200, 300, 160), slot(300, 280, 120, 18)],
            {
                "kinds_by_loop": [["line"] * 4, ["line", "arc", "line", "arc"]],
                "equal_radius": "proposed",
            },
            seed=106,
            noise=0.2,
            sampler="raster",
        ),
        Case(
            "H_four_equal_holes",
            holes4,
            {"equal_radius": "proposed", "kinds_by_loop": [["line"] * 4] + [["circle"]] * 4},
            seed=107,
            noise=0.2,
            sampler="raster",
        ),
        Case(
            "H_ellipse",
            [[("line", tuple(p), tuple(q)) for p, q in _ellipse()]],
            {"no_invented_circle": True},
            seed=108,
            noise=0.2,
            sampler="raster",
        ),
        Case(
            "H_rounded_rect_small_r",
            [rot(rounded_rect(200, 240, 220, 130, 5), 3, C)],
            {"closed": True},
            seed=109,
            noise=0.2,
            sampler="raster",
        ),
        Case(
            "H_noisy_rect_sigma1",
            [rot(rect(200, 240, 200, 120), 15, C)],
            {"kinds": ["line"] * 4},
            seed=110,
            noise=1.0,
        ),
    ]


def _ellipse():
    t = np.linspace(0, 2 * np.pi, 160, endpoint=False)
    pts = np.c_[300 + 110 * np.cos(t), 300 + 80 * np.sin(t)]
    return list(zip(pts, np.roll(pts, -1, axis=0), strict=True))
