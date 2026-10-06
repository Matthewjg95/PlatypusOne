"""Run every analysis over an experiment and write derived results.

Guarantees:
  - refuses to analyse when the experiment or a dataset has validation ERRORs;
    frame-level errors exclude that frame and are listed in the output;
  - derived/<ANALYSIS_VERSION>/ is deleted and rebuilt on every run;
  - source frames are opened read-only, and re-hashed after the run; any
    change aborts with an integrity error.
"""

from __future__ import annotations

import json
import shutil
from collections import defaultdict
from pathlib import Path
from typing import Any

import cv2
import numpy as np

from . import ANALYSIS_VERSION, UNKNOWN, geometry, images, metrics, repeatability
from . import calibration as cal
from .contract import (
    Dataset,
    Frame,
    dimension_verified,
    load_experiment,
    sha256_file,
    validation_summary,
)


class AnalysisRefused(Exception):
    pass


def derived_dir(exp_root: Path) -> Path:
    return exp_root.resolve() / "derived" / ANALYSIS_VERSION


def _targets_of(ds: Dataset, fr: Frame, kind: str) -> list[tuple[str, dict[str, Any]]]:
    tl = fr.get("targets")
    if not isinstance(tl, list):
        return []
    return [(t, ds.targets[t]) for t in tl if ds.targets.get(t, {}).get("kind") == kind]


def _focus_state(fr: Frame) -> tuple[str, str]:
    return str(fr.get("focus_mode")), str(fr.get("focus_position"))


def _json_default(o: Any) -> Any:
    if isinstance(o, np.floating):
        return float(o)
    if isinstance(o, np.integer):
        return int(o)
    if isinstance(o, np.ndarray):
        return o.tolist()
    raise TypeError(type(o))


def write_json(path: Path, obj: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, indent=2, sort_keys=True, default=_json_default) + "\n")


