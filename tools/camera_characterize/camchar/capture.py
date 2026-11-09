"""Board-side guided capture: walk capture_plan.json and append frames.

Standard library only. For every remaining planned shot it prints the
instruction, waits for the operator, runs the capture command, hashes the new
file and appends a complete frame entry to dataset.json. Failures are kept
(outcome capture_failed / discarded, with the reason) - nothing is retried
into a pass, nothing is overwritten.

The capture command is a template; {out} is replaced by the new file's path.
It must write exactly that file and may print one JSON object on its last
stdout line with read-back state, e.g. from capture_raw.sh:
  {"format":"raw10p","width":1640,"height":1232,"stride":2056,
   "bayer_pattern":"RGGB","black_level":64,"white_level":1023,
   "exposure_us":..., "analogue_gain":..., "focus_position":...}
Read-back values override the dataset defaults for that frame.
"""

from __future__ import annotations

import json
import shlex
import subprocess
import sys
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

from . import UNKNOWN
from .contract import sha256_file

EXT = {
    "raw10p": "raw",
    "raw16": "raw",
    "abgr8888": "bin",
    "yuyv": "yuv",
    "png": "png",
    "pgm": "pgm",
    "ppm": "ppm",
}


def _remaining(plan: dict[str, Any], manifest: dict[str, Any]) -> list[dict[str, Any]]:
    done = {(f.get("plan_cell"), f.get("plan_index")) for f in manifest["frames"]}
    return [s for s in plan["shots"] if (s["plan_cell"], s["plan_index"]) not in done]


def _save(path: Path, manifest: dict[str, Any]) -> None:
    tmp = path.with_suffix(".json.tmp")
    tmp.write_text(json.dumps(manifest, indent=2) + "\n")
    tmp.replace(path)  # atomic: a crash never leaves a half-written manifest


def run(dataset: Path, command: str, only_cell: str | None = None, ask=input) -> int:
    mpath = dataset / "dataset.json"
    manifest = json.loads(mpath.read_text())
    plan = json.loads((dataset / "capture_plan.json").read_text())
    shots = [s for s in _remaining(plan, manifest) if only_cell in (None, s["plan_cell"])]
    print(f"{len(shots)} shots remaining in {dataset}")
    for s in shots:
        fid = f"{s['plan_cell']}-{s['plan_index']:02d}"
        print(
            f"\n[{fid}] {s['working_distance_mm']} mm | {s['scene']} | "
            f"{s['placement']} | {s['lighting']}"
        )
        print(f"  {s['instruction']}")
        ans = ask("  Enter = capture, s = skip for now, d = discard (reason), q = quit: ").strip()
        if ans == "q":
            break
        if ans == "s":
            continue
        entry: dict[str, Any] = {k: v for k, v in s.items() if k != "instruction"}
        entry["frame_id"] = fid
        entry["timestamp_utc"] = datetime.now(UTC).strftime("%Y-%m-%dT%H:%M:%SZ")
        if ans.startswith("d"):
            entry["outcome"] = "discarded"
            entry["failure_reason"] = ans[1:].strip() or ask("  reason: ").strip() or UNKNOWN
            manifest["frames"].append(entry)
            _save(mpath, manifest)
            continue
        ext = EXT.get(str(manifest["defaults"].get("pixel_format")), "bin")
        rel = Path("frames") / f"{fid}.{ext}"
        out = dataset / rel
        if out.exists():
            print(f"  ERROR {rel} already exists; refusing to overwrite", file=sys.stderr)
            return 1
        cmd = command.replace("{out}", shlex.quote(str(out)))
        entry["capture_command"] = command
        proc = subprocess.run(cmd, shell=True, capture_output=True, text=True, check=False)
        (dataset / "frames" / f"{fid}.log").write_text(
            f"$ {cmd}\n[exit {proc.returncode}]\n"
            f"--- stdout\n{proc.stdout}\n--- stderr\n{proc.stderr}"
        )
        if proc.returncode != 0 or not out.is_file():
            entry["outcome"] = "capture_failed"
            entry["failure_reason"] = f"exit {proc.returncode}; see frames/{fid}.log"
            print(f"  capture FAILED ({entry['failure_reason']})")
        else:
            readback: dict[str, Any] = {}
            lines = proc.stdout.strip().splitlines()
            if lines:
                try:
                    readback = json.loads(lines[-1])
                except json.JSONDecodeError:
                    readback = {}
            entry.update(readback)
            entry["path"] = str(rel)
            entry["sha256"] = sha256_file(out)
            entry.setdefault("format", manifest["defaults"].get("pixel_format", UNKNOWN))
            entry["outcome"] = "ok"
            note = ask("  note (optional): ").strip()
            if note:
                entry["notes"] = note
            print(f"  saved {rel} sha256 {entry['sha256'][:12]}")
        manifest["frames"].append(entry)
        _save(mpath, manifest)
    return 0
