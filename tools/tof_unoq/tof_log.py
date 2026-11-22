#!/usr/bin/env python3
"""Record VL53L8CX frames from the UNO Q MCU sketch (firmware/tof_unoq_vl53l8cx).

Run ON THE UNO Q (Debian side), before or right after the sketch starts:

    python3 tools/tof_unoq/tof_log.py --out ~/tof-runs/20261016-500mm \
        --label 500mm --ref-distance-mm 500 --frames 100

arduino-router republishes the sketch's `Monitor` output on 127.0.0.1:7500. The
sketch waits for a client before it touches the bus, so starting this logger
records the whole bring-up (SCAN, ACK, ID, INIT, CFG) as well as the frames.

Output directory (refuses to overwrite):
    raw.log      every received line, verbatim, prefixed "<host_mono_ns> <host_real_ns> OK|BAD"
    events.csv   HELLO / EVT / CFG / HB records
    frames.csv   one row per FRM (sequence, MCU times, host arrival time, status counts)
    zones.csv    one row per zone per frame (raw distance, status, sigma; nothing corrected)
    session.json identity, configuration read back from the sensor, operator inputs,
                 counters (CRC rejects, sequence gaps), exit reason

Timestamps: MCU microseconds (64-bit-extended micros()) and the Linux host's
CLOCK_MONOTONIC / CLOCK_REALTIME at line arrival. Arrival time includes the
MCU -> Bridge UART -> router -> TCP latency; it is NOT the sensor acquisition time.

Exit status: 0 frames recorded; 2 no frames (see session.json "diagnosis");
3 could not connect; 4 output directory exists.
"""

from __future__ import annotations

import argparse
import contextlib
import csv
import json
import os
import platform
import socket
import sys
import time
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import tofproto  # noqa: E402

TOOL_VERSION = "tof_log 0.1.0"

# Bring-up failures the sketch reports, with the first thing to check.
# Mirrors docs/hardware/tof_unoq/BENCH_CHECKLIST.md "Troubleshooting".
DIAGNOSIS = {
    "NO_ACK": (
        "Nothing acknowledged at 0x29 on Wire1 (Qwiic/I2C4). Power off; check VIN/GND/SDA/SCL "
        "continuity, the SPI/I2C-to-GND jumper (the carrier defaults to SPI) and VIN at the "
        "carrier."
    ),
    "ID_FAIL": (
        "Something ACKs at 0x29 but page-0 registers 0/1 are not 0xF0/0x0C. Wrong device, "
        "SDA/SCL marginal (scope), or a second device at 0x29."
    ),
    "INIT_FAIL": (
        "Firmware upload or boot failed (ULD status in the event). Check AVDD during init, bus "
        "integrity (scope SDA/SCL) and cable length; power-cycle the sensor before retrying."
    ),
    "CFG_FAIL": "Sensor rejected the configuration (127 = invalid setting, e.g. >15 Hz at 8x8).",
    "CFG_READBACK_FAIL": "Configuration read-back failed; treat as a bus fault.",
    "RANGING_START_FAIL": "start_ranging failed after a good init; check AVDD under load.",
    "RANGING_STALLED": "No data-ready for several frame periods; check AVDD/VIN droop and resets.",
}
FATAL_CODES = set(DIAGNOSIS)

# Trigger and IMU failures never end a ToF session; they get their own diagnosis.
PERIPHERAL_DIAGNOSIS = {
    "TRIGGER_FAIL": (
        "Nothing answered as a Qwiic Button at 0x6F (or its ID is not 0x5D). Check the Qwiic "
        "daisy chain order and seating, and that the breakout's address jumpers are open."
    ),
    "TRIGGER_LOST": (
        "The button stopped answering after bring-up. Re-seat the Qwiic cables, scope SCL for "
        "clock stretching, try I2CHZ 100, then send PERIPH."
    ),
    "IMU_FAIL": (
        "BMI270 bring-up failed; the event's stage says where (chip_id: no 0x24 at 0x68; "
        "init_status: the 8 KB config was not accepted - power-cycle and retry)."
    ),
    "IMU_LOST": "The BMI270 stopped answering after bring-up. Re-seat the cables, then PERIPH.",
}


class OutputExists(RuntimeError):
    pass


