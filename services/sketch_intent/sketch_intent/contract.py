"""Input contract (contour observations) and resolver parameters.

Accepted inputs, all in IMAGE PIXELS (x right, y down):

  shadowscan-outline   Outline Forge JSON written by the Tab5 ShadowScan
                       applet and PlatypusOne's session exporter (PR #29):
                       {"format": "shadowscan-outline", "units": "px",
                        "scale_mm_per_unit": <mm/px | null>,
                        "outlines": [[[x, y], ...], ...]}
  shadowscan-scan      ShadowScan Mobile SketchJson v1: raw.outer / raw.inner
                       pixel contours; scale from mmSketch.imageTransform
  platypus.contour_observation/1
                       thin native form for Engineering Scout: loops with
                       roles, the observation id, and the scale claim with
                       its provenance (docs/architecture/SKETCH_INTENT_RESOLVER.md)

Whatever the input, the original points are kept exactly as given, and every
input field the resolver does not use is carried into the proposal under
``input.carried``. A missing scale is not an error: the proposal stays in
pixels and says so.
"""

from __future__ import annotations

import hashlib
import json
import math
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any

import numpy as np

CONTOUR_SCHEMA = "platypus.contour_observation/1"
FRAME_PX = "image pixels; x right, y down; origin at the image's top-left pixel corner"


class InputError(ValueError):
    pass


@dataclass
class Loop:
    loop_id: str
    role: str  # outer | hole | unknown
    points: np.ndarray  # (N, 2) float, exactly as given
    closed: bool = True


@dataclass
class Observation:
    source_format: str
    source_sha256: str
    source_ids: dict[str, Any]
    loops: list[Loop]
    scale_mm_per_px: float | None
    calibration: dict[str, Any]
    unresolved: list[Any] = field(default_factory=list)
    carried: dict[str, Any] = field(default_factory=dict)

    @property
    def calibrated(self) -> bool:
        return self.scale_mm_per_px is not None


@dataclass(frozen=True)
class Params:
    """Every threshold the resolver uses. Recorded verbatim in each proposal.

    tolerance_px      evidence band: a primitive "fits" when every inlier point
                      lies within this orthogonal distance and the RMS is within
                      half of it. Set from the contour source's noise, NOT from
                      the answer you want.
    rms_inflation     a constraint is only "proposed" if its constrained RMS is at
                      most rms_inflation x the unconstrained RMS (or that + 0.05 x
                      tol): many points can tell apart parameters that a residual
                      band alone would let through (e.g. radii 12 vs 14 px).
    question_factor   a constraint whose constrained fit stays within
                      question_factor x tolerance is plausible intent but moves
                      geometry beyond the evidence band: it becomes a QUESTION.
                      Beyond that it is rejected by evidence.
    """

    tolerance_px: float = 2.0
    question_factor: float = 2.0
    gap_factor: float = 6.0  # spacing > gap_factor x median spacing ...
    gap_min_px: float = 6.0  # ... and > gap_min_px is a gap in the evidence
    outlier_factor: float = 3.0  # outlier: > outlier_factor x tolerance off its local line
    outlier_window: int = 5
    angle_window_deg: float = 12.0  # parallel/perpendicular hypotheses tested within this
    min_arc_span_deg: float = 20.0
    min_arc_points: int = 6
    max_radius_factor: float = 2.0  # arc radius above this x loop size -> treat as line
    min_circle_span_deg: float = 300.0  # a closed loop is a circle only if covered this far
    equal_radius_rel: float = 0.25  # equal-radius hypotheses tested within this ratio
    concentric_factor: float = 1.0  # centers within this x tolerance -> concentric candidate
    min_corner_angle_deg: float = 8.0  # flatter junctions are collinear, not corners
    rms_inflation: float = 1.25  # constrained RMS may grow to this x unconstrained (or +0.05 tol)

    def validate(self) -> None:
        for k, v in asdict(self).items():
            if (
                isinstance(v, bool)
                or not isinstance(v, int | float)
                or not math.isfinite(v)
                or v <= 0
            ):
                raise InputError(f"parameter {k} must be a positive finite number")
        if self.question_factor < 1:
            raise InputError("question_factor must be >= 1")


def _points(raw: Any, where: str) -> np.ndarray:
    if not isinstance(raw, list) or len(raw) < 3:
        raise InputError(f"{where}: a loop needs at least 3 points")
    out = []
    for i, p in enumerate(raw):
        if (
            not isinstance(p, list | tuple)
            or len(p) != 2
            or any(isinstance(v, bool) or not isinstance(v, int | float) for v in p)
            or not all(math.isfinite(float(v)) for v in p)
        ):
            raise InputError(f"{where}[{i}]: point must be two finite numbers")
        out.append((float(p[0]), float(p[1])))
    return np.array(out, dtype=np.float64)


def _area(p: np.ndarray) -> float:
    x, y = p[:, 0], p[:, 1]
    return 0.5 * float(np.dot(x, np.roll(y, -1)) - np.dot(np.roll(x, -1), y))


def _inside(pt: np.ndarray, poly: np.ndarray) -> bool:
    x, y = pt
    inside = False
    n = len(poly)
    for i in range(n):
        x1, y1 = poly[i]
        x2, y2 = poly[(i + 1) % n]
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
            inside = not inside
    return inside


