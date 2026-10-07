"""Export reviewed, accepted geometry through Mesh2CAD's sketch-asset contract.

Target: Mesh2CAD ``mra.sketch_assets`` schema 1.1 (PR #16, stdlib-only):
closed ``profiles`` of analytic line / arc segments plus ``circles``. That
library owns DXF / SVG (and, with OpenCascade, STEP) export; this module only
produces a valid asset dict, so there is one exporter, not two.

Refuses to export when:
  - the input is uncalibrated (sketch assets are in mm / in, and we will not
    invent a scale);
  - the input file no longer hashes to the proposal's input, or re-running the
    resolver gives a different proposal_id (evidence or code changed);
  - a loop has an undecided or rejected primitive, an open junction, or
    freeform content - that loop is skipped and listed, never approximated.

Frame: x_mm = (x_px - x0) * k, y_mm = (y0 - y_px) * k with (x0, y0) the
outer loop's bounding-box left / bottom in pixels, so the sketch is Y-up with
its lower-left at the origin. Both are recorded in provenance.
"""

from __future__ import annotations

import sys
from pathlib import Path
from typing import Any

import numpy as np

from . import RESOLVER_VERSION
from .contract import Observation, Params
from .resolve import analyze, proposal_id
from .review import accepted_sets
from .solve import solve


class ExportError(ValueError):
    pass


def accepted_geometry(
    proposal: dict[str, Any], obs: Observation
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    if obs.source_sha256 != proposal["input"]["sha256"]:
        raise ExportError("input file does not match the proposal's input sha256: evidence changed")
    prm = Params(**{k: v for k, v in proposal["parameters"].items() if k != "tolerance_mm"})
    if proposal_id(obs, prm) != proposal["proposal_id"]:
        raise ExportError(
            "re-running the resolver gives a different proposal_id (resolver version changed?)"
        )
    loops, ctx, hyps = analyze(obs, prm)
    acc, rej = accepted_sets(proposal)
    applied = [h for h in hyps if h.cid in acc]
    geom = solve(ctx, applied)
    skipped = []
    for lo, la in zip(geom["loops"], loops, strict=True):
        reasons = []
        for p in la.primitives:
            if p.pid in rej:
                reasons.append(f"{p.pid} rejected")
            elif p.pid not in acc:
                reasons.append(f"{p.pid} not accepted")
        if lo["open_junctions"]:
            reasons += [
                f"open junction after {j['after']}: {j['reason']}" for j in lo["open_junctions"]
            ]
        if not lo["closed_profile"] and not reasons:
            reasons.append("not a closed analytic profile")
        lo["exportable"] = not reasons
        if reasons:
            skipped.append({"loop_id": lo["loop_id"], "reasons": reasons})
    return geom, skipped


def to_sketch_asset(
    proposal: dict[str, Any], obs: Observation, name: str
) -> tuple[dict[str, Any], dict[str, Any]]:
    if not obs.calibrated:
        raise ExportError(
            "insufficient calibration: the input has no scale, so no mm geometry can be exported"
        )
    geom, skipped = accepted_geometry(proposal, obs)
    k = obs.scale_mm_per_px
    outer = next((lp for lp in obs.loops if lp.role == "outer"), obs.loops[0])
    x0 = float(outer.points[:, 0].min())
    y0 = float(outer.points[:, 1].max())

    def tx(p) -> list[float]:
        return [(float(p[0]) - x0) * k, (y0 - float(p[1])) * k]

    profiles, circles = [], []
    for lo in geom["loops"]:
        if not lo["exportable"]:
            continue
        ents = lo["entities"]
        role = "outline" if lo["role"] == "outer" else "cutout"
        if len(ents) == 1 and ents[0]["kind"] == "circle":
            e = ents[0]
            circles.append(
                {
                    "center": tx(e["center"]),
                    "diameter": 2 * e["radius"] * k,
                    "role": "outline" if lo["role"] == "outer" else "hole",
                }
            )
            continue
        segs = []
        for e in ents:
            if e["kind"] == "line":
                segs.append({"type": "line", "start": tx(e["start"]), "end": tx(e["end"])})
            else:
                # image y-down -> CAD y-up negates angles: increasing image angle is clockwise in
                # CAD
                segs.append(
                    {
                        "type": "arc",
                        "start": tx(e["start"]),
                        "end": tx(e["end"]),
                        "center": tx(e["center"]),
                        "ccw": e["sweep_sign_image"] < 0,
                    }
                )
        # exact continuity: each segment starts where the previous ended
        for i in range(len(segs)):
            segs[i]["start"] = segs[i - 1]["end"]
        profiles.append({"segments": segs, "role": role})
    if not profiles and not circles:
        raise ExportError(
            "nothing exportable: "
            + "; ".join(f"{s['loop_id']}: {', '.join(s['reasons'])}" for s in skipped)
        )
    acc, _ = accepted_sets(proposal)
    asset = {
        "name": name,
        "units": "mm",
        "origin": [0.0, 0.0],
        "circles": circles,
        "polygons": [],
        "profiles": profiles,
        "metadata": {"tags": ["sketch-intent", "reviewed-proposal"]},
        "provenance": {
            "producer": RESOLVER_VERSION,
            "proposal_id": proposal["proposal_id"],
            "input_format": proposal["input"]["format"],
            "input_sha256": proposal["input"]["sha256"],
            "source_ids": proposal["input"]["source_ids"],
            "calibration": proposal["input"]["calibration"],
            "mm_per_px": k,
            "frame": {"origin_px": [x0, y0], "y_axis": "flipped (image y-down -> sketch y-up)"},
            "accepted_items": sorted(acc),
            "reviewed_by": sorted({d["by"] for d in proposal["review"]["decisions"]}),
            "skipped_loops": skipped,
            "note": (
                "geometry is reviewed design intent derived from a measured contour, "
                "not measured truth"
            ),
        },
        "schema_version": "1.1",
    }
    report = {"accepted_geometry": geom, "skipped_loops": skipped}
    return asset, report


def mesh2cad_export(
    asset: dict[str, Any], mesh2cad_root: Path, dxf: Path | None, svg: Path | None
) -> dict[str, Any]:
    """Validate with Mesh2CAD's own model and write DXF / SVG with its exporters."""
    sys.path.insert(0, str(mesh2cad_root))
    try:
        from mra.sketch_assets import SketchAsset, export_dxf, export_svg  # type: ignore
    except ImportError as exc:
        raise ExportError(f"mra.sketch_assets not importable from {mesh2cad_root}: {exc}") from exc
    finally:
        sys.path.pop(0)
    model = SketchAsset.from_dict(asset)
    out: dict[str, Any] = {"validated_by": "mra.sketch_assets.SketchAsset.from_dict"}
    if dxf is not None:
        export_dxf(model, dxf)
        out["dxf"] = str(dxf)
    if svg is not None:
        export_svg(model, svg)
        out["svg"] = str(svg)
    return out


def bbox_mm(asset: dict[str, Any]) -> list[float]:
    pts = [
        p for prof in asset["profiles"] for s in prof["segments"] for p in (s["start"], s["end"])
    ]
    pts += [c["center"] for c in asset["circles"]]
    a = np.array(pts)
    return [float(a[:, 0].min()), float(a[:, 1].min()), float(a[:, 0].max()), float(a[:, 1].max())]