class TofSession:
    """Turns received lines into the session files. Usable without a socket (tests, coacq)."""

    def __init__(self, outdir: Path, label: str = "", operator: dict | None = None, argv=None):
        self.outdir = Path(outdir)
        if self.outdir.exists() and any(self.outdir.iterdir()):
            raise OutputExists(f"{self.outdir} exists and is not empty")
        self.outdir.mkdir(parents=True, exist_ok=True)
        self.label = label
        self.operator = operator or {}
        self.argv = argv or []
        self.started_real_ns = time.time_ns()
        self.started_mono_ns = time.monotonic_ns()
        # The four files stay open for the whole session and are closed in close().
        self.raw = self._open("raw.log", "\n")
        self._ev_f = self._open("events.csv")
        self._fr_f = self._open("frames.csv")
        self._zn_f = self._open("zones.csv")
        self._imu_f = self._open("imu.csv")
        self.imu = csv.writer(self._imu_f)
        self.imu.writerow(
            [
                "host_mono_ns",
                "host_real_ns",
                "mcu_us",
                "seq",
                "sensortime",
                "ax_mg",
                "ay_mg",
                "az_mg",
                "gx_mdps",
                "gy_mdps",
                "gz_mdps",
                "temp_cc",
            ]
        )
        self.trigger_presses = 0
        self.peripheral_errors: list[dict] = []
        self.ev = csv.writer(self._ev_f)
        self.fr = csv.writer(self._fr_f)
        self.zn = csv.writer(self._zn_f)
        self.ev.writerow(
            ["host_mono_ns", "host_real_ns", "type", "mcu_us", "level", "code", "fields_json"]
        )
        self.fr.writerow(
            [
                "seq",
                "streamcount",
                "mcu_us_ready",
                "mcu_us_read_done",
                "host_mono_ns",
                "host_real_ns",
                "zones",
                "silicon_temp_c",
                "n_valid",
                "n_valid_50pct",
                "n_below_50pct",
                "n_no_target",
                "label",
            ]
        )
        self.zn.writerow(
            [
                "seq",
                "host_mono_ns",
                "zone",
                "row",
                "col",
                "distance_mm",
                "target_status",
                "status_class",
                "nb_target_detected",
                "range_sigma_mm",
            ]
        )
        self.hello: dict = {}
        self.cfg: dict = {}
        self.last_hb: dict = {}
        self.errors: list[dict] = []
        self.counts = Counter()
        self.frames = 0
        self.last_seq: int | None = None
        self.last_stream: int | None = None
        self.seq_gaps: list[tuple[int, int]] = []
        self.stream_gaps = 0
        self.first_frame_mono_ns: int | None = None
        self.last_frame_mono_ns: int | None = None
        self.state = "connecting"

    def _open(self, name: str, newline: str = ""):
        return open(self.outdir / name, "w", encoding="utf-8", newline=newline)  # noqa: SIM115

    def handle(self, line: str, mono_ns: int, real_ns: int) -> tofproto.Record | None:
        try:
            rec = tofproto.parse_line(line)
        except tofproto.ProtocolError as exc:
            self.counts["rejected"] += 1
            self.raw.write(f"{mono_ns} {real_ns} BAD {line.rstrip()}  # {exc}\n")
            return None
        self.raw.write(f"{mono_ns} {real_ns} OK {rec.raw}\n")
        self.counts[rec.type] += 1
        if rec.type == "FRM":
            self._frame(rec, mono_ns, real_ns)
        elif rec.type == "IMU":
            t = rec.temp_cc
            self.imu.writerow(
                [mono_ns, real_ns, rec.mcu_us, rec.seq, rec.sensortime, *rec.acc_mg]
                + [*rec.gyr_mdps, "" if t is None else t]
            )
        else:
            self.ev.writerow(
                [
                    mono_ns,
                    real_ns,
                    rec.type,
                    rec.mcu_us,
                    rec.level or "",
                    rec.code or "",
                    json.dumps(rec.kv, sort_keys=True),
                ]
            )
            if rec.type == "HELLO":
                self.hello = rec.kv
            elif rec.type == "CFG":
                self.cfg = rec.kv
            elif rec.type == "HB":
                self.last_hb = rec.kv
                self.state = rec.kv.get("state", self.state)
            elif rec.type == "EVT" and rec.code == "TRIGGER" and rec.kv.get("edge") == "press":
                self.trigger_presses += 1
            elif rec.type == "EVT" and rec.level == "E" and rec.code in PERIPHERAL_DIAGNOSIS:
                self.peripheral_errors.append(
                    {"code": rec.code, "fields": rec.kv, "host_mono_ns": mono_ns}
                )
            elif rec.type == "EVT" and rec.level == "E":
                self.errors.append({"code": rec.code, "fields": rec.kv, "host_mono_ns": mono_ns})
        return rec

    def _frame(self, rec: tofproto.Record, mono_ns: int, real_ns: int) -> None:
        if self.last_seq is not None and rec.seq != self.last_seq + 1:
            self.seq_gaps.append((self.last_seq, rec.seq))
        if self.last_stream is not None and (rec.streamcount - self.last_stream) % 256 != 1:
            self.stream_gaps += 1
        self.last_seq, self.last_stream = rec.seq, rec.streamcount
        classes = Counter(z.status_class for z in rec.zones)
        self.fr.writerow(
            [
                rec.seq,
                rec.streamcount,
                rec.us_ready,
                rec.us_read_done,
                mono_ns,
                real_ns,
                rec.zones_count,
                rec.temp_c,
                classes["valid"],
                classes["valid_50pct"],
                classes["below_50pct"],
                classes["no_target"],
                self.label,
            ]
        )
        for z in rec.zones:
            r, c = z.row_col(rec.zones_count)
            self.zn.writerow(
                [
                    rec.seq,
                    mono_ns,
                    z.index,
                    r,
                    c,
                    z.distance_mm,
                    z.target_status,
                    z.status_class,
                    z.nb_target_detected,
                    z.range_sigma_mm,
                ]
            )
        self.frames += 1
        if self.first_frame_mono_ns is None:
            self.first_frame_mono_ns = mono_ns
        self.last_frame_mono_ns = mono_ns

    def fatal_error(self) -> dict | None:
        for e in reversed(self.errors):
            if e["code"] in FATAL_CODES:
                return e
        return None

    def close(self, exit_reason: str) -> dict:
        for f in (self.raw, self._ev_f, self._fr_f, self._zn_f, self._imu_f):
            f.flush()
            f.close()
        fatal = self.fatal_error()
        span_s = None
        if self.frames > 1:
            span_s = (self.last_frame_mono_ns - self.first_frame_mono_ns) / 1e9
        summary = {
            "tool": TOOL_VERSION,
            "protocol": tofproto.PROTOCOL_VERSION,
            "argv": self.argv,
            "label": self.label,
            "operator": self.operator,
            "host": {
                "hostname": platform.node(),
                "platform": platform.platform(),
                "python": sys.version.split()[0],
            },
            "started_real_ns": self.started_real_ns,
            "started_mono_ns": self.started_mono_ns,
            "hello": self.hello,
            "cfg_read_back_from_sensor": self.cfg,
            "last_heartbeat": self.last_hb,
            "frames": self.frames,
            "frame_span_s": span_s,
            "host_arrival_rate_hz": (self.frames - 1) / span_s if span_s else None,
            "record_counts": dict(self.counts),
            "crc_or_format_rejects": self.counts["rejected"],
            "sequence_gaps": self.seq_gaps,
            "sensor_streamcount_gaps": self.stream_gaps,
            "errors": self.errors,
            "diagnosis": DIAGNOSIS.get(fatal["code"]) if fatal else None,
            "trigger_presses": self.trigger_presses,
            "imu_samples": self.counts["IMU"],
            "peripheral_errors": self.peripheral_errors,
            "peripheral_diagnosis": [
                PERIPHERAL_DIAGNOSIS[e["code"]] for e in self.peripheral_errors
            ],
            "exit_reason": exit_reason,
            "evidence_class": "raw sensor output; no calibration or correction applied",
            "timestamp_note": (
                "host_*_ns is arrival time on Linux (includes Bridge/router latency); mcu_us is "
                "the MCU's micros() extended to 64 bits. No hardware synchronization."
            ),
        }
        (self.outdir / "session.json").write_text(json.dumps(summary, indent=2) + "\n")
        return summary


