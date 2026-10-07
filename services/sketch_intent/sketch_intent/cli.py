"""sketch_intent CLI - the whole path without a separate application.

  resolve INPUT -o proposal.json [--svg overlay.svg] [--tolerance-px 2.0]
  show    PROPOSAL                                     summary + questions
  review  PROPOSAL -o reviewed.json --by NAME [--accept-proposed]
          [--accept ID ...] [--reject ID ...] [--override-note TEXT]
  export  REVIEWED --input INPUT -o asset.sketch.json [--name NAME]
          [--mesh2cad PATH [--dxf out.dxf] [--svg out.svg]] [--report r.json]
  overlay REVIEWED --input INPUT -o accepted.svg       accepted geometry overlay

Run: PYTHONPATH=services/sketch_intent python3 -m sketch_intent ...
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import fields
from pathlib import Path

from .contract import InputError, Params, load_observation


def _params(a) -> Params:
    kw = {
        f.name: getattr(a, f.name) for f in fields(Params) if getattr(a, f.name, None) is not None
    }
    return Params(**kw)


def cmd_resolve(a) -> int:
    from .overlay import render
    from .resolve import resolve

    obs = load_observation(a.input)
    proposal = resolve(obs, _params(a))
    out = Path(a.output)
    if out.exists() and not a.force:
        print(f"{out} exists (use --force to replace a proposal)", file=sys.stderr)
        return 1
    out.write_text(json.dumps(proposal, indent=2) + "\n")
    print(f"wrote {out} (proposal {proposal['proposal_id']})")
    if a.svg:
        Path(a.svg).write_text(render(proposal))
        print(f"wrote {a.svg}")
    _summary(proposal)
    return 0


def _summary(p) -> None:
    from collections import Counter

    kinds = Counter(x["kind"] for x in p["primitives"])
    decs = Counter(c["decision"] for c in p["constraints"])
    cal = p["input"]["calibration"]["status"]
    print(f"primitives: {dict(kinds)} | constraints: {dict(decs)} | calibration: {cal}")
    for lo in p["proposed_geometry"]["loops"]:
        d = lo["discrepancy"]
        mm = f" ({d['max_mm']:.3f} mm)" if d.get("max_mm") is not None else ""
        print(
            f"  {lo['loop_id']} {lo['role']}: max discrepancy {d['max_px']:.2f} px{mm}, closed "
            f"profile {lo['closed_profile']}"
        )
    for q in p["questions"]:
        print(f"  QUESTION {q['text']}  options: {q['options']}")
    for u in p["unsupported"]:
        print(f"  UNSUPPORTED {','.join(u['about'])}: {u['reason']}")


def cmd_show(a) -> int:
    p = json.loads(Path(a.proposal).read_text())
    print(f"proposal {p['proposal_id']} ({p['resolver_version']}), review {p['review']['state']}")
    _summary(p)
    for d in p["review"]["decisions"]:
        print(
            f"  {d['item']}: {d['decision']} by {d['by']} at {d['at']}"
            + (f" OVERRIDE: {d['override']}" if d.get("override") else "")
        )
    return 0


def cmd_review(a) -> int:
    from .review import ReviewError, review_file

    try:
        out = review_file(
            Path(a.proposal),
            Path(a.output),
            accept=a.accept or [],
            reject=a.reject or [],
            by=a.by,
            accept_proposed=a.accept_proposed,
            override_note=a.override_note,
            note=a.note,
        )
    except ReviewError as exc:
        print(f"REFUSED: {exc}", file=sys.stderr)
        return 1
    print(
        f"wrote {a.output}: review {out['review']['state']}"
        + (f", undecided {out['review']['undecided']}" if out["review"]["undecided"] else "")
    )
    return 0


def cmd_export(a) -> int:
    from .export import ExportError, mesh2cad_export, to_sketch_asset

    proposal = json.loads(Path(a.reviewed).read_text())
    obs = load_observation(a.input)
    try:
        asset, report = to_sketch_asset(
            proposal, obs, a.name or f"sketch-intent-{proposal['proposal_id'][:8]}"
        )
        out = Path(a.output)
        out.write_text(json.dumps(asset, indent=2) + "\n")
        print(f"wrote {out}")
        for s in report["skipped_loops"]:
            print(f"  SKIPPED {s['loop_id']}: {'; '.join(s['reasons'])}")
        if a.report:
            Path(a.report).write_text(json.dumps(report, indent=2) + "\n")
        if a.mesh2cad:
            r = mesh2cad_export(
                asset,
                Path(a.mesh2cad),
                Path(a.dxf) if a.dxf else None,
                Path(a.svg) if a.svg else None,
            )
            print(f"  mesh2cad: {r}")
    except ExportError as exc:
        print(f"REFUSED: {exc}", file=sys.stderr)
        return 1
    return 0


def cmd_overlay(a) -> int:
    from .export import ExportError, accepted_geometry
    from .overlay import render

    proposal = json.loads(Path(a.reviewed).read_text())
    obs = load_observation(a.input)
    try:
        geom, _ = accepted_geometry(proposal, obs)
    except ExportError as exc:
        print(f"REFUSED: {exc}", file=sys.stderr)
        return 1
    from .review import accepted_sets

    geom["applied_constraints"] = sorted(accepted_sets(proposal)[0])
    Path(a.output).write_text(render(proposal, geom, title="accepted"))
    print(f"wrote {a.output}")
    return 0


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(
        prog="sketch_intent",
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = p.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("resolve")
    s.add_argument("input")
    s.add_argument("-o", "--output", required=True)
    s.add_argument("--svg")
    s.add_argument("--force", action="store_true")
    for f in fields(Params):
        s.add_argument("--" + f.name.replace("_", "-"), dest=f.name, type=type(f.default))
    s.set_defaults(fn=cmd_resolve)
    s = sub.add_parser("show")
    s.add_argument("proposal")
    s.set_defaults(fn=cmd_show)
    s = sub.add_parser("review")
    s.add_argument("proposal")
    s.add_argument("-o", "--output", required=True)
    s.add_argument("--by", required=True)
    s.add_argument("--accept", nargs="*")
    s.add_argument("--reject", nargs="*")
    s.add_argument("--accept-proposed", action="store_true")
    s.add_argument("--override-note")
    s.add_argument("--note")
    s.set_defaults(fn=cmd_review)
    s = sub.add_parser("export")
    s.add_argument("reviewed")
    s.add_argument("--input", required=True)
    s.add_argument("-o", "--output", required=True)
    s.add_argument("--name")
    s.add_argument("--report")
    s.add_argument("--mesh2cad")
    s.add_argument("--dxf")
    s.add_argument("--svg")
    s.set_defaults(fn=cmd_export)
    s = sub.add_parser("overlay")
    s.add_argument("reviewed")
    s.add_argument("--input", required=True)
    s.add_argument("-o", "--output", required=True)
    s.set_defaults(fn=cmd_overlay)
    a = p.parse_args(argv)
    try:
        return a.fn(a)
    except InputError as exc:
        print(f"INPUT ERROR: {exc}", file=sys.stderr)
        return 2
