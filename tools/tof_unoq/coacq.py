#!/usr/bin/env python3
"""Independent camera + ToF acquisition on the UNO Q with software timestamp correlation.

Run ON THE UNO Q with the tof_unoq_vl53l8cx sketch flashed and the camera working:

  # per-frame mode: one command per camera frame (default: the repo's raw IMX219 script)
  python3 tools/tof_unoq/coacq.py --out ~/tof-runs/coacq-01 --camera-frames 10 --interval 1.0 \
      --camera-cmd 'tools/camera_characterize/capture_raw.sh {out}'

  # stream mode: one v4l2-ctl run; per-buffer V4L2 timestamps parsed from --verbose.
  # The media pipeline must already be configured (e.g. run capture_raw.sh once).
  python3 tools/tof_unoq/coacq.py --out ~/tof-runs/coacq-02 --camera-frames 30 \
      --camera-stream 'v4l2-ctl -d /dev/video0 --stream-mmap=4 --stream-count={n} \
                       --stream-to={out} --verbose'

The ToF and the camera run independently. Nothing triggers or synchronises
them in hardware. Each camera frame is paired with the ToF frame whose Linux
arrival time is nearest, and the time difference is recorded:

  * camera time: V4L2 buffer timestamp when v4l2-ctl reports it as ts-monotonic
    (same clock as time.monotonic_ns()); otherwise the midpoint of the host
    clock bracket around the capture command, with the bracket half-width as
    its uncertainty;
  * ToF time: Linux arrival time of the FRM line, which LAGS acquisition by the
    MCU read-out plus Bridge/router transport (see analyze.py clock_relation).

Output:  <out>/tof/...    tof_log.py session (raw.log, frames.csv, zones.csv, session.json)
         <out>/camera/    frames + camera.csv
         <out>/pairs.csv  camera frame -> nearest ToF frame, dt_ms, timing basis
         <out>/coacq.json manifest (commands, configs, counts, synchronisation statement)
"""

from __future__ import annotations

import argparse
import bisect
import contextlib
import csv
import json
import os
import shlex
import subprocess
import sys
import threading
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import tof_log  # noqa: E402
import v4l2ts  # noqa: E402

TOOL_VERSION = "coacq 0.1.0"
SYNC_STATEMENT = (
    "Independent acquisition. No hardware trigger or shared clock between the IMX219 and the "
    "VL53L8CX. "
    "Pairing is by nearest Linux CLOCK_MONOTONIC time; ToF times are arrival times, not "
    "acquisition times."
)


class TofReader(threading.Thread):
    """Feeds monitor lines into a TofSession on a background thread."""

    def __init__(self, session: tof_log.TofSession, client):
        super().__init__(daemon=True)
        self.session = session
        self.client = client
        self.stop = threading.Event()
        self.lock = threading.Lock()

    def run(self) -> None:
        try:
            for line, mono, real in self.client.lines():
                if self.stop.is_set():
                    return
                if line is not None and line.strip():
                    with self.lock:
                        rec = self.session.handle(line, mono, real)
                    if rec is not None and rec.type == "EVT":
                        print(f"[tof {rec.level}] {rec.code} {rec.kv}", file=sys.stderr)
        except OSError as exc:
            print(f"tof reader stopped: {exc}", file=sys.stderr)

    def frames(self) -> int:
        with self.lock:
            return self.session.frames


def _fmt(cmd: str, **kw) -> list[str]:
    return [part.format(**kw) for part in shlex.split(cmd)]


def capture_per_frame(args, cam_dir: Path) -> list[dict]:
    rows = []
    for k in range(args.camera_frames):
        out = cam_dir / f"frame_{k:04d}.{args.ext}"
        t0 = time.monotonic_ns()
        proc = subprocess.run(
            _fmt(args.camera_cmd, out=str(out)),
            capture_output=True,
            text=True,
            timeout=args.camera_timeout,
        )
        t1 = time.monotonic_ns()
        meta = None
        for ln in proc.stdout.splitlines()[::-1]:
            ln = ln.strip()
            if ln.startswith("{"):
                with contextlib.suppress(json.JSONDecodeError):
                    meta = json.loads(ln)
                break
        rows.append(
            {
                "frame": k,
                "file": out.name if out.exists() else "",
                "returncode": proc.returncode,
                "host_mono_start_ns": t0,
                "host_mono_end_ns": t1,
                "v4l2_seq": "",
                "v4l2_ts_mono_ns": "",
                "v4l2_clock": "not_reported_by_command",
                "camera_time_ns": (t0 + t1) // 2,
                "camera_time_basis": "host_bracket_midpoint",
                "camera_time_uncertainty_ms": (t1 - t0) / 2e6,
                "camera_meta_json": json.dumps(meta) if meta else "",
                "stderr_tail": proc.stderr.strip().splitlines()[-1] if proc.stderr.strip() else "",
            }
        )
        print(f"camera frame {k}: rc={proc.returncode} {(t1 - t0) / 1e6:.0f} ms", file=sys.stderr)
        if args.interval and k + 1 < args.camera_frames:
            time.sleep(args.interval)
    return rows