class MonitorClient:
    """Line reader for the arduino-router monitor TCP port."""

    def __init__(self, host: str, port: int, timeout_s: float = 5.0):
        self.sock = socket.create_connection((host, port), timeout=timeout_s)
        self.sock.settimeout(0.2)
        self._buf = b""

    def send(self, command: str) -> None:
        self.sock.sendall(command.encode("ascii") + b"\n")

    def lines(self):
        """Yield (line, mono_ns, real_ns).

        Yields (None, ...) when idle so callers can check their stop conditions.
        """
        while True:
            try:
                chunk = self.sock.recv(4096)
            except TimeoutError:
                yield None, time.monotonic_ns(), time.time_ns()
                continue
            mono, real = time.monotonic_ns(), time.time_ns()
            if not chunk:
                return
            self._buf += chunk
            while b"\n" in self._buf:
                line, self._buf = self._buf.split(b"\n", 1)
                yield line.decode("ascii", errors="replace"), mono, real

    def close(self) -> None:
        self.sock.close()


def imu_console_line(rec: tofproto.Record) -> str:
    """One IMU sample for the operator: sensor axes, magnitude, no calibration."""
    ax, ay, az = rec.acc_mg
    mag = (ax * ax + ay * ay + az * az) ** 0.5
    gx, gy, gz = rec.gyr_mdps
    return (
        f"[IMU] #{rec.seq} acc mg ({ax}, {ay}, {az}) |a|={mag:.0f} mg  "
        f"gyr dps ({gx / 1000:.2f}, {gy / 1000:.2f}, {gz / 1000:.2f})"
    )