def _assign_roles(arrays: list[np.ndarray]) -> list[Loop]:
    """Largest |area| is the outer loop; loops inside it are holes."""
    order = sorted(range(len(arrays)), key=lambda i: -abs(_area(arrays[i])))
    outer = order[0]
    loops = []
    for i, p in enumerate(arrays):
        if i == outer:
            role = "outer"
        elif _inside(p.mean(axis=0), arrays[outer]) or _inside(p[0], arrays[outer]):
            role = "hole"
        else:
            role = "unknown"
        loops.append(Loop(f"L{i}", role, p))
    return loops


def _scale(value: Any, where: str) -> float | None:
    if value is None:
        return None
    if (
        isinstance(value, bool)
        or not isinstance(value, int | float)
        or not math.isfinite(value)
        or value <= 0
    ):
        raise InputError(f"{where} must be a positive finite number or null")
    return float(value)


def load_observation(path: str | Path) -> Observation:
    raw_bytes = Path(path).read_bytes()
    try:
        data = json.loads(raw_bytes)
    except json.JSONDecodeError as exc:
        raise InputError(f"not JSON: {exc}") from exc
    return observation_from_dict(data, hashlib.sha256(raw_bytes).hexdigest())


def observation_from_dict(data: dict[str, Any], sha256: str = "UNKNOWN") -> Observation:
    if not isinstance(data, dict):
        raise InputError("top level must be an object")
    fmt = data.get("format") or data.get("schema")
    if fmt == "shadowscan-outline":
        if data.get("units", "px") != "px":
            raise InputError("shadowscan-outline: only units 'px' are supported")
        outlines = data.get("outlines")
        if not isinstance(outlines, list) or not outlines:
            raise InputError("shadowscan-outline: 'outlines' must be a non-empty list")
        arrays = [_points(o, f"outlines[{i}]") for i, o in enumerate(outlines)]
        scale = _scale(data.get("scale_mm_per_unit"), "scale_mm_per_unit")
        calib = {
            "status": "calibrated" if scale else "uncalibrated",
            "scale_source": "input field scale_mm_per_unit" if scale else None,
            "provenance": "UNKNOWN (shadowscan-outline carries a scale but not how it was derived)",
        }
        carried = {k: v for k, v in data.items() if k not in ("outlines",)}
        return Observation(fmt, sha256, {}, _assign_roles(arrays), scale, calib, [], carried)
    if fmt == "shadowscan-scan":
        rawj = data.get("raw")
        if not isinstance(rawj, dict):
            raise InputError("shadowscan-scan: missing 'raw'")
        arrays = [_points(rawj.get("outer"), "raw.outer")]
        arrays += [_points(p, f"raw.inner[{i}]") for i, p in enumerate(rawj.get("inner", []))]
        loops = [Loop("L0", "outer", arrays[0])]
        loops += [Loop(f"L{i + 1}", "hole", a) for i, a in enumerate(arrays[1:])]
        scale = None
        calib: dict[str, Any] = {"status": "uncalibrated", "scale_source": None, "provenance": None}
        mm = data.get("mmSketch")
        if (
            isinstance(mm, dict)
            and mm.get("units") == "MM"
            and isinstance(mm.get("imageTransform"), dict)
        ):
            scale = _scale(
                mm["imageTransform"].get("unitsPerPixel"), "mmSketch.imageTransform.unitsPerPixel"
            )
            calib = {
                "status": "calibrated",
                "scale_source": "mmSketch.imageTransform.unitsPerPixel",
                "provenance": (
                    "ShadowScan calibration (reference/paper); "
                    "method recorded by ShadowScan, not here"
                ),
            }
        carried = {"version": data.get("version"), "diagnostics": rawj.get("diagnostics", {})}
        return Observation(fmt, sha256, {}, loops, scale, calib, [], carried)
    if fmt == CONTOUR_SCHEMA:
        if data.get("units") != "px":
            raise InputError(f"{CONTOUR_SCHEMA}: units must be 'px'")
        loops_j = data.get("loops")
        if not isinstance(loops_j, list) or not loops_j:
            raise InputError(f"{CONTOUR_SCHEMA}: 'loops' must be a non-empty list")
        loops = []
        for i, lj in enumerate(loops_j):
            role = lj.get("role", "unknown")
            if role not in ("outer", "hole", "unknown"):
                raise InputError(f"loops[{i}].role must be outer, hole or unknown")
            closed = lj.get("closed", True)
            if not isinstance(closed, bool):
                raise InputError(f"loops[{i}].closed must be a boolean")
            loops.append(
                Loop(
                    str(lj.get("loop_id", f"L{i}")),
                    role,
                    _points(lj.get("points"), f"loops[{i}].points"),
                    closed,
                )
            )
        cal = data.get("calibration") or {}
        scale = _scale(cal.get("mm_per_px"), "calibration.mm_per_px")
        calib = {
            "status": "calibrated" if scale else "uncalibrated",
            "scale_source": "calibration.mm_per_px" if scale else None,
            "provenance": cal.get("provenance", "UNKNOWN"),
            "method": cal.get("method", "UNKNOWN"),
        }
        ids = {k: data[k] for k in ("observation_id", "artifact_id", "session_id") if k in data}
        carried = {k: v for k, v in data.items() if k not in ("loops", "calibration", "unresolved")}
        return Observation(
            fmt, sha256, ids, loops, scale, calib, list(data.get("unresolved", [])), carried
        )
    raise InputError(
        f"unsupported input format {fmt!r}; expected shadowscan-outline, shadowscan-scan or "
        f"{CONTOUR_SCHEMA}"
    )
