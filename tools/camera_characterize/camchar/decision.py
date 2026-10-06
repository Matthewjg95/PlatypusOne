"""Per-camera comparable metrics and the three-role decision gate.

The gate never forces a winner:
  - a criterion with no evidence (or no threshold yet) is INSUFFICIENT;
  - a role with any INSUFFICIENT criterion and no FAIL is INSUFFICIENT;
  - exactly one eligible camera -> PROPOSED (a human confirms it);
  - several eligible cameras -> CANDIDATES, no automatic tie-break;
  - none -> INSUFFICIENT EVIDENCE or NO ELIGIBLE CAMERA.
Synthetic fixtures can never produce a role.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any

import numpy as np

from . import UNKNOWN

PASS, FAIL, INSUFFICIENT = "PASS", "FAIL", "INSUFFICIENT"
QUANT_SCENES = ("calibration", "planar_part", "dimensional_reference", "repeatability", "lighting")


def load_criteria(path: Path) -> tuple[dict[str, Any], str]:
    data = path.read_bytes()
    return json.loads(data), hashlib.sha256(data).hexdigest()


def _median(v: list[float]) -> float | None:
    v = [x for x in v if x is not None]
    return float(np.median(v)) if v else None


def _bool(v: Any) -> bool | None:
    return v if isinstance(v, bool) else None


def _measured(phys: dict[str, Any], key: str) -> float | None:
    """Physical dimensions count only when measured on the unit (not vendor data)."""
    entry = phys.get(key)
    if isinstance(entry, dict) and entry.get("status") == "measured":
        v = entry.get("value")
        return float(v) if isinstance(v, (int, float)) else None
    return None


def camera_metrics(
    results: list[dict[str, Any]], rep_distance: Any, criteria: dict[str, Any]
) -> dict[str, Any]:
    """Collapse one camera's datasets into named metrics, each with evidence refs."""
    m: dict[str, dict[str, Any]] = {}

    def put(name: str, value: Any, evidence: list[str] | str, note: str = "") -> None:
        m[name] = {"value": value, "evidence": evidence, "note": note}

    cals = [c for r in results for c in r["calibrations"] if c.get("status") == "ok"]
    cals.sort(key=lambda c: (-c["views"], c["calibration_set"]))
    if cals:
        p = cals[0]
        ev = [f"calibration:{p['calibration_set']}"] + [v["frame"] for v in p["per_view"]]
        put("calibration_rms_px", p["rms_reprojection_px"], ev)
        put("calibration_views", p["views"], ev)
        put("calibration_edge_coverage", p["coverage"]["max_corner_radius_norm"], ev)
        fx = p["camera_matrix"][0][0]
        put("focal_rel_std", p["std_dev"]["fx"] / fx if fx else None, ev)
        put(
            "distortion_max_corner_pct",
            p["distortion_at_image_points"]["max_corner_displacement_pct"],
            ev,
            "model-derived from our calibration, not the vendor figure",
        )
    else:
        reasons = [c.get("reason") for r in results for c in r["calibrations"]]
        for k in (
            "calibration_rms_px",
            "calibration_views",
            "calibration_edge_coverage",
            "focal_rel_std",
        ):
            put(
                k,
                None,
                [],
                "; ".join(str(x) for x in reasons if x) or "no calibration set captured",
            )
    put("calibration_sets_ok", len(cals), [f"calibration:{c['calibration_set']}" for c in cals])
    same = [c for c in cals if c["size"] == cals[0]["size"]] if cals else []
    if len(same) >= 2:
        f = [c["camera_matrix"][0][0] for c in same]
        g = [c["camera_matrix"][1][1] for c in same]
        delta = max((max(f) - min(f)) / np.mean(f), (max(g) - min(g)) / np.mean(g))
        put(
            "calibration_focal_rel_delta",
            float(delta),
            [f"calibration:{c['calibration_set']}" for c in same],
        )
    else:
        put("calibration_focal_rel_delta", None, [], "needs two calibration sets at the same size")

    geo = [
        (fr["frame"], fr["geometry"])
        for r in results
        for fr in r["frames"].values()
        if fr.get("geometry", {}).get("calibrated")
    ]
    put(
        "corrected_line_rms_edge_px",
        _median([g["line_residual_undistorted"].get("edge_mean_px") for _, g in geo]),
        [f for f, g in geo if g["line_residual_undistorted"].get("edge_mean_px") is not None],
    )
    put(
        "reprojection_edge_to_center",
        _median([(g.get("reprojection_px") or {}).get("edge_to_center") for _, g in geo]),
        [f for f, g in geo if (g.get("reprojection_px") or {}).get("edge_to_center") is not None],
    )

    env_cfg = criteria["envelope"]
    thr, factor = env_cfg["edge_rise_px_max"], env_cfg["edge_placement_factor"]
    by_dist: dict[str, dict[str, list[float]]] = {}
    env_ev: list[str] = []
    for r in results:
        for row in r["focus_envelope"]:
            d = by_dist.setdefault(row["working_distance_mm"], {"center": [], "edge": []})
            if row["placement"] == "center":
                d["center"].append(row["edge_rise_px_median"])
            elif row["placement"] in ("edge", "corner"):
                d["edge"].append(row["edge_rise_px_median"])
            env_ev += row["frames"]
    good = []
    for dist, d in by_dist.items():
        c, e = _median(d["center"]), _median(d["edge"])
        if c is not None and e is not None and c <= thr and e <= thr * factor:
            good.append(dist)
    any_env = any(d["center"] and d["edge"] for d in by_dist.values())
    put(
        "working_envelope_distances",
        len(good) if any_env else None,
        env_ev,
        f"distances meeting the envelope rule: {sorted(good, key=_num)}"
        if any_env
        else "no centre+edge edge-rise data",
    )

    rep = _representative_series(results, rep_distance)
    if rep:
        name, s = rep
        stats = s["as_captured"]["repeatability"]["subject_length_mm"]
        put(
            "repeatability_rel_spread",
            stats.get("relative_spread"),
            s["as_captured"]["frames"],
            f"series {name}, as captured",
        )
        put(
            "repeatability_accepted_frames",
            s["as_captured"]["n_accepted"],
            s["as_captured"]["frames"],
        )
    else:
        put(
            "repeatability_rel_spread",
            None,
            [],
            f"no repeat series at {rep_distance} mm with Scout results",
        )
        put("repeatability_accepted_frames", None, [])

    quant = [fr for r in results for fr in r["frames"].values() if fr.get("scene") in QUANT_SCENES]
    if quant:
        # UNKNOWN focus = missing evidence (INSUFFICIENT), not a failure.
        # auto, or a motor position not recorded, is a FAIL: the state moved
        # or cannot be reproduced.
        modes = [fr["focus_mode"] for fr in quant]
        if any(mm in (UNKNOWN, None) for mm in modes):
            ok = None
        else:
            ok = all(
                mm in ("manual_locked", "fixed", "not_exposed")
                or (mm == "motorized_position" and fr["focus_position"] not in (UNKNOWN, None))
                for mm, fr in zip(modes, quant, strict=True)
            )
        put("focus_state_recorded", ok, [fr["frame"] for fr in quant])
    else:
        put("focus_state_recorded", None, [])

    focus_series = [
        (n, s)
        for r in results
        for n, s in r["series"].items()
        if s.get("edge_rise_px", {}).get("n", 0) >= 3
    ]
    put(
        "focus_repeat_rel_spread",
        _median([s["edge_rise_px"].get("relative_spread") for _, s in focus_series]),
        [f"series:{n}" for n, _ in focus_series],
        "edge-rise spread within repeat series (refocus series for B0393)",
    )

    phys = {}
    for r in results:
        phys.update(r.get("physical") or {})
    w, h = _measured(phys, "board_w_mm"), _measured(phys, "board_h_mm")
    put(
        "board_max_side_mm",
        max(w, h) if w is not None and h is not None else None,
        ["physical"],
        "measured only",
    )
    put("lens_stack_mm", _measured(phys, "lens_stack_mm"), ["physical"], "measured only")

    for key in ("enumerates", "reboot_repeat", "dsi_coexistence"):
        vals = [_bool((r.get("bringup") or {}).get(key)) for r in results]
        vals = [v for v in vals if v is not None]
        ev = [str((r.get("bringup") or {}).get("evidence", "")) for r in results]
        put(key, (all(vals) if vals else None), ev)

    acc = sum(v["accepted"] for r in results for v in r["lighting"].values())
    tot = acc + sum(v["refused"] for r in results for v in r["lighting"].values())
    put(
        "lighting_acceptance_fraction",
        acc / tot if tot else None,
        [f for r in results for v in r["lighting"].values() for f in v["frames"]],
    )
    return m


