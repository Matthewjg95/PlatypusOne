"""Reproducible end-to-end demo on SYNTHETIC fixtures, through the real CLI.

    PYTHONPATH=services/sketch_intent python3 services/sketch_intent/demo/run_demo.py \
        [--out services/sketch_intent/demo/out] [--mesh2cad ../mesh2cad] [--png]

For each fixture: input JSON -> `resolve` (proposal + overlay SVG) -> `review`
(accept what the resolver proposed; questions are answered explicitly below,
never by default) -> `export` (Mesh2CAD sketch asset, plus DXF / SVG when a
Mesh2CAD checkout is given) -> `overlay` of the accepted geometry. Refusals are
part of the demo: an open loop, freeform evidence and an uncalibrated input
must NOT export.

The reviewer here is a script and is recorded as such ("demo script").
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests"))

from fixtures import dev_cases, realize  # noqa: E402

# fixture -> (questions to accept, questions to reject); everything else proposed is accepted
PLAN = {
    "noisy_rectangle": ([], []),
    "skewed_quadrilateral": ([], []),
    "rounded_rectangle": ([], "all-questions"),
    "plate_two_holes_and_slot": ([], "all-questions"),
    "missing_segment": ([], []),  # the gap stays open: export must refuse
    "outliers": ([], []),
    "freeform_blob": ([], []),  # unsupported: export must refuse
    "uncalibrated_rectangle": ([], []),  # insufficient calibration: export must refuse
}
CHROME = Path("/opt/pw-browsers/chromium-1194/chrome-linux/chrome")


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "demo" / "out"))
    ap.add_argument("--mesh2cad", help="Mesh2CAD checkout with mra.sketch_assets (PR #16)")
    ap.add_argument(
        "--png", action="store_true", help="also rasterise overlays with headless Chromium"
    )
    a = ap.parse_args(argv)
    out = Path(a.out)
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    env_py = [sys.executable, "-m", "sketch_intent"]
    cases = {c.name: c for c in dev_cases()}
    summary = []

    def cli(*args: str) -> subprocess.CompletedProcess:
        r = subprocess.run(
            [*env_py, *args],
            cwd=out,
            capture_output=True,
            text=True,
            env={"PYTHONPATH": str(ROOT), "PATH": "/usr/bin:/bin"},
        )
        (out / "log.txt").open("a").write(
            f"$ sketch_intent {' '.join(args)}\n{r.stdout}{r.stderr}\n"
        )
        return r

    for name, (acc_q, rej_q) in PLAN.items():
        data, _ = realize(cases[name])
        (out / f"{name}.json").write_text(json.dumps(data))
        r = cli(
            "resolve",
            f"{name}.json",
            "-o",
            f"{name}.proposal.json",
            "--svg",
            f"{name}.proposed.svg",
        )
        if r.returncode:
            print(r.stderr)
            return 1
        prop = json.loads((out / f"{name}.proposal.json").read_text())
        questions = [c["id"] for c in prop["constraints"] if c["decision"] == "question"]
        questions += [p["id"] for p in prop["primitives"] if p["decision"] == "question"]
        reject = questions if rej_q == "all-questions" else list(rej_q)
        args = [
            "review",
            f"{name}.proposal.json",
            "-o",
            f"{name}.reviewed.json",
            "--by",
            "demo script",
            "--accept-proposed",
            "--note",
            "automated demo review",
        ]
        if acc_q:
            args += ["--accept", *acc_q]
        if reject:
            args += ["--reject", *reject]
        r = cli(*args)
        if r.returncode:
            print(r.stderr)
            return 1
        exp = [
            "export",
            f"{name}.reviewed.json",
            "--input",
            f"{name}.json",
            "-o",
            f"{name}.sketch.json",
            "--report",
            f"{name}.export-report.json",
        ]
        if a.mesh2cad:
            exp += [
                "--mesh2cad",
                str(Path(a.mesh2cad).resolve()),
                "--dxf",
                f"{name}.dxf",
                "--svg",
                f"{name}.m2c.svg",
            ]
        r = cli(*exp)
        exported = r.returncode == 0
        refusal = "" if exported else r.stderr.strip().removeprefix("REFUSED: ")
        if exported:
            cli(
                "overlay",
                f"{name}.reviewed.json",
                "--input",
                f"{name}.json",
                "-o",
                f"{name}.accepted.svg",
            )
        if a.png and CHROME.exists():
            for svg in sorted(out.glob(f"{name}.*.svg")):
                subprocess.run(
                    [
                        str(CHROME),
                        "--headless",
                        "--no-sandbox",
                        f"--screenshot={svg.with_suffix('.png')}",
                        "--window-size=1400,1000",
                        f"file://{svg.resolve()}",
                    ],
                    capture_output=True,
                )
        summary.append(
            {
                "fixture": name,
                "questions": len(prop["questions"]),
                "unsupported": len(prop["unsupported"]),
                "rejected_questions": reject,
                "exported": exported,
                "refusal": refusal,
            }
        )
        status = "exported" if exported else f"REFUSED ({refusal})"
        print(
            f"{name:28s} questions {len(prop['questions'])}  unsupported "
            f"{len(prop['unsupported'])}  {status}"
        )
    (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"outputs in {out} (log.txt has every CLI call)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