def capture_stream(args, cam_dir: Path) -> list[dict]:
    out = cam_dir / f"stream.{args.ext}"
    t0 = time.monotonic_ns()
    proc = subprocess.run(
        _fmt(args.camera_stream, out=str(out), n=str(args.camera_frames)),
        capture_output=True,
        text=True,
        timeout=args.camera_timeout,
    )
    t1 = time.monotonic_ns()
    (cam_dir / "v4l2-ctl.stderr.txt").write_text(proc.stderr)
    stamps = v4l2ts.parse(proc.stderr)
    rows = []
    for k, st in enumerate(stamps):
        mono = st.ts_mono_ns
        rows.append(
            {
                "frame": k,
                "file": out.name if out.exists() else "",
                "returncode": proc.returncode,
                "host_mono_start_ns": t0,
                "host_mono_end_ns": t1,
                "v4l2_seq": st.seq,
                "v4l2_ts_mono_ns": mono if mono is not None else "",
                "v4l2_clock": st.clock,
                "camera_time_ns": mono if mono is not None else "",
                "camera_time_basis": "v4l2_buffer_timestamp" if mono is not None else "unavailable",
                "camera_time_uncertainty_ms": "" if mono is not None else (t1 - t0) / 2e6,
                "camera_meta_json": json.dumps(
                    {"flags": st.flags, "bytesused": st.bytesused, "error": st.error}
                ),
                "stderr_tail": "",
            }
        )
    if not stamps:
        print("no per-buffer lines in v4l2-ctl output (was --verbose given?)", file=sys.stderr)
    return rows


def pair(camera_rows: list[dict], tof_frames_csv: Path) -> list[dict]:
    with open(tof_frames_csv, newline="") as f:
        tof = [(int(r["host_mono_ns"]), int(r["seq"])) for r in csv.DictReader(f)]
    times = [t for t, _ in tof]
    out = []
    for c in camera_rows:
        ct = c["camera_time_ns"]
        row = {
            "camera_frame": c["frame"],
            "camera_time_basis": c["camera_time_basis"],
            "camera_time_uncertainty_ms": c["camera_time_uncertainty_ms"],
        }
        if ct == "" or not tof:
            row.update({"tof_seq": "", "dt_ms": "", "tof_before_seq": "", "tof_after_seq": ""})
        else:
            i = bisect.bisect_left(times, ct)
            cands = [j for j in (i - 1, i) if 0 <= j < len(tof)]
            j = min(cands, key=lambda j: abs(times[j] - ct))
            row.update(
                {
                    "tof_seq": tof[j][1],
                    "dt_ms": (times[j] - ct) / 1e6,
                    "tof_before_seq": tof[i - 1][1] if i - 1 >= 0 else "",
                    "tof_after_seq": tof[i][1] if i < len(tof) else "",
                }
            )
        out.append(row)
    return out


def _write_csv(path: Path, rows: list[dict]) -> None:
    if not rows:
        path.write_text("")
        return
    with open(path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)


