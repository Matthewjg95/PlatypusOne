"""observation -> primitive hypotheses -> constraint hypotheses -> proposal.

The proposal (schema platypus.sketch_proposal/1) is a pure function of the
input bytes, the parameters and RESOLVER_VERSION, so ``proposal_id`` is a
hash of exactly those three. Review and export re-run the resolver from the
original input and refuse to continue if that id or the input hash differs.
"""

from __future__ import annotations

import hashlib
import json
from dataclasses import asdict
from typing import Any

import numpy as np

from . import PROPOSAL_SCHEMA, RESOLVER_VERSION
from .constraints import Context, Hypothesis, all_hypotheses
from .contract import FRAME_PX, Observation, Params
from .segment import LoopAnalysis, Primitive, analyze_loop
from .solve import solve


def proposal_id(obs: Observation, prm: Params) -> str:
    key = json.dumps(
        {"input": obs.source_sha256, "params": asdict(prm), "resolver": RESOLVER_VERSION},
        sort_keys=True,
    )
    return hashlib.sha256(key.encode()).hexdigest()[:20]


def analyze(obs: Observation, prm: Params) -> tuple[list[LoopAnalysis], Context, list[Hypothesis]]:
    prm.validate()
    counter = [0]
    loops = [analyze_loop(lp, prm, counter) for lp in obs.loops]
    ctx = Context(loops, prm, obs.scale_mm_per_px)
    return loops, ctx, all_hypotheses(ctx)


def _prim_dict(p: Primitive, ctx: Context) -> dict[str, Any]:
    d: dict[str, Any] = {
        "id": p.pid,
        "loop_id": p.loop_id,
        "kind": p.kind,
        "point_indices": [int(i) for i in p.idx],
        "n_points": int(len(p.idx)),
        "notes": p.notes,
        "alternatives": p.alternatives,
        "ambiguous": p.ambiguous,
    }
    f = p.fit
    if p.kind == "line":
        d["fit"] = {
            "model": "total least squares line",
            "point_px": f.centroid.tolist(),
            "direction": f.direction.tolist(),
            "angle_deg_image": float(
                np.degrees(np.arctan2(f.direction[1], f.direction[0])) % 180.0
            ),
        }
    elif p.kind in ("arc", "circle"):
        d["fit"] = {
            "model": "geometric least squares circle",
            "center_px": f.center.tolist(),
            "radius_px": f.radius,
            "radius_mm": ctx.mm(f.radius),
            "diameter_mm": ctx.mm(2 * f.radius),
            "span_deg": float(np.degrees(f.span_rad)),
        }
    if f is not None:
        d["residual"] = {
            "max_px": f.max_abs,
            "rms_px": f.rms,
            "max_mm": ctx.mm(f.max_abs),
            "rms_mm": ctx.mm(f.rms),
        }
    if p.kind == "freeform":
        d["decision"] = "unsupported"
    elif p.ambiguous:
        d["decision"] = "question"
    else:
        d["decision"] = "proposed"
    return d


def _hyp_dict(h: Hypothesis) -> dict[str, Any]:
    return {
        "id": h.cid,
        "type": h.ctype,
        "entities": h.entities,
        "decision": h.decision,
        "measured": h.measured,
        "correction": h.correction,
        "constrained_fit": h.constrained_fit,
        "reason": h.reason,
        "flags": h.flags,
    }