def analyze_dataset(
    ds: Dataset, out: Path, scout_tool: Path | None, min_views: int
) -> dict[str, Any]:
    out.mkdir(parents=True, exist_ok=True)
    work = out / "work"
    work.mkdir(exist_ok=True)
    frames: dict[str, dict[str, Any]] = {}
    loaded: dict[str, images.Loaded] = {}
    detections: dict[str, tuple[cal.Board, cal.Detection]] = {}
    board_reject: dict[str, str] = {}

    for fr in ds.frames:
        if not fr.usable:
            continue
        img = images.load(fr)
        loaded[fr.frame_id] = img
        rec: dict[str, Any] = {
            "frame": fr.ref,
            "scene": fr.get("scene"),
            "placement": fr.get("placement"),
            "lighting": fr.get("lighting"),
            "working_distance_mm": fr.get("working_distance_mm"),
            "format": fr.get("format"),
            "size": [int(fr.get("width")), int(fr.get("height"))],
            "focus_mode": fr.get("focus_mode"),
            "focus_position": fr.get("focus_position"),
            "exposure": metrics.exposure(img),
            "region_sharpness": metrics.region_sharpness(img),
        }
        if _targets_of(ds, fr, "scout_reference"):
            rec["edge_rise"] = metrics.edge_rise(img)
        for _tid, t in _targets_of(ds, fr, "charuco"):
            board = cal.Board.from_target(t)
            det, why = cal.detect(img.gray8, board)
            rec["charuco"] = {"detected": det is not None, "reason": why}
            if det is not None:
                detections[fr.frame_id] = (board, det)
                rec["charuco"]["corners"] = int(len(det.ids))
            else:
                board_reject[fr.frame_id] = why
        frames[fr.frame_id] = rec

    by_id = {f.frame_id: f for f in ds.frames}

    # --- calibration groups ---------------------------------------------------
    groups: dict[tuple[str, int, int, str], list[str]] = defaultdict(list)
    for fid, rec in frames.items():
        fr = by_id[fid]
        if fr.get("scene") == "calibration" and "charuco" in rec:
            key = (str(fr.get("calibration_set", "default")), *rec["size"], str(fr.get("format")))
            groups[key].append(fid)
    calibrations: list[dict[str, Any]] = []
    cal_models: dict[tuple[str, int, int, str], tuple[np.ndarray, np.ndarray, tuple[str, str]]] = {}
    for key, fids in sorted(groups.items()):
        cset, w, h, fmt = key
        states = {_focus_state(by_id[f]) for f in fids}
        mounts = {str(by_id[f].get("mount_id", UNKNOWN)) for f in fids}
        entry: dict[str, Any] = {
            "dataset_id": ds.dataset_id,
            "calibration_set": cset,
            "mount_id": next(iter(mounts)) if len(mounts) == 1 else sorted(mounts),
            "size": [w, h],
            "format": fmt,
            "frames_offered": [by_id[f].ref for f in fids],
            "frames_rejected": {by_id[f].ref: board_reject[f] for f in fids if f in board_reject},
            "focus_states": sorted(list(s) for s in states),
        }
        if len(states) != 1:
            entry["status"] = "invalid"
            entry["reason"] = "focus state changes within one calibration set"
            calibrations.append(entry)
            continue
        if len(mounts) != 1:
            entry["status"] = "invalid"
            entry["reason"] = "mount_id changes within one calibration set"
            calibrations.append(entry)
            continue
        accepted = [(by_id[f].ref, detections[f][1]) for f in fids if f in detections]
        res = cal.calibrate(accepted, (w, h), min_views)
        res.pop("_rvecs", None)
        res.pop("_tvecs", None)
        entry.update(res)
        if res["status"] == "ok":
            K = np.array(res["camera_matrix"])
            dist = np.array([res["distortion"][k] for k in ("k1", "k2", "p1", "p2", "k3")])
            cal_models[key] = (K, dist, next(iter(states)))
            first = next(f for f in fids if f in detections)
            prev = cal.undistort_preview(loaded[first].gray8, K, dist)
            name = f"undistort_preview_{cset}_{w}x{h}.png"
            cv2.imwrite(str(out / name), prev)
            entry["undistort_preview"] = name
            entry["undistort_preview_source"] = by_id[first].ref
        calibrations.append(entry)

    def model_for(fr: Frame, size: list[int]):
        """Calibration for this frame: same set (if named), size, format, focus state."""
        want_set = fr.get("calibration_set", None)
        for (cset, w, h, fmt), (K, dist, state) in sorted(cal_models.items()):
            if [w, h] != size or fmt != fr.get("format") or state != _focus_state(fr):
                continue
            if want_set not in (None, UNKNOWN) and cset != want_set:
                continue
            return (cset, K, dist)
        return None

    # --- geometry ---------------------------------------------------------------
    for fid, (board, det) in detections.items():
        rec = frames[fid]
        m = model_for(by_id[fid], rec["size"])
        g = geometry.analyze_frame(
            det, board, tuple(rec["size"]), m[1] if m else None, m[2] if m else None
        )
        g["calibration_set"] = m[0] if m else None
        rec["geometry"] = g

    # --- Scout measurements -----------------------------------------------------
    for fid, rec in frames.items():
        fr = by_id[fid]
        refs = _targets_of(ds, fr, "scout_reference")
        if not refs:
            continue
        ref_mm = float(refs[0][1]["reference_mm"])
        rec["scout_reference_verified"] = dimension_verified(refs[0][1], "reference_mm")
        if scout_tool is None:
            rec["scout"] = {"as_captured": {"ok": False, "error": "scout_measure unavailable"}}
            continue
        pgm = work / f"{fid}.pgm"
        images.write_pgm(pgm, loaded[fid].gray8)
        rec["scout"] = {"as_captured": repeatability.run_scout(scout_tool, pgm, ref_mm)}
        m = model_for(fr, rec["size"])
        if m:
            und = cv2.undistort(loaded[fid].gray8, m[1], m[2])
            pgm_u = work / f"{fid}.undistorted.pgm"
            images.write_pgm(pgm_u, und)
            rec["scout"]["undistorted"] = repeatability.run_scout(scout_tool, pgm_u, ref_mm)
            rec["scout"]["undistorted_calibration_set"] = m[0]

    # --- repeat series ----------------------------------------------------------
    series: dict[str, list[str]] = defaultdict(list)
    for fr in ds.frames:
        rs = fr.get("repeat_series", None)
        if rs not in (None, UNKNOWN):
            series[str(rs)].append(fr.frame_id)
    series_out: dict[str, Any] = {}
    for name, fids in sorted(series.items()):
        usable = [f for f in fids if f in frames]
        entry: dict[str, Any] = {
            "frames_listed": len(fids),
            "frames_unusable": [by_id[f].ref for f in fids if f not in frames],
            "working_distance_mm": sorted({str(by_id[f].get("working_distance_mm")) for f in fids}),
        }
        truth = _truth_for(ds, [by_id[f] for f in usable])
        for variant in ("as_captured", "undistorted"):
            rows = [
                {"frame": by_id[f].ref, "measurement": frames[f]["scout"][variant]}
                for f in usable
                if variant in frames[f].get("scout", {})
            ]
            if rows:
                entry[variant] = repeatability.summarize(rows, truth)
        rises = [
            frames[f]["edge_rise"]["edge_rise_px_median"]
            for f in usable
            if frames[f].get("edge_rise")
        ]
        entry["edge_rise_px"] = repeatability.series_stats(rises)
        lums = [loaded[f].lum for f in usable]
        if lums:
            entry["temporal_noise"] = repeatability.temporal_noise(
                lums, loaded[usable[0]].native_full_scale
            )
        series_out[name] = entry

    # --- focus envelope (edge rise vs distance and placement) ------------------
    env: dict[tuple[str, str], list[float]] = defaultdict(list)
    env_frames: dict[tuple[str, str], list[str]] = defaultdict(list)
    for rec in frames.values():
        er = rec.get("edge_rise")
        if er:
            k = (str(rec["working_distance_mm"]), str(rec["placement"]))
            env[k].append(er["edge_rise_px_median"])
            env_frames[k].append(rec["frame"])
    envelope = [
        {
            "working_distance_mm": k[0],
            "placement": k[1],
            "edge_rise_px_median": float(np.median(v)),
            "n": len(v),
            "frames": env_frames[k],
        }
        for k, v in sorted(env.items(), key=lambda kv: (_num(kv[0][0]), kv[0][1]))
    ]

    # --- lighting robustness (Scout on each lighting condition) ----------------
    lighting: dict[str, dict[str, Any]] = {}
    for rec in frames.values():
        if "scout" not in rec:
            continue
        cond = str(rec["lighting"])
        e = lighting.setdefault(
            cond, {"accepted": 0, "refused": 0, "frames": [], "clipped_pct": []}
        )
        ok = rec["scout"]["as_captured"].get("ok")
        e["accepted" if ok else "refused"] += 1
        e["frames"].append(rec["frame"])
        e["clipped_pct"].append(rec["exposure"]["clipped_pct"])

    return {
        "dataset_id": ds.dataset_id,
        "sku": ds.sku,
        "camera": ds.camera,
        "platform": ds.manifest.get("platform", {}),
        "bringup": ds.manifest.get("bringup", {}),
        "physical": ds.manifest.get("physical", {}),
        "evidence_type": ds.evidence_type,
        "capture_modes": sorted(
            {
                (str(by_id[f].get("format")), *frames[f]["size"], str(by_id[f].get("frame_rate")))
                for f in frames
            }
        ),
        "frames": frames,
        "excluded_frames": [
            {
                "frame": f.ref,
                "outcome": f.get("outcome", "ok"),
                "issues": [i.as_dict() for i in f.issues],
            }
            for f in ds.frames
            if not f.usable
        ],
        "calibrations": calibrations,
        "series": series_out,
        "focus_envelope": envelope,
        "lighting": lighting,
    }


