"""camera_characterize — evidence-to-decision pipeline for the IMX219 bench-off (#40).

  plan     [--markdown OUT]                      expand matrix_spec.json
  init     --camera SKU --dataset-id ID DIR      start a dataset (+ capture_plan.json)
  capture  DIR --command 'capture_raw.sh {out}'  guided capture on the UNO Q
  validate PATH                                  experiment or dataset; exit 1 on errors
  analyze  EXP                                   derived/<version>/analysis.json
  compare  EXP                                   derived/<version>/decision.json
  report   EXP                                   derived/<version>/report.md
  all      EXP                                   analyze + compare + report
  target   OUT.svg                               printable ChArUco board

plan/init/capture/validate need only the Python standard library.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent  # tools/camera_characterize
REPO = HERE.parent.parent
DEFAULT_SPEC = HERE / "matrix_spec.json"
DEFAULT_CRITERIA = HERE / "decision_criteria.json"


def find_scout_measure(explicit: str | None) -> Path | None:
    for cand in (
        explicit,
        os.environ.get("CAMCHAR_SCOUT_MEASURE"),
        str(REPO / "build/tools/scout_measure/scout_measure"),
        str(REPO / "build/tools/scout_measure/Release/scout_measure.exe"),
    ):
        if cand and Path(cand).is_file():
            return Path(cand)
    return None


def cmd_plan(a) -> int:
    from . import plan

    spec = json.loads(Path(a.spec).read_text())
    md = plan.plan_markdown(spec)
    if a.markdown:
        Path(a.markdown).write_text(md)
        print(f"wrote {a.markdown}")
    else:
        print(md)
    return 0


def cmd_init(a) -> int:
    from . import plan

    spec = json.loads(Path(a.spec).read_text())
    plan.init_dataset(spec, a.camera, Path(a.dir), a.dataset_id)
    print(f"initialised {a.dir}: fill camera/platform/bringup/physical, then `capture`")
    return 0


def cmd_capture(a) -> int:
    from . import capture

    return capture.run(Path(a.dir), a.command, a.cell)


def cmd_validate(a) -> int:
    from . import contract, plan

    p = Path(a.path)
    if (p / "experiment.json").is_file():
        _, datasets, exp_issues = contract.load_experiment(p, not a.no_hash)
    else:
        datasets, exp_issues = [contract.load_dataset(p, not a.no_hash)], []
    summary = contract.validation_summary(exp_issues, datasets)
    for i in summary["experiment_issues"]:
        print(f"{i['level']:5} experiment {i['where']}: {i['message']}")
    for d, ds in zip(summary["datasets"], datasets, strict=True):
        print(
            f"\n{d['dataset_id']} ({d['sku']}, {d['evidence_type']}): "
            f"{d['usable_frames']} usable, {d['failed_or_discarded_frames']} failed/discarded"
        )
        for i in d["issues"]:
            print(f"  {i['level']:5} {i['where']}: {i['message']}")
        for f in d["frames"]:
            for i in f["issues"]:
                print(f"  {i['level']:5} {i['where']}: {i['message']}")
        cov = plan.coverage(ds.root, ds.manifest)
        if cov:
            print(
                f"  plan coverage: {cov['captured_ok_frames']}/{cov['planned_frames']} frames, "
                f"{cov['cells_complete']}/{cov['cells_total']} cells complete"
            )
            if cov["missing"]:
                print(f"  INCOMPLETE: missing {cov['missing']}")
    print(f"\n{summary['error_count']} error(s)")
    return 1 if summary["error_count"] else 0


def _analysis_path(exp: Path) -> Path:
    from .analyze import derived_dir

    return derived_dir(exp) / "analysis.json"


def cmd_analyze(a) -> int:
    from . import analyze

    tool = find_scout_measure(a.scout_measure)
    if tool is None:
        print(
            "WARNING: scout_measure not found "
            "(build it: cmake --build build --target scout_measure);"
            " repeatability/accuracy will be reported as unavailable",
            file=sys.stderr,
        )
    try:
        _, out = analyze.run(Path(a.experiment), tool, a.min_views)
    except analyze.AnalysisRefused as exc:
        print(f"REFUSED: {exc}", file=sys.stderr)
        return 1
    print(f"wrote {out / 'analysis.json'}")
    return 0


def cmd_compare(a) -> int:
    from . import decision
    from .analyze import derived_dir, write_json

    analysis = json.loads(_analysis_path(Path(a.experiment)).read_text())
    criteria, sha = decision.load_criteria(Path(a.criteria))
    d = decision.decide(analysis, criteria, sha)
    out = derived_dir(Path(a.experiment)) / "decision.json"
    write_json(out, d)
    for role, r in d["decision"].items():
        print(f"{role:10} {r['outcome']:22} {r.get('camera') or r.get('cameras') or ''}")
    print(f"wrote {out}")
    return 0


def cmd_report(a) -> int:
    from . import report
    from .analyze import derived_dir

    root = derived_dir(Path(a.experiment))
    analysis = json.loads((root / "analysis.json").read_text())
    decision_ = json.loads((root / "decision.json").read_text())
    out = root / "report.md"
    out.write_text(report.render(analysis, decision_))
    print(f"wrote {out}")
    return 0


def cmd_all(a) -> int:
    rc = cmd_analyze(a)
    if rc:
        return rc
    return cmd_compare(a) or cmd_report(a)


def cmd_target(a) -> int:
    from .calibration import Board
    from .target import svg

    b = Board((7, 10), 20.0, 0.7, "DICT_4X4_50")
    Path(a.out).write_text(svg(b, "ChArUco 7x10 squares, 20 mm, markers 14 mm, DICT_4X4_50"))
    print(f"wrote {a.out}")
    return 0


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(
        prog="camera_characterize",
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = p.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("plan")
    s.add_argument("--spec", default=str(DEFAULT_SPEC))
    s.add_argument("--markdown")
    s.set_defaults(fn=cmd_plan)

    s = sub.add_parser("init")
    s.add_argument("dir")
    s.add_argument("--camera", required=True)
    s.add_argument("--dataset-id", required=True)
    s.add_argument("--spec", default=str(DEFAULT_SPEC))
    s.set_defaults(fn=cmd_init)

    s = sub.add_parser("capture")
    s.add_argument("dir")
    s.add_argument("--command", required=True, help="template with {out}")
    s.add_argument("--cell")
    s.set_defaults(fn=cmd_capture)

    s = sub.add_parser("validate")
    s.add_argument("path")
    s.add_argument("--no-hash", action="store_true")
    s.set_defaults(fn=cmd_validate)

    for name, fn in (
        ("analyze", cmd_analyze),
        ("compare", cmd_compare),
        ("report", cmd_report),
        ("all", cmd_all),
    ):
        s = sub.add_parser(name)
        s.add_argument("experiment")
        s.add_argument("--scout-measure")
        s.add_argument("--min-views", type=int, default=10)
        s.add_argument("--criteria", default=str(DEFAULT_CRITERIA))
        s.set_defaults(fn=fn)

    s = sub.add_parser("target")
    s.add_argument("out")
    s.set_defaults(fn=cmd_target)

    a = p.parse_args(argv)
    return a.fn(a)