def resolve(obs: Observation, prm: Params) -> dict[str, Any]:
    loops, ctx, hyps = analyze(obs, prm)
    k = obs.scale_mm_per_px
    prims = [_prim_dict(p, ctx) for la in loops for p in la.primitives]
    proposed = [h for h in hyps if h.decision == "proposed"]
    geom = solve(ctx, proposed)

    questions = []
    for p in prims:
        if p["decision"] == "question":
            questions.append(
                {
                    "about": [p["id"]],
                    "kind": "competing_interpretations",
                    "text": f"{p['id']}: " + "; ".join(p["notes"]),
                    "options": [a["interpretation"] for a in p["alternatives"] if a["fits"]]
                    + ["reject"],
                }
            )
    for h in hyps:
        if h.decision == "question":
            questions.append(
                {
                    "about": [h.cid],
                    "kind": h.ctype,
                    "text": f"{h.cid} {h.ctype} {'/'.join(h.entities)}: {h.reason}. "
                    + (
                        "Inside the band, but the evidence fits clearly worse with it "
                        "(RMS inflation): accept as design intent?"
                        if "evidence_distinguishes" in h.flags
                        else "Beyond the evidence band: accept as design intent?"
                    ),
                    "options": ["accept", "reject"],
                }
            )
    # Constraints that pass one at a time can still overshoot together.
    joint = [
        (lo["loop_id"], e["pid"], e["discrepancy"]["max_px"])
        for lo in geom["loops"]
        for e in lo["entities"]
        if e["discrepancy"]["exceeds_tolerance"]
    ]
    for loop_id, pid, mx in joint:
        involved = [h.cid for h in proposed if pid in h.entities]
        questions.append(
            {
                "about": [pid] + involved,
                "kind": "joint_excess",
                "text": f"{pid} ({loop_id}): the proposed constraints applied together move it "
                f"{mx:.2f} px from its evidence, beyond the {prm.tolerance_px:g} px band "
                f"(each passed alone). Review {', '.join(involved) or 'its fit'}.",
                "options": ["accept", "reject one of the constraints"],
            }
        )
    ambiguities = (
        [{"about": [pid], "kind": "joint_excess", "max_px": mx} for _, pid, mx in joint]
        + [
            {
                "about": [p["id"]],
                "kind": "competing_interpretations",
                "alternatives": p["alternatives"],
            }
            for p in prims
            if p["ambiguous"]
        ]
        + [
            {"about": [h.cid], "kind": "near_threshold", "decision": h.decision, "reason": h.reason}
            for h in hyps
            if "near_threshold" in h.flags
        ]
        + [
            {"about": [c["constraint"]], "kind": "conflict", "reason": c["reason"]}
            for c in geom["conflicts"]
        ]
    )

    unsupported = [
        {"about": [p["id"]], "reason": "; ".join(p["notes"]) or "freeform"}
        for p in prims
        if p["kind"] == "freeform"
    ] + [{"about": [h.cid], "reason": h.reason} for h in hyps if h.decision == "unsupported"]
    for lo in geom["loops"]:
        if not lo["closed_profile"]:
            unsupported.append(
                {
                    "about": [lo["loop_id"]],
                    "reason": "proposed geometry is not a closed analytic profile: "
                    + "; ".join(j["reason"] for j in lo["open_junctions"])
                    or "freeform content",
                }
            )
    if not obs.calibrated:
        unsupported.append(
            {
                "about": ["input.calibration"],
                "reason": "no scale in the input: all values are in pixels; physical dimensions "
                "and mm export are unavailable (insufficient calibration)",
            }
        )

    evidence = []
    for la in loops:
        evidence.append(
            {
                "loop_id": la.loop.loop_id,
                "role": la.loop.role,
                "closed": la.loop.closed,
                "n_points": int(len(la.loop.points)),
                "points_px": la.loop.points.tolist(),
                "gaps": la.gaps,
                "outliers": la.outliers,
            }
        )

    return {
        "schema": PROPOSAL_SCHEMA,
        "resolver_version": RESOLVER_VERSION,
        "proposal_id": proposal_id(obs, prm),
        "input": {
            "format": obs.source_format,
            "sha256": obs.source_sha256,
            "source_ids": obs.source_ids,
            "coordinate_frame": FRAME_PX,
            "units": "px",
            "scale_mm_per_px": k,
            "calibration": obs.calibration,
            "unresolved": obs.unresolved,
            "carried": obs.carried,
        },
        "parameters": {**asdict(prm), "tolerance_mm": None if k is None else prm.tolerance_px * k},
        "evidence": {
            "note": "original points, unmodified; outliers and gaps are flagged, never removed",
            "loops": evidence,
        },
        "primitives": prims,
        "constraints": [_hyp_dict(h) for h in hyps],
        "questions": questions,
        "ambiguities": ambiguities,
        "unsupported": unsupported,
        "proposed_geometry": {
            "applied_constraints": [h.cid for h in proposed],
            "note": (
                "geometry with only the 'proposed' constraints applied; questions are NOT "
                "applied. This is inferred intent, not measurement."
            ),
            **geom,
        },
        "review": {"state": "pending", "decisions": []},
    }
