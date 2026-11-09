"""Measurement repeatability and accuracy through the product's Scout analyzer.

Each frame containing a Scout reference square is measured by `scout_measure`
(tools/scout_measure: services/vision analyzeFrame, unchanged). Two inputs:
  as_captured   the frame as stored (what the product measures today)
  undistorted   the same frame undistorted with this camera's calibration,
                when one exists (a physical model, not a fudge factor)

REPEATABILITY = spread of repeated measurements of an unmoved scene.
ACCURACY      = difference from INDEPENDENT physical truth (calipers etc.),
                computed only when the target's truth is marked verified.
No correction factor is ever applied to bring a camera's numbers to truth.
"""

from __future__ import annotations

import json
import subprocess
from pathlib import Path
from typing import Any

import numpy as np


def run_scout(tool: Path, pgm: Path, reference_mm: float) -> dict[str, Any]:
    proc = subprocess.run(
        [str(tool), str(pgm), "--reference-mm", repr(float(reference_mm))],
        capture_output=True,
        text=True,
        check=False,
    )
    if proc.returncode != 0:
        return {"ok": False, "error": f"tool_failed: {proc.stderr.strip()}"}
    return json.loads(proc.stdout)


def series_stats(values: list[float]) -> dict[str, Any]:
    v = np.asarray(values, dtype=np.float64)
    if len(v) == 0:
        return {"n": 0}
    out: dict[str, Any] = {
        "n": int(len(v)),
        "mean": float(v.mean()),
        "min": float(v.min()),
        "max": float(v.max()),
        "range": float(v.max() - v.min()),
    }
    if len(v) >= 2:
        sd = float(v.std(ddof=1))
        out["std"] = sd
        out["relative_spread"] = sd / out["mean"] if out["mean"] else None
    else:
        out["std"] = None
        out["relative_spread"] = None
    return out


def summarize(results: list[dict[str, Any]], truth: dict[str, float] | None) -> dict[str, Any]:
    """results: [{"frame": ref, "measurement": scout_json}], one series."""
    ok = [r for r in results if r["measurement"].get("ok")]
    refused: dict[str, int] = {}
    for r in results:
        if not r["measurement"].get("ok"):
            e = r["measurement"].get("error", "unknown")
            refused[e] = refused.get(e, 0) + 1
    out: dict[str, Any] = {
        "frames": [r["frame"] for r in results],
        "n_frames": len(results),
        "n_accepted": len(ok),
        "n_refused": len(results) - len(ok),
        "refusals": refused,
        "repeatability": {
            "subject_length_mm": series_stats([r["measurement"]["subject_length_mm"] for r in ok]),
            "subject_width_mm": series_stats([r["measurement"]["subject_width_mm"] for r in ok]),
            "mm_per_pixel": series_stats([r["measurement"]["mm_per_pixel"] for r in ok]),
        },
    }
    if not truth:
        out["accuracy"] = {"status": "not_computed", "reason": "no verified independent truth"}
        return out
    acc: dict[str, Any] = {"status": "computed", "truth": truth}
    for key, tkey in (("subject_length_mm", "length_mm"), ("subject_width_mm", "width_mm")):
        if tkey not in truth:
            continue
        errs = [r["measurement"][key] - truth[tkey] for r in ok]
        if errs:
            e = np.asarray(errs)
            acc[key] = {
                "mean_error_mm": float(e.mean()),  # systematic part (bias)
                "mean_abs_error_mm": float(np.abs(e).mean()),
                "max_abs_error_mm": float(np.abs(e).max()),
                "n": int(len(e)),
            }
    out["accuracy"] = acc
    return out


def temporal_noise(lums: list[np.ndarray], full_scale: float) -> dict[str, Any] | None:
    """Per-pixel standard deviation across an unmoved series (median over pixels).

    Valid only when the fixture and lighting did not move: it then measures
    sensor + processing noise at that exposure, not scene change.
    """
    if len(lums) < 3 or len({x.shape for x in lums}) != 1:
        return None
    stack = np.stack(lums, axis=0)
    sd = stack.std(axis=0, ddof=1)
    med = float(np.median(sd))
    return {
        "frames": len(lums),
        "median_pixel_std_norm": med,
        "median_pixel_std_dn": med * full_scale,
        "p95_pixel_std_norm": float(np.percentile(sd, 95)),
    }