def run(args, client_factory=tof_log.MonitorClient) -> int:
    root = Path(os.path.expanduser(args.out))
    if root.exists() and any(root.iterdir()):
        print(f"REFUSED: {root} exists and is not empty", file=sys.stderr)
        return 4
    cam_dir = root / "camera"
    cam_dir.mkdir(parents=True, exist_ok=True)
    operator = {
        "target": args.target,
        "notes": args.notes,
        "operator": args.operator,
        "ref_distance_mm": args.ref_distance_mm,
    }
    session = tof_log.TofSession(root / "tof", args.label, operator, sys.argv)
    try:
        client = client_factory(args.host, args.port)
    except OSError as exc:
        session.close(f"connect failed: {exc}")
        print(
            f"could not connect to the ToF monitor at {args.host}:{args.port}: {exc}",
            file=sys.stderr,
        )
        return 3
    client.send("STATUS")
    reader = TofReader(session, client)
    reader.start()

    # Wait for ToF frames before starting the camera so every camera frame has neighbours.
    t_end = time.monotonic() + args.tof_wait
    while reader.frames() < 2 and time.monotonic() < t_end:
        with reader.lock:
            fatal = session.fatal_error()
        if fatal:
            break
        time.sleep(0.05)
    if reader.frames() < 2:
        reader.stop.set()
        client.close()
        reader.join(timeout=2)
        summary = session.close("no ToF frames before camera start")
        print(
            f"ToF not ranging; camera not started. diagnosis: {summary['diagnosis']}",
            file=sys.stderr,
        )
        return 2

    if args.camera_stream:
        rows = capture_stream(args, cam_dir)
        mode = "stream"
    else:
        rows = capture_per_frame(args, cam_dir)
        mode = "per_frame"
    time.sleep(args.tail)  # let ToF frames after the last camera frame arrive
    reader.stop.set()
    client.close()
    reader.join(timeout=2)
    with reader.lock:
        tof_summary = session.close("co-acquisition finished")

    _write_csv(cam_dir / "camera.csv", rows)
    pairs = pair(rows, root / "tof" / "frames.csv")
    _write_csv(root / "pairs.csv", pairs)
    dts = [abs(p["dt_ms"]) for p in pairs if p["dt_ms"] != ""]
    manifest = {
        "tool": TOOL_VERSION,
        "argv": sys.argv,
        "label": args.label,
        "operator": operator,
        "camera": {
            "mode": mode,
            "command": args.camera_stream or args.camera_cmd,
            "frames_requested": args.camera_frames,
            "frames_recorded": len(rows),
            "failed_commands": sum(1 for r in rows if r["returncode"] != 0),
            "time_bases": sorted({r["camera_time_basis"] for r in rows}),
        },
        "tof": {
            "frames": tof_summary["frames"],
            "cfg_read_back_from_sensor": tof_summary["cfg_read_back_from_sensor"],
            "hello": tof_summary["hello"],
            "sequence_gaps": tof_summary["sequence_gaps"],
            "crc_or_format_rejects": tof_summary["crc_or_format_rejects"],
            "errors": tof_summary["errors"],
        },
        "pairing": {
            "pairs": len(pairs),
            "abs_dt_ms_max": max(dts) if dts else None,
            "abs_dt_ms_median": sorted(dts)[len(dts) // 2] if dts else None,
        },
        "synchronisation": SYNC_STATEMENT,
        "calibration": "none applied; no extrinsic camera-ToF calibration exists yet",
    }
    (root / "coacq.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest["pairing"]), file=sys.stderr)
    return 0 if rows and tof_summary["frames"] else 2


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    p.add_argument("--out", required=True)
    g = p.add_mutually_exclusive_group()
    g.add_argument(
        "--camera-cmd",
        default="tools/camera_characterize/capture_raw.sh {out}",
        help="per-frame capture command; {out} is replaced by the frame path",
    )
    g.add_argument(
        "--camera-stream",
        help="one streaming command with {out} and {n}; must print v4l2-ctl --verbose",
    )
    p.add_argument("--camera-frames", type=int, default=10)
    p.add_argument("--interval", type=float, default=1.0, help="seconds between per-frame captures")
    p.add_argument("--camera-timeout", type=float, default=60.0)
    p.add_argument("--ext", default="raw", help="file extension for camera output")
    p.add_argument(
        "--tof-wait",
        type=float,
        default=60.0,
        help="seconds to wait for ToF frames before the camera",
    )
    p.add_argument("--tail", type=float, default=1.0)
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=7500)
    p.add_argument("--label", default="")
    p.add_argument("--ref-distance-mm", type=float)
    p.add_argument("--target", default="")
    p.add_argument("--operator", default="")
    p.add_argument("--notes", default="")
    return p


def main(argv=None) -> int:
    return run(build_parser().parse_args(argv))


if __name__ == "__main__":
    sys.exit(main())