def _num(s: str) -> float:
    try:
        return float(s)
    except ValueError:
        return float("inf")


def _representative_series(results, rep_distance):
    best = None
    for r in results:
        for name, s in sorted(r["series"].items()):
            if "as_captured" not in s:
                continue
            if rep_distance not in (None, UNKNOWN) and not any(
                _num(x) == _num(str(rep_distance)) for x in s["working_distance_mm"]
            ):
                continue
            if best is None or s["as_captured"]["n_frames"] > best[1]["as_captured"]["n_frames"]:
                best = (f"{r['dataset_id']}/{name}", s)
    return best


def evaluate(metric: dict[str, Any] | None, op: str, threshold: Any) -> str:
    if threshold is None:
        return INSUFFICIENT
    if metric is None or metric.get("value") is None:
        return INSUFFICIENT
    v = metric["value"]
    if op == "<=":
        return PASS if v <= threshold else FAIL
    if op == ">=":
        return PASS if v >= threshold else FAIL
    if op == "==":
        return PASS if v == threshold else FAIL
    raise ValueError(op)


def decide(
    analysis: dict[str, Any],
    criteria: dict[str, Any],
    criteria_sha: str,
    _allow_synthetic_roles: bool = False,  # tests only; never exposed by the CLI
) -> dict[str, Any]:
    rep = analysis["experiment"].get("representative_distance_mm", UNKNOWN)
    by_sku: dict[str, list[dict[str, Any]]] = {}
    for r in analysis["datasets"]:
        by_sku.setdefault(r["sku"], []).append(r)
    cameras: dict[str, Any] = {}
    for sku, rs in sorted(by_sku.items()):
        metrics_ = camera_metrics(rs, rep, criteria)
        roles = {}
        for role, crits in criteria["roles"].items():
            rows = []
            for c in crits:
                st = evaluate(metrics_.get(c["metric"]), c["op"], c["threshold"])
                rows.append(
                    {
                        **c,
                        "value": (metrics_.get(c["metric"]) or {}).get("value"),
                        "status": st,
                        "evidence_count": len(
                            (metrics_.get(c["metric"]) or {}).get("evidence") or []
                        ),
                    }
                )
            statuses = {r["status"] for r in rows}
            verdict = (
                "NOT_ELIGIBLE"
                if FAIL in statuses
                else INSUFFICIENT
                if INSUFFICIENT in statuses
                else "ELIGIBLE"
            )
            roles[role] = {"verdict": verdict, "criteria": rows}
        cameras[sku] = {"metrics": metrics_, "roles": roles}

    synthetic = analysis.get("evidence_type") != "physical_capture"
    decision: dict[str, Any] = {}
    for role in criteria["roles"]:
        eligible = sorted(
            s for s, c in cameras.items() if c["roles"][role]["verdict"] == "ELIGIBLE"
        )
        insufficient = sorted(
            s for s, c in cameras.items() if c["roles"][role]["verdict"] == INSUFFICIENT
        )
        if synthetic and not _allow_synthetic_roles:
            decision[role] = {
                "outcome": "INSUFFICIENT EVIDENCE",
                "reason": "synthetic fixture - not camera evidence",
            }
        elif len(eligible) == 1:
            decision[role] = {
                "outcome": "PROPOSED",
                "camera": eligible[0],
                "reason": "only eligible camera; human confirmation required",
            }
        elif len(eligible) > 1:
            decision[role] = {
                "outcome": "CANDIDATES",
                "cameras": eligible,
                "reason": "several eligible; no automatic tie-break - compare raw metrics",
            }
        elif insufficient:
            decision[role] = {
                "outcome": "INSUFFICIENT EVIDENCE",
                "cameras_lacking_evidence": insufficient,
            }
        else:
            decision[role] = {
                "outcome": "NO ELIGIBLE CAMERA",
                "reason": "every camera failed at least one criterion",
            }
    prod = decision.get("product", {})
    fb = decision.get("fallback", {})
    if prod.get("outcome") == "PROPOSED" and fb.get("outcome") in ("PROPOSED", "CANDIDATES"):
        others = [
            c
            for c in ([fb.get("camera")] if fb.get("camera") else fb.get("cameras", []))
            if c != prod["camera"]
        ]
        if others:
            fb.update(
                {"outcome": "PROPOSED" if len(others) == 1 else "CANDIDATES", "cameras": others}
            )
            fb["camera"] = others[0] if len(others) == 1 else None
            fb["reason"] = "eligible and distinct from the product candidate"
        else:
            decision["fallback"] = {
                "outcome": "INSUFFICIENT EVIDENCE",
                "reason": "only the product candidate qualifies; "
                "a fallback must be a different path",
            }
    return {
        "criteria_version": criteria.get("criteria_version"),
        "criteria_sha256": criteria_sha,
        "evidence_type": analysis.get("evidence_type"),
        "synthetic_roles_allowed": bool(_allow_synthetic_roles),
        "cameras": cameras,
        "decision": decision,
    }