def _num(s: str) -> float:
    try:
        return float(s)
    except ValueError:
        return float("inf")


def _truth_for(ds: Dataset, frs: list[Frame]) -> dict[str, float] | None:
    """Verified length/width of the planar part shared by every frame, if any."""
    found: dict[str, float] | None = None
    for fr in frs:
        parts = _targets_of(ds, fr, "planar_part")
        if not parts:
            return None
        truth = parts[0][1].get("truth", {})
        t = {k: float(truth[k]) for k in ("length_mm", "width_mm") if dimension_verified(truth, k)}
        if not t:
            return None
        if found is not None and found != t:
            return None
        found = t
    return found


def run(
    exp_root: Path, scout_tool: Path | None, min_views: int = 10
) -> tuple[dict[str, Any], Path]:
    exp_root = exp_root.resolve()
    exp, datasets, exp_issues = load_experiment(exp_root)
    summary = validation_summary(exp_issues, datasets)
    blocking = [i for i in exp_issues if i.level == "ERROR"] + [
        i for d in datasets for i in d.issues if i.level == "ERROR"
    ]
    out = derived_dir(exp_root)
    if out.exists():
        assert out.parent.parent == exp_root, "refusing to delete outside the experiment"
        shutil.rmtree(out)
    out.mkdir(parents=True)
    write_json(out / "validation.json", summary)
    if blocking:
        raise AnalysisRefused(
            f"{len(blocking)} blocking validation error(s); see {out / 'validation.json'}"
        )

    before = {
        f.ref: f.get("_sha256_actual") for d in datasets for f in d.frames if f.abs_path is not None
    }
    results = []
    for ds in datasets:
        r = analyze_dataset(ds, out / ds.dataset_id, scout_tool, min_views)
        write_json(out / ds.dataset_id / "analysis.json", r)
        results.append(r)

    changed = [
        f.ref
        for d in datasets
        for f in d.frames
        if f.abs_path is not None and sha256_file(f.abs_path) != before[f.ref]
    ]
    if changed:
        raise AnalysisRefused(f"SOURCE INTEGRITY FAILURE: {changed} changed during analysis")
    for d in out.rglob("work"):
        shutil.rmtree(d)  # intermediate PGMs are reproducible; do not keep
    combined = {
        "analysis_version": ANALYSIS_VERSION,
        "opencv_version": cv2.__version__,
        "numpy_version": np.__version__,
        "experiment": exp,
        "evidence_type": exp.get("evidence_type"),
        "source_hashes": before,
        "source_integrity": "unchanged",
        "validation": summary,
        "datasets": results,
    }
    write_json(out / "analysis.json", combined)
    return combined, out