def run(args, session_factory=TofSession, client_factory=MonitorClient) -> int:
    operator = {
        "ref_distance_mm": args.ref_distance_mm,
        "target": args.target,
        "notes": args.notes,
        "operator": args.operator,
    }
    try:
        session = session_factory(Path(args.out), args.label, operator, sys.argv)
    except OutputExists as exc:
        print(f"REFUSED: {exc}", file=sys.stderr)
        return 4
    try:
        client = client_factory(args.host, args.port)
    except OSError as exc:
        session.close(f"connect failed: {exc}")
        print(
            f"could not connect to {args.host}:{args.port}: {exc}\n"
            "Is arduino-router running (systemctl status arduino-router) and the sketch flashed?",
            file=sys.stderr,
        )
        return 3

    client.send("STATUS")  # the sketch may already be running from an earlier connection
    for cmd in args.send or []:
        client.send(cmd)
    reconfig = []
    if args.res:
        reconfig.append(f"RES {args.res}")
    if args.hz:
        reconfig.append(f"HZ {args.hz}")
    reconfig_sent = not reconfig
    deadline = time.monotonic() + args.duration if args.duration else None
    no_frame_deadline = time.monotonic() + args.init_timeout
    reason = "stream ended"
    try:
        for line, mono, real in client.lines():
            if line is not None and line.strip():
                rec = session.handle(line, mono, real)
                if rec and rec.type == "EVT":
                    print(f"[{rec.level}] {rec.code} {rec.kv}", file=sys.stderr)
                elif rec and rec.type == "IMU" and rec.seq % 10 == 0:
                    print(imu_console_line(rec), file=sys.stderr)
                if not reconfig_sent and rec and rec.type in ("FRM", "CFG"):
                    for cmd in ["STOP", *reconfig, "START"]:
                        client.send(cmd)
                    reconfig_sent = True
            if session.fatal_error() and session.frames == 0:
                reason = f"sketch reported {session.fatal_error()['code']}"
                break
            if args.frames and session.frames >= args.frames:
                reason = f"{args.frames} frames recorded"
                break
            now = time.monotonic()
            if deadline and now >= deadline:
                reason = f"duration {args.duration}s reached"
                break
            if session.frames == 0 and now >= no_frame_deadline:
                reason = f"no frame within {args.init_timeout}s"
                break
    except KeyboardInterrupt:
        reason = "interrupted by operator"
    finally:
        if args.stop_on_exit:
            with contextlib.suppress(OSError):
                client.send("STOP")
        client.close()
    summary = session.close(reason)
    print(
        f"{summary['frames']} frames, {summary['crc_or_format_rejects']} rejected lines, "
        f"{len(summary['sequence_gaps'])} sequence gaps -> {args.out} ({reason})",
        file=sys.stderr,
    )
    if summary["diagnosis"]:
        print(f"DIAGNOSIS: {summary['diagnosis']}", file=sys.stderr)
    return 0 if summary["frames"] else 2


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    p.add_argument("--out", required=True, help="new session directory")
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=7500, help="arduino-router monitor port")
    p.add_argument("--frames", type=int, default=0, help="stop after N frames (0 = no limit)")
    p.add_argument(
        "--duration", type=float, default=0.0, help="stop after S seconds (0 = no limit)"
    )
    p.add_argument(
        "--init-timeout", type=float, default=60.0, help="give up if no frame within S seconds"
    )
    p.add_argument("--res", type=int, choices=(4, 8), help="re-configure resolution after start")
    p.add_argument("--hz", type=int, help="re-configure ranging frequency after start")
    p.add_argument("--stop-on-exit", action="store_true", help="send STOP when the logger exits")
    p.add_argument(
        "--send",
        action="append",
        metavar="CMD",
        help="extra sketch command after STATUS, repeatable (e.g. PERIPH, 'I2CHZ 100')",
    )
    p.add_argument(
        "--label", default="", help="free text stored with every frame, e.g. 500mm-white-card"
    )
    p.add_argument(
        "--ref-distance-mm", type=float, help="operator-measured reference distance (tape/ruler)"
    )
    p.add_argument("--target", default="", help="target description (material, size, angle)")
    p.add_argument("--operator", default="", help="who ran the capture")
    p.add_argument("--notes", default="")
    return p


def main(argv=None) -> int:
    args = build_parser().parse_args(argv)
    args.out = os.path.expanduser(args.out)
    return run(args)


if __name__ == "__main__":
    sys.exit(main())
