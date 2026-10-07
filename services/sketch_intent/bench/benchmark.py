"""Sketch intent resolver benchmark: dev, held-out, sweeps, external evaluation sets.

    PYTHONPATH=services/sketch_intent python3 services/sketch_intent/bench/benchmark.py \
        [--out docs/architecture/SKETCH_INTENT_BENCHMARK.md] [--json results.json] \
        [--shadowscan PATH/TO/shadowscan-mobile] [--washer-ref pr39] [--quick]

Everything is deterministic. Reported quantities:
  * GT discrepancy: symmetric Hausdorff distance and RMS between the proposed
    geometry and the clean ground truth, in px and mm. This is the error of the
    proposal against KNOWN SYNTHETIC truth, not an accuracy claim for real parts.
  * Constraint behaviour against ground truth: every parallel / perpendicular /
    equal-radius / tangent hypothesis is scored against what the GT geometry
    actually is. A FALSE PROPOSAL (proposed, but untrue in GT) is the failure
    that matters most: that is silent perfection. MISSED = true in GT but not
    proposed (questions are counted separately).
  * Expectations written into the fixtures before they were run.
  * Runtime (best of 3, this machine, single thread). Counts, never percentages.

External sets are evaluation only - nothing was tuned on them:
  * ShadowScan synthetic renders (corpus/photos, truth from corpus.txt), traced
    with a plain Otsu threshold. The scale comes from the part's own width
    (corpus "ref-width"), so width is NOT an independent check; holes are.
  * PlatypusOne scan-0053 (real washer, Adesso camera, PR #39 evidence), traced
    the way its own offline script segments it, scaled with the on-device
    mm/px. Caliper truth: outer 19.12 mm, bore 8.71 mm.
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import math
import re
import subprocess
import sys
import time
from collections import Counter
from pathlib import Path
from typing import Any

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tests"))
sys.path.insert(0, str(ROOT / "bench"))

from fixtures import Case, dev_cases, heldout_cases, polygon, realize, rect, rot  # noqa: E402
from freeze import check as freeze_check  # noqa: E402
from sketch_intent.contract import Params, observation_from_dict  # noqa: E402
from sketch_intent.fitting import cross2  # noqa: E402
from sketch_intent.resolve import resolve  # noqa: E402

WASHER_DIR = "docs/hardware/evidence/scout-2026-10-02-washer"


# --------------------------------------------------------------------------- geometry
def sample_entity(e: dict[str, Any], step: float = 0.25) -> np.ndarray:
    if e["kind"] == "line":
        a, b = np.array(e["start"]), np.array(e["end"])
        n = max(2, int(np.linalg.norm(b - a) / step))
        return a + (b - a) * np.linspace(0, 1, n)[:, None]
    if e["kind"] == "circle":
        c, r = np.array(e["center"]), e["radius"]
        t = np.linspace(0, 2 * np.pi, max(16, int(2 * np.pi * r / step)), endpoint=False)
        return c + r * np.c_[np.cos(t), np.sin(t)]
    if e["kind"] == "arc":
        c, r = np.array(e["center"]), e["radius"]
        a0 = math.atan2(e["start"][1] - c[1], e["start"][0] - c[0])
        a1 = math.atan2(e["end"][1] - c[1], e["end"][0] - c[0])
        ccw = e["sweep_sign_image"] > 0
        sweep = (a1 - a0) % (2 * np.pi) if ccw else -((a0 - a1) % (2 * np.pi))
        t = a0 + sweep * np.linspace(0, 1, max(4, int(abs(sweep) * r / step)))
        return c + r * np.c_[np.cos(t), np.sin(t)]
    pts = np.array(e["points"])
    out = [pts[:1]]
    for p, q in zip(pts[:-1], pts[1:], strict=True):
        n = max(1, int(np.linalg.norm(q - p) / step))
        out.append(p + (q - p) * np.linspace(0, 1, n + 1)[1:, None])
    return np.concatenate(out)


def _nearest(a: np.ndarray, b: np.ndarray) -> np.ndarray:
    """For each point of a, distance to the nearest point of b (chunked)."""
    out = np.empty(len(a))
    for i in range(0, len(a), 2048):
        d = np.linalg.norm(a[i : i + 2048, None, :] - b[None, :, :], axis=2)
        out[i : i + 2048] = d.min(axis=1)
    return out


def gt_discrepancy(proposal: dict[str, Any], clean: list[np.ndarray]) -> list[dict[str, Any]]:
    """Per GT loop: symmetric Hausdorff and RMS to the proposed geometry loop nearest it."""
    loops = proposal["proposed_geometry"]["loops"]
    samples = [np.concatenate([sample_entity(e) for e in lo["entities"]]) for lo in loops]
    out = []
    for gi, G in enumerate(clean):
        j = int(np.argmin([np.linalg.norm(S.mean(0) - G.mean(0)) for S in samples]))
        d1, d2 = _nearest(G, samples[j]), _nearest(samples[j], G)
        out.append(
            {
                "gt_loop": gi,
                "loop_id": loops[j]["loop_id"],
                "hausdorff_px": float(max(d1.max(), d2.max())),
                "rms_px": float(np.sqrt(np.mean(np.r_[d1, d2] ** 2))),
                "closed_profile": loops[j]["closed_profile"],
            }
        )
    return out


# --------------------------------------------------------------------- GT constraint truth
def _gt_elements(case: Case) -> tuple[list[dict], list[dict]]:
    lines, rounds = [], []
    for li, segs in enumerate(case.loops):
        for s in segs:
            if s[0] == "line":
                p, q = np.array(s[1], float), np.array(s[2], float)
                lines.append({"loop": li, "p": p, "q": q, "d": (q - p) / np.linalg.norm(q - p)})
            else:
                rounds.append({"loop": li, "c": np.array(s[1], float), "r": float(s[2])})
    return lines, rounds


def _match_line(prim: dict, gt_lines: list[dict], P: np.ndarray) -> dict | None:
    pts = P[prim["point_indices"]]
    best, bd = None, 1e9
    for g in gt_lines:
        v = pts - g["p"]
        t = np.clip(v @ g["d"], 0, np.linalg.norm(g["q"] - g["p"]))
        d = float(np.median(np.linalg.norm(v - t[:, None] * g["d"], axis=1)))
        if d < bd:
            best, bd = g, d
    return best if bd < 3.0 else None


def _match_round(prim: dict, gt_rounds: list[dict]) -> dict | None:
    c = np.array(prim["fit"]["center_px"])
    best = min(gt_rounds, key=lambda g: np.linalg.norm(g["c"] - c), default=None)
    return (
        best if best is not None and np.linalg.norm(best["c"] - c) < 0.5 * best["r"] + 3 else None
    )


def constraint_truth(case: Case, proposal: dict[str, Any]) -> dict[str, Counter]:
    """Score parallel / perpendicular / equal_radius / tangent decisions against GT."""
    gt_lines, gt_rounds = _gt_elements(case)
    P = {lp["loop_id"]: np.array(lp["points_px"]) for lp in proposal["evidence"]["loops"]}
    prim = {p["id"]: p for p in proposal["primitives"]}
    m: dict[str, Any] = {}
    for p in proposal["primitives"]:
        if p["kind"] == "line":
            m[p["id"]] = _match_line(p, gt_lines, P[p["loop_id"]])
        elif p["kind"] in ("arc", "circle"):
            m[p["id"]] = _match_round(p, gt_rounds)
    out: dict[str, Counter] = {}
    for c in proposal["constraints"]:
        t = c["type"]
        if t not in ("parallel", "perpendicular", "equal_radius", "tangent"):
            continue
        g = [m.get(e) for e in c["entities"]]
        if any(x is None for x in g):
            truth = None
        elif t in ("parallel", "perpendicular"):
            cosang = abs(float(g[0]["d"] @ g[1]["d"]))
            truth = cosang > math.cos(math.radians(0.01)) if t == "parallel" else cosang < 1.7e-4
        elif t == "equal_radius":
            truth = abs(g[0]["r"] - g[1]["r"]) < 1e-6
        else:  # tangent: GT arc center at distance r from each GT neighbour line
            arc, lines = g[0], g[1:]
            truth = all(
                abs(abs(float(cross2(ln["d"], arc["c"] - ln["p"]))) - arc["r"]) < 1e-3
                for ln in lines
            )
        key = "unmatched" if truth is None else ("true_in_gt" if truth else "false_in_gt")
        out.setdefault(t, Counter())[f"{key}/{c['decision']}"] += 1
    del prim
    return out


def summarize_truth(tr: dict[str, Counter]) -> dict[str, int]:
    s = Counter()
    for _t, cnt in tr.items():
        for k, v in cnt.items():
            truth, dec = k.split("/")
            if truth == "false_in_gt" and dec == "proposed":
                s["FALSE_PROPOSAL"] += v
            elif truth == "false_in_gt":
                s["correctly_not_proposed"] += v
            elif truth == "true_in_gt" and dec == "proposed":
                s["correctly_proposed"] += v
            elif truth == "true_in_gt" and dec == "question":
                s["true_but_question"] += v
            elif truth == "true_in_gt":
                s["MISSED"] += v
            else:
                s["unmatched"] += v
    return dict(s)


# ------------------------------------------------------------------------ expectations
def _cyc(a: list, b: list) -> bool:
    return len(a) == len(b) and any(a[i:] + a[:i] == b for i in range(len(a)))


def check_expect(
    case: Case, p: dict[str, Any], tr: dict[str, Counter]
) -> list[tuple[str, bool, str]]:
    ex = case.expect
    kinds = [
        [x["kind"] for x in p["primitives"] if x["loop_id"] == lp["loop_id"]]
        for lp in p["evidence"]["loops"]
    ]
    dec = {
        t: Counter(c["decision"] for c in p["constraints"] if c["type"] == t)
        for t in ("perpendicular", "parallel", "tangent", "equal_radius", "collinear")
    }
    res = []

    def add(name, ok, got):
        res.append((name, bool(ok), str(got)))

    if "kinds" in ex:
        add("kinds", len(kinds) >= 1 and _cyc(kinds[0], ex["kinds"]), kinds[0] if kinds else None)
    if "kinds_by_loop" in ex:
        want = ex["kinds_by_loop"]
        ok = len(kinds) == len(want) and all(any(_cyc(k, w) for k in kinds) for w in want)
        add("kinds_by_loop", ok, kinds)
    for t in ("perpendicular", "parallel", "tangent", "equal_radius"):
        if t not in ex:
            continue
        d = dec[t]
        if ex[t] == "proposed":
            # every hypothesis that is TRUE in GT proposed, none FALSE in GT proposed
            c = tr.get(t, Counter())
            ok = (
                d["proposed"] > 0
                and c.get("false_in_gt/proposed", 0) == 0
                and not any(k.startswith("true_in_gt/") and not k.endswith("/proposed") for k in c)
            )
            add(t, ok, dict(c) or dict(d))
        elif ex[t] == "not_proposed":
            c = tr.get(t, Counter())
            add(t, c.get("false_in_gt/proposed", 0) == 0, dict(c) or dict(d))
        elif ex[t] == "question_or_proposed":
            add(t, d["rejected_by_evidence"] == 0 and sum(d.values()) > 0, dict(d))
    if "parallel_count" in ex:
        add(
            "parallel_count",
            dec["parallel"]["proposed"] == ex["parallel_count"],
            dict(dec["parallel"]),
        )
    if "closed" in ex:
        got = [lo["closed_profile"] for lo in p["proposed_geometry"]["loops"]]
        add("closed", all(g == ex["closed"] for g in got), got)
    if "radius" in ex:
        r = [x["fit"]["radius_px"] for x in p["primitives"] if x["kind"] in ("arc", "circle")]
        add("radius_within_1px", len(r) == 1 and abs(r[0] - ex["radius"]) < 1.0, r)
    if "outliers" in ex:
        n = len(p["evidence"]["loops"][0]["outliers"])
        add("outliers", n == ex["outliers"], n)
    if ex.get("has_gap"):
        add(
            "gap_flagged",
            len(p["evidence"]["loops"][0]["gaps"]) >= 1,
            p["evidence"]["loops"][0]["gaps"],
        )
    if ex.get("gap_vertex") == "question":
        add(
            "gap_bridge_is_question",
            dec["collinear"]["question"] >= 1 and dec["collinear"]["proposed"] == 0,
            dict(dec["collinear"]),
        )
    if ex.get("freeform"):
        add("freeform_flagged", any(k == "freeform" for ks in kinds for k in ks), kinds)
    if ex.get("no_invented_circle"):
        add("no_invented_circle", not any(k in ("circle",) for ks in kinds for k in ks), kinds)
    if ex.get("uncalibrated"):
        add(
            "uncalibrated_reported",
            any(u["about"] == ["input.calibration"] for u in p["unsupported"]),
            "",
        )
    if "corner_angles" in ex:
        ang = corner_angles(p)
        lo, hi = ex["corner_angles"]
        ok = len(ang) == 4 and all(min(abs(a - lo), abs(a - hi)) < 1.0 for a in ang)
        add("corner_angles_kept", ok, [round(a, 2) for a in ang])
    return res


def corner_angles(p: dict[str, Any]) -> list[float]:
    ents = p["proposed_geometry"]["loops"][0]["entities"]
    if not all(e["kind"] == "line" for e in ents):
        return []
    out = []
    for e, f in zip(ents, ents[1:] + ents[:1], strict=True):
        d1 = np.subtract(e["end"], e["start"])
        d2 = np.subtract(f["end"], f["start"])
        out.append(
            math.degrees(
                math.acos(np.clip(-(d1 @ d2) / np.linalg.norm(d1) / np.linalg.norm(d2), -1, 1))
            )
        )
    return out


# ------------------------------------------------------------------------------ runners
def timed_resolve(data: dict, reps: int = 3) -> tuple[dict, float]:
    best = 1e9
    p = None
    for _ in range(reps):
        t = time.perf_counter()
        p = resolve(observation_from_dict(data, "bench"), Params())
        best = min(best, time.perf_counter() - t)
    return p, best


def run_case(case: Case, reps: int = 3) -> dict[str, Any]:
    data, clean = realize(case)
    p, dt = timed_resolve(data, reps)
    tr = constraint_truth(case, p)
    k = case.scale_mm_per_px
    disc = gt_discrepancy(p, clean)
    for d in disc:
        d["hausdorff_mm"] = None if k is None else d["hausdorff_px"] * k
    return {
        "name": case.name,
        "sampler": case.sampler if data.get("synthetic_sampler") else "walk",
        "noise_px": case.noise,
        "n_points": sum(len(lp["points_px"]) for lp in p["evidence"]["loops"]),
        "runtime_ms": dt * 1000,
        "kinds": [
            [x["kind"] for x in p["primitives"] if x["loop_id"] == lp["loop_id"]]
            for lp in p["evidence"]["loops"]
        ],
        "decisions": {
            t: dict(Counter(c["decision"] for c in p["constraints"] if c["type"] == t))
            for t in sorted({c["type"] for c in p["constraints"]})
        },
        "questions": [q["text"] for q in p["questions"]],
        "unsupported": [u["reason"] for u in p["unsupported"]],
        "gt_discrepancy": disc,
        "constraint_truth": {t: dict(c) for t, c in tr.items()},
        "truth_summary": summarize_truth(tr),
        "expectations": check_expect(case, p, tr),
    }


def noise_sweep(quick: bool) -> list[dict[str, Any]]:
    """Two tolerance policies on the same contours:

    fixed     the default 2 px band, whatever the noise;
    matched   tolerance = max(2, 3.5 sigma): the documented rule that the band must
              cover the contour's own noise (the largest of ~600 Gaussian deviates
              is about 3.3 sigma). sigma is the fixture's known noise here; on real
              data it has to be measured from the contour source.
    """
    C = (300.0, 300.0)
    shapes = {
        "rectangle (90 deg)": rot(rect(200, 240, 200, 120), 7, C),
        "skew 2 deg": polygon([(200, 240), (400, 240), (404.2, 360), (204.2, 360)]),
        "skew 6 deg": polygon([(200, 240), (400, 240), (412.6, 360), (212.6, 360)]),
    }
    if quick:
        plan = [("fixed", s) for s in (0.0, 0.5)] + [("matched", 1.0)]
        seeds = range(2)
    else:
        plan = [("fixed", s) for s in (0.0, 0.25, 0.5, 0.75, 1.0)]
        plan += [("matched", s) for s in (0.5, 1.0, 1.5, 2.0)]
        seeds = range(5)
    rows = []
    for name, segs in shapes.items():
        for policy, s in plan:
            prm = Params() if policy == "fixed" else Params(tolerance_px=max(2.0, 3.5 * s))
            perp = Counter()
            kinds_ok = 0
            hd, rt = [], []
            for seed in seeds:
                case = Case(name, [segs], {}, seed=1000 + seed, noise=s)
                data, clean = realize(case)
                t = time.perf_counter()
                p = resolve(observation_from_dict(data, "sweep"), prm)
                rt.append(time.perf_counter() - t)
                perp.update(c["decision"] for c in p["constraints"] if c["type"] == "perpendicular")
                kinds_ok += [x["kind"] for x in p["primitives"]] == ["line"] * 4
                hd.append(gt_discrepancy(p, clean)[0]["hausdorff_px"])
            rows.append(
                {
                    "shape": name,
                    "policy": policy,
                    "tolerance_px": prm.tolerance_px,
                    "sigma_px": s,
                    "runs": len(seeds),
                    "four_lines": kinds_ok,
                    "perpendicular": dict(perp),
                    "hausdorff_px_median": float(np.median(hd)),
                    "hausdorff_px_max": float(np.max(hd)),
                    "runtime_s_max": float(max(rt)),
                }
            )
    return rows


def density_sweep() -> list[dict[str, Any]]:
    """Same rounded rectangle at different scales and point spacings (px)."""
    from fixtures import rounded_rect

    rows = []
    for scale in (0.5, 1.0, 2.0):
        for spacing in (0.5, 1.0, 2.0, 4.0):
            segs = rounded_rect(100 * scale, 100 * scale, 220 * scale, 130 * scale, 22 * scale)
            case = Case("rr", [segs], {}, seed=7, noise=0.3, spacing=spacing)
            data, clean = realize(case)
            p, dt = timed_resolve(data, 1)
            kinds = [x["kind"] for x in p["primitives"]]
            rows.append(
                {
                    "scale": scale,
                    "spacing_px": spacing,
                    "n_points": len(data["outlines"][0]),
                    "kinds_ok": _cyc(kinds, ["line", "arc"] * 4),
                    "kinds": "".join(k[0] for k in kinds),
                    "closed": p["proposed_geometry"]["loops"][0]["closed_profile"],
                    "hausdorff_px": gt_discrepancy(p, clean)[0]["hausdorff_px"],
                    "questions": len(p["questions"]),
                    "runtime_ms": dt * 1000,
                }
            )
    return rows


# ----------------------------------------------------------------- external evaluation
def _otsu_contours(gray: np.ndarray, dark_object: bool = True) -> list[np.ndarray]:
    import cv2

    flag = cv2.THRESH_BINARY_INV if dark_object else cv2.THRESH_BINARY
    _, mask = cv2.threshold(gray, 0, 255, flag + cv2.THRESH_OTSU)
    n, lab, stats, _ = cv2.connectedComponentsWithStats(mask)
    if n < 2:
        return []
    big = 1 + int(np.argmax(stats[1:, cv2.CC_STAT_AREA]))
    mask = np.where(lab == big, 255, 0).astype(np.uint8)
    cs, _ = cv2.findContours(mask, cv2.RETR_CCOMP, cv2.CHAIN_APPROX_NONE)
    cs = [c[:, 0, :].astype(float) + 0.5 for c in cs if len(c) >= 20]
    cs.sort(key=lambda c: -abs(cv2.contourArea(c.astype(np.float32))))
    return cs


def shadowscan_eval(root: Path) -> list[dict[str, Any]]:
    import cv2

    corpus = (root / "corpus" / "corpus.txt").read_text().splitlines()
    rows = []
    for line in corpus:
        if not line.startswith("image=photos/") or "ref-width=" not in line or "/real/" in line:
            continue
        kv = dict(re.findall(r"(\S+?)=(\S+)", line))
        img = cv2.imread(str(root / "corpus" / kv["image"]), cv2.IMREAD_GRAYSCALE)
        cs = _otsu_contours(img, dark_object=kv.get("polarity", "dark") == "dark")
        if not cs:
            rows.append({"image": kv["image"], "error": "no contour"})
            continue
        width_px = float(cs[0][:, 0].max() - cs[0][:, 0].min())
        k = float(kv["ref-width"]) / width_px
        data = {
            "format": "shadowscan-outline",
            "units": "px",
            "scale_mm_per_unit": k,
            "outlines": [c.tolist() for c in cs],
        }
        p, dt = timed_resolve(data, 1)
        holes_truth = sorted(
            float(d)
            for spec in kv.get("holes", "").split(",")
            if spec
            for d in [spec.split("x")[0]] * int(spec.split("x")[1])
        )
        circles = sorted(x["fit"]["diameter_mm"] for x in p["primitives"] if x["kind"] == "circle")
        # each analytic circle against the nearest true hole diameter (counts can differ)
        errs = [min((c - t for t in holes_truth), key=abs) for c in circles] if holes_truth else []
        prim = {x["id"]: x for x in p["primitives"]}

        def truth_class(pid, prim=prim, holes=tuple(holes_truth)):
            if prim[pid]["kind"] != "circle" or not holes:
                return None
            d = prim[pid]["fit"]["diameter_mm"]
            hs = np.array(sorted(set(holes)))
            return float(hs[int(np.argmin(np.abs(hs - d)))])

        eq = Counter()
        for c in p["constraints"]:
            if c["type"] != "equal_radius":
                continue
            cls = [truth_class(e) for e in c["entities"]]
            key = (
                "unscored"
                if None in cls
                else ("same_truth" if cls[0] == cls[1] else "different_truth")
            )
            eq[f"{key}/{c['decision']}"] += 1
        rows.append(
            {
                "image": kv["image"],
                "mm_per_px_from_ref_width": k,
                "n_loops": len(cs),
                "kinds": [
                    [x["kind"] for x in p["primitives"] if x["loop_id"] == lp["loop_id"]]
                    for lp in p["evidence"]["loops"]
                ],
                "holes_truth_mm": holes_truth,
                "circle_diameters_mm": [round(c, 3) for c in circles],
                "n_hypotheses": len(p["constraints"]),
                "hole_errors_mm": [round(e, 3) for e in errs],
                "hole_tol_mm": float(kv.get("holetol", "nan")),
                "equal_radius": dict(eq),
                "questions": len(p["questions"]),
                "unsupported": [u["reason"][:90] for u in p["unsupported"]],
                "closed": [lo["closed_profile"] for lo in p["proposed_geometry"]["loops"]],
                "runtime_ms": dt * 1000,
            }
        )
    return rows


def washer_eval(ref: str) -> dict[str, Any] | None:
    """scan-0053, read from git (PR #39 evidence) without modifying anything."""
    import cv2

    def blob(path: str) -> bytes | None:
        r = subprocess.run(
            ["git", "-C", str(REPO), "show", f"{ref}:{WASHER_DIR}/{path}"], capture_output=True
        )
        return r.stdout if r.returncode == 0 else None

    raw, rec = blob("scan-0053/source.yuyv"), blob("scan-0053/observation.json")
    if raw is None or rec is None:
        return None
    record = json.loads(rec)
    mmpp = next(c["value"] for c in record["derived"] if c["name"] == "mm_per_pixel")
    y = np.frombuffer(raw, np.uint8).reshape(480, 1280)[:, 0::2].astype(float)
    # same segmentation as offline/washer_circle_fit.py: ROI, metal/paper midpoint
    roi = (slice(120, 400), slice(40, 300))
    thr = (np.percentile(y[roi], 10) + np.percentile(y[roi], 90)) / 2
    mask = np.zeros(y.shape, np.uint8)
    mask[roi] = np.where(y[roi] < thr, 255, 0)
    n, lab, stats, _ = cv2.connectedComponentsWithStats(mask, connectivity=4)
    big = 1 + int(np.argmax(stats[1:, cv2.CC_STAT_AREA]))
    mask = np.where(lab == big, 255, 0).astype(np.uint8)
    cs, _ = cv2.findContours(mask, cv2.RETR_CCOMP, cv2.CHAIN_APPROX_NONE)
    cs = sorted((c[:, 0, :].astype(float) + 0.5 for c in cs if len(c) >= 20), key=lambda c: -len(c))
    data = {
        "format": "shadowscan-outline",
        "units": "px",
        "scale_mm_per_unit": mmpp,
        "outlines": [c.tolist() for c in cs],
        "source": f"{WASHER_DIR}/scan-0053 @ {ref}",
    }
    p, dt = timed_resolve(data, 1)
    prims = [
        {
            "id": x["id"],
            "loop": x["loop_id"],
            "kind": x["kind"],
            "diameter_mm": x.get("fit", {}).get("diameter_mm"),
            "residual_max_mm": (x.get("residual") or {}).get("max_mm"),
            "residual_rms_mm": (x.get("residual") or {}).get("rms_mm"),
        }
        for x in p["primitives"]
    ]
    return {
        "mm_per_px": mmpp,
        "threshold": float(thr),
        "loops_points": [len(c) for c in cs],
        "tolerance_mm": p["parameters"]["tolerance_mm"],
        "primitives": prims,
        "constraints": [
            (c["type"], c["entities"], c["decision"], c["reason"]) for c in p["constraints"]
        ],
        "questions": [q["text"] for q in p["questions"]],
        "unsupported": [u["reason"] for u in p["unsupported"]],
        "caliper_mm": {"outer": 19.12, "bore": 8.71},
        "offline_circle_fit_mm": {"outer": 18.61, "bore": 8.58},
        "runtime_ms": dt * 1000,
        "proposal": p,
    }


# ------------------------------------------------------------------------------ report
def _fmt_disc(d: list[dict]) -> str:
    worst = max(d, key=lambda x: x["hausdorff_px"])
    mm = f" / {worst['hausdorff_mm']:.3f} mm" if worst.get("hausdorff_mm") is not None else ""
    return f"{worst['hausdorff_px']:.2f} px{mm}"


def report(res: dict[str, Any]) -> str:
    L = []
    w = L.append
    w("# Sketch intent resolver - benchmark report")
    w("")
    w(
        "Generated by `services/sketch_intent/bench/benchmark.py` (resolver "
        f"`{res['resolver_version']}`, "
        f"default parameters, tolerance {res['params']['tolerance_px']} px). Deterministic; "
        "re-run to reproduce."
    )
    w("")
    w(
        "**Everything below except the washer section is synthetic.** GT discrepancy is measured "
        "against exact synthetic geometry and says how far the *proposal* is from the shape that "
        "generated the contour. It is not a statement about the accuracy of any camera, scale, or "
        "real part. Residuals inside proposals are fit diagnostics, not accuracy."
    )
    w("")
    fz = res["freeze"]
    w("## Freeze status")
    w("")
    if fz["matches"]:
        w(
            "Resolver code and parameters match `bench/FREEZE.json` (frozen "
            f"{fz['frozen_at_utc']}, "
            f"source sha256 `{fz['source_sha256'][:16]}...`). The held-out set, the ShadowScan "
            "renders and "
            "the washer were first run after that freeze, and nothing was changed in response "
            "to them."
        )
    else:
        w(
            "**Code or parameters differ from bench/FREEZE.json: the 'held-out' results below "
            "are NOT "
            "held-out evidence for this code.**"
        )
    w("")
    w("## Failure cases (generated)")
    w("")
    w("Listed first so they are not lost in the tables. None of these was tuned away.")
    w("")
    nf = 0
    for setname in ("dev", "heldout"):
        for r in res[setname]:
            for n, ok, got in r["expectations"]:
                if not ok:
                    nf += 1
                    w(f"- **{setname} `{r['name']}`**: expectation `{n}` failed (got `{got}`).")
            if r["truth_summary"].get("FALSE_PROPOSAL"):
                nf += 1
                w(
                    f"- **{setname} `{r['name']}`**: {r['truth_summary']['FALSE_PROPOSAL']} "
                    "false constraint proposals."
                )
    for r in res["noise"]:
        if r["shape"].startswith("skew") and r["perpendicular"].get("proposed"):
            nf += 1
            w(
                f"- **noise sweep `{r['shape']}`, {r['policy']} tolerance "
                f"{r['tolerance_px']:g} px, σ {r['sigma_px']}**: "
                f"{r['perpendicular']['proposed']} perpendicular hypotheses PROPOSED on a "
                "skewed shape "
                f"(silent perfection; {r['runs']} runs)."
            )
        if r["four_lines"] < r["runs"]:
            nf += 1
            w(
                f"- **noise sweep `{r['shape']}`, {r['policy']} tolerance "
                f"{r['tolerance_px']:g} px, σ {r['sigma_px']}**: "
                f"segmentation wrong (not 4 lines) in {r['runs'] - r['four_lines']} of "
                f"{r['runs']} runs; "
                f"slowest run {r['runtime_s_max']:.1f} s."
            )
    for r in res["density"]:
        if not r["kinds_ok"]:
            nf += 1
            w(
                f"- **density sweep scale {r['scale']}, spacing {r['spacing_px']} px**: "
                f"primitives `{r['kinds']}` "
                f"(expected alternating line/arc), closed {r['closed']}, {r['questions']} "
                "questions."
            )
    for r in res.get("shadowscan") or []:
        if "error" in r:
            continue
        if len(r["circle_diameters_mm"]) != len(r["holes_truth_mm"]):
            nf += 1
            w(
                f"- **ShadowScan `{Path(r['image']).name}`**: {len(r['circle_diameters_mm'])} "
                "analytic circles for "
                f"{len(r['holes_truth_mm'])} true holes."
            )
        bad = [e for e in r["hole_errors_mm"] if abs(e) > r["hole_tol_mm"]]
        if bad:
            nf += 1
            w(
                f"- **ShadowScan `{Path(r['image']).name}`**: hole errors beyond the corpus "
                "tolerance "
                f"±{r['hole_tol_mm']} mm: {bad}."
            )
        if r["equal_radius"].get("different_truth/proposed"):
            nf += 1
            w(
                f"- **ShadowScan `{Path(r['image']).name}`**: "
                f"{r['equal_radius']['different_truth/proposed']} "
                "equal-radius proposals between circles of different true diameter."
            )
        if r["n_hypotheses"] > 200:
            nf += 1
            w(
                f"- **ShadowScan `{Path(r['image']).name}`**: {r['n_hypotheses']} hypotheses / "
                f"{r['questions']} "
                "questions - pairwise hypotheses do not scale to many features."
            )
    ws = res.get("washer")
    if ws:
        lines = [x for x in ws["primitives"] if x["kind"] == "line"]
        perps = [c for c in ws["constraints"] if c[0] == "perpendicular" and c[2] == "proposed"]
        if lines:
            nf += 1
            w(
                f"- **washer scan-0053**: the round washer's edges are not single circles within "
                f"{ws['tolerance_mm']:.3f} mm; they split into arcs and {len(lines)} short lines."
            )
        if perps:
            nf += 1
            w(
                f"- **washer scan-0053**: {len(perps)} perpendicular constraints PROPOSED "
                "between those spurious short "
                "lines (each ~5 deg from 90, inside the band because the lines are short): "
                "angular constraints on short "
                "segments are weakly determined and the decision rule does not say so."
            )
    if not nf:
        w("- none")
    w("")
    for setname in ("dev", "heldout"):
        rows = res[setname]
        title = (
            "Development fixtures (used while writing the resolver)"
            if setname == "dev"
            else "Held-out fixtures (first run after the freeze; mostly raster-traced contours)"
        )
        w(f"## {title}")
        w("")
        w(
            "| case | sampler | σ px | points | runtime ms | GT Hausdorff (worst loop) | false "
            "proposals | "
            "missed | true→question | expectations |"
        )
        w("|---|---|---|---|---|---|---|---|---|---|")
        for r in rows:
            ts = r["truth_summary"]
            ex = r["expectations"]
            fails = [n for n, ok, _ in ex if not ok]
            exs = f"{sum(ok for _, ok, _ in ex)}/{len(ex)}" + (
                f" (fail: {', '.join(fails)})" if fails else ""
            )
            w(
                f"| {r['name']} | {r['sampler']} | {r['noise_px']} | {r['n_points']} | "
                f"{r['runtime_ms']:.0f} | "
                f"{_fmt_disc(r['gt_discrepancy'])} | {ts.get('FALSE_PROPOSAL', 0)} | "
                f"{ts.get('MISSED', 0)} | "
                f"{ts.get('true_but_question', 0)} | {exs} |"
            )
        tot = Counter()
        for r in rows:
            tot.update(r["truth_summary"])
        w("")
        w(
            f"Constraint decisions scored against GT ({setname}): "
            + ", ".join(f"{k} {v}" for k, v in sorted(tot.items()))
        )
        w("")
        fails = [(r["name"], n, got) for r in rows for n, ok, got in r["expectations"] if not ok]
        if fails:
            w("Failed expectations (reported, not tuned away):")
            w("")
            for name, n, got in fails:
                w(f"- `{name}` / {n}: got `{got}`")
            w("")
        notable = [(r["name"], q) for r in rows for q in r["questions"]]
        if notable:
            w("<details><summary>Questions raised</summary>")
            w("")
            for name, q in notable:
                w(f"- `{name}`: {q}")
            w("")
            w("</details>")
            w("")
    w("## Noise sensitivity (perpendicular decisions)")
    w("")
    w(
        "Each row: the same shape re-sampled with independent seeds. Counts are over 4 corner "
        "hypotheses × runs. A *proposed* perpendicular on a skewed shape is silent perfection."
    )
    w("")
    w(
        "Tolerance policies: *fixed* = the default 2 px; *matched* = max(2, 3.5 σ), the documented "
        "rule that the band must cover the contour's own noise (σ known here; on real data it must "
        "be measured from the contour source, never chosen to get an answer)."
    )
    w("")
    w(
        "| shape | policy | tol px | σ px | runs | runs with 4 lines | perpendicular decisions | "
        "GT Hausdorff median / max px | slowest run s |"
    )
    w("|---|---|---|---|---|---|---|---|---|")
    for r in res["noise"]:
        w(
            f"| {r['shape']} | {r['policy']} | {r['tolerance_px']:g} | {r['sigma_px']} | "
            f"{r['runs']} | "
            f"{r['four_lines']} | "
            f"{', '.join(f'{k} {v}' for k, v in sorted(r['perpendicular'].items())) or '-'} | "
            f"{r['hausdorff_px_median']:.2f} / {r['hausdorff_px_max']:.2f} | "
            f"{r['runtime_s_max']:.1f} |"
        )
    w("")
    w("## Scale and sampling density (rounded rectangle, σ 0.3 px)")
    w("")
    w(
        "| scale | spacing px | points | primitives (l/a) | expected kinds | closed | GT "
        "Hausdorff px | questions | runtime ms |"
    )
    w("|---|---|---|---|---|---|---|---|---|")
    for r in res["density"]:
        w(
            f"| {r['scale']} | {r['spacing_px']} | {r['n_points']} | {r['kinds']} | "
            f"{r['kinds_ok']} | {r['closed']} | "
            f"{r['hausdorff_px']:.2f} | {r['questions']} | {r['runtime_ms']:.0f} |"
        )
    w("")
    w("## External evaluation: ShadowScan synthetic renders")
    w("")
    if res.get("shadowscan") is None:
        w("Not run (no ShadowScan checkout given).")
    else:
        w(
            "Contours traced from `corpus/photos/*.png` with a plain Otsu threshold (this "
            "harness, not "
            "ShadowScan's pipeline). Scale = corpus `ref-width` / traced outer width, so the "
            "outer width "
            "is not an independent check; hole diameters are. Renders are synthetic."
        )
        w("")
        w(
            "| image | loops | kinds | hole truth mm | circles found | circle Ø mm | error to "
            "nearest truth mm (corpus tol) "
            "| equal radius (circle pairs scored) | hypotheses | questions | closed loops |"
        )
        w("|---|---|---|---|---|---|---|---|---|---|---|")
        for r in res["shadowscan"]:
            if "error" in r:
                w(f"| {r['image']} | {r['error']} | | | | | | | |")
                continue
            kinds = "; ".join("".join(k[0] for k in ks) for ks in r["kinds"])
            if len(kinds) > 120:
                kinds = kinds[:120] + "..."
            truth = Counter(r["holes_truth_mm"])
            w(
                f"| {Path(r['image']).name} | {r['n_loops']} | {kinds} | "
                f"{', '.join(f'{d:g}x{n}' for d, n in sorted(truth.items()))} | "
                f"{len(r['circle_diameters_mm'])}/{len(r['holes_truth_mm'])} | "
                f"{r['circle_diameters_mm']} | "
                f"{r['hole_errors_mm']} (±{r['hole_tol_mm']}) | {r['equal_radius']} | "
                f"{r['n_hypotheses']} | "
                f"{r['questions']} | {sum(r['closed'])}/{len(r['closed'])} |"
            )
        w("")
        w("Kinds legend: l line, a arc, c circle, f freeform.")
    w("")
    w("## External evaluation: real washer scan-0053 (PlatypusOne PR #39 evidence)")
    w("")
    ws = res.get("washer")
    if ws is None:
        w("Not run (PR #39 evidence not reachable from this checkout).")
    else:
        w(
            "Raw frame `source.yuyv`, segmented with the same threshold rule and "
            "largest-component "
            f"choice as `offline/washer_circle_fit.py` (threshold {ws['threshold']:.1f}), "
            f"traced with OpenCV, scaled with the on-device {ws['mm_per_px']:.5f} mm/px "
            f"(reference-square area scale; print scale UNRESOLVED in the run record). Tolerance "
            f"{ws['tolerance_mm']:.3f} mm. Nothing was tuned on this capture."
        )
        w("")
        w("| primitive | loop | kind | Ø mm | fit residual max / rms mm |")
        w("|---|---|---|---|---|")
        for x in ws["primitives"]:
            d = f"{x['diameter_mm']:.2f}" if x["diameter_mm"] else "-"
            rr = (
                f"{x['residual_max_mm']:.3f} / {x['residual_rms_mm']:.3f}"
                if x["residual_max_mm"] is not None
                else "-"
            )
            w(f"| {x['id']} | {x['loop']} | {x['kind']} | {d} | {rr} |")
        w("")
        w(
            f"Caliper truth (run record): outer {ws['caliper_mm']['outer']} mm, bore "
            f"{ws['caliper_mm']['bore']} mm. "
            f"PR #39 offline algebraic circle fit: {ws['offline_circle_fit_mm']['outer']} / "
            f"{ws['offline_circle_fit_mm']['bore']} mm."
        )
        w("")
        for c in ws["constraints"]:
            w(f"- constraint {c[0]} {'/'.join(c[1])}: **{c[2]}** - {c[3]}")
        for q in ws["questions"]:
            w(f"- question: {q}")
        for u in ws["unsupported"]:
            w(f"- unsupported: {u}")
        w("")
        w(
            "One capture, one part, one lighting condition: this is a smoke test of the path "
            "on real "
            "pixels, not a characterization. Any diameter error here is dominated by "
            "segmentation and the "
            "unverified print scale, which the resolver neither knows nor corrects."
        )
    w("")
    return "\n".join(L) + "\n"


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out")
    ap.add_argument("--json")
    ap.add_argument("--shadowscan")
    ap.add_argument(
        "--washer-ref", default="pr39", help="git ref carrying the PR #39 washer evidence"
    )
    ap.add_argument("--quick", action="store_true", help="smaller sweeps (CI smoke)")
    a = ap.parse_args(argv)

    from sketch_intent import RESOLVER_VERSION

    matches, frozen = freeze_check()
    res: dict[str, Any] = {
        "resolver_version": RESOLVER_VERSION,
        "params": dataclasses.asdict(Params()),
        "freeze": {"matches": matches, **(frozen or {})},
    }
    reps = 1 if a.quick else 3
    res["dev"] = [run_case(c, reps) for c in dev_cases()]
    res["heldout"] = [run_case(c, reps) for c in heldout_cases()]
    res["noise"] = noise_sweep(a.quick)
    res["density"] = [] if a.quick else density_sweep()
    res["shadowscan"] = shadowscan_eval(Path(a.shadowscan)) if a.shadowscan else None
    w = washer_eval(a.washer_ref)
    if w is not None:
        w.pop("proposal")
    res["washer"] = w
    md = report(res)
    if a.out:
        Path(a.out).write_text(md)
        print(f"wrote {a.out}")
    else:
        print(md)
    if a.json:
        Path(a.json).write_text(json.dumps(res, indent=1, default=str) + "\n")
    false_props = sum(
        r["truth_summary"].get("FALSE_PROPOSAL", 0) for r in res["dev"] + res["heldout"]
    )
    print(f"false proposals (dev + held-out): {false_props}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
