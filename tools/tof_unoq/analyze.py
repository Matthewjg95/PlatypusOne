#!/usr/bin/env python3
"""Summarise a tof_log.py session for the Gate D/E records.

    python3 tools/tof_unoq/analyze.py ~/tof-runs/20261016-500mm

Writes analysis.json and analysis.md into the session directory. Reports raw
sensor readings only. When the operator recorded a reference distance, the
central-zone median is shown next to it as a difference. No offset, scale or
crosstalk correction is derived or applied, and nothing is labelled pass/fail:
the operator closes gates.
"""

from __future__ import annotations

import argparse
import csv
import json
import statistics
import sys
from collections import defaultdict
from pathlib import Path

ANALYSIS_VERSION = "tof_unoq.analyze 0.1.0"

# Central 2x2 zones in the ULD's raw index order.
CENTRAL = {64: (27, 28, 35, 36), 16: (5, 6, 9, 10)}


def _pct(values: list[float], q: float) -> float | None:
    if not values:
        return None
    s = sorted(values)
    k = (len(s) - 1) * q
    lo, hi = int(k), min(int(k) + 1, len(s) - 1)
    return s[lo] + (s[hi] - s[lo]) * (k - lo)


def _stats(values: list[float]) -> dict:
    if not values:
        return {"n": 0}
    out = {
        "n": len(values),
        "median": statistics.median(values),
        "min": min(values),
        "max": max(values),
    }
    if len(values) > 1:
        out["stdev"] = statistics.stdev(values)
        out["p05"] = _pct(values, 0.05)
        out["p95"] = _pct(values, 0.95)
    return out


def load(session: Path) -> tuple[dict, list[dict], list[dict]]:
    meta = json.loads((session / "session.json").read_text())
    with open(session / "frames.csv", newline="") as f:
        frames = list(csv.DictReader(f))
    with open(session / "zones.csv", newline="") as f:
        zones = list(csv.DictReader(f))
    return meta, frames, zones


def analyze(session: Path) -> dict:
    meta, frames, zones = load(session)
    out: dict = {
        "analysis": ANALYSIS_VERSION,
        "session": str(session),
        "label": meta.get("label"),
        "operator": meta.get("operator"),
        "cfg_read_back_from_sensor": meta.get("cfg_read_back_from_sensor"),
        "frames": len(frames),
        "crc_or_format_rejects": meta.get("crc_or_format_rejects"),
        "sequence_gaps": meta.get("sequence_gaps"),
        "sensor_streamcount_gaps": meta.get("sensor_streamcount_gaps"),
        "errors": meta.get("errors"),
    }
    if not frames:
        out["note"] = "no frames: see session.json diagnosis"
        return out

    ready = [int(f["mcu_us_ready"]) for f in frames]
    done = [int(f["mcu_us_read_done"]) for f in frames]
    host = [int(f["host_mono_ns"]) for f in frames]
    temps = [float(f["silicon_temp_c"]) for f in frames]
    mcu_dt_ms = [(b - a) / 1000 for a, b in zip(ready, ready[1:], strict=False)]
    host_dt_ms = [(b - a) / 1e6 for a, b in zip(host, host[1:], strict=False)]
    read_ms = [(d - r) / 1000 for r, d in zip(ready, done, strict=True)]
    out["timing"] = {
        "mcu_frame_interval_ms": _stats(mcu_dt_ms),
        "host_arrival_interval_ms": _stats(host_dt_ms),
        "mcu_i2c_read_ms": _stats(read_ms),
        "silicon_temp_c": _stats(temps),
    }
    # Host arrival minus MCU read-done time. The lower envelope bounds the
    # transport latency plus the (unknown) clock offset; the spread above it is
    # Bridge/router/scheduling jitter. Estimate only: there is no common clock.
    offs = [h / 1000 - d for h, d in zip(host, done, strict=True)]
    base = min(offs)
    out["clock_relation"] = {
        "method": (
            "host_mono_us - mcu_us_read_done; lower envelope = best-case latency + clock offset"
        ),
        "lower_envelope_us": base,
        "excess_over_envelope_us": _stats([o - base for o in offs]),
        "note": "software correlation only; not a measured hardware synchronisation",
    }
    if len(frames) >= 20:
        n = len(frames) // 4
        a = min(offs[:n])
        b = min(offs[-n:])
        span_s = (done[-1] - done[0]) / 1e6
        out["clock_relation"]["drift_estimate_ppm"] = (b - a) / span_s if span_s > 0 else None

    nz = int(frames[0]["zones"])
    by_zone: dict[int, list[dict]] = defaultdict(list)
    for z in zones:
        by_zone[int(z["zone"])].append(z)
    side = 8 if nz == 64 else 4
    zone_out = {}
    for zi in range(nz):
        rows = by_zone.get(zi, [])
        valid = [float(r["distance_mm"]) for r in rows if r["status_class"] == "valid"]
        zone_out[zi] = {
            "row": zi // side,
            "col": zi % side,
            "frames": len(rows),
            "valid_frames": len(valid),
            "status_counts": _count(r["target_status"] for r in rows),
            "distance_valid_mm": _stats(valid),
            "sigma_mm_median": statistics.median([float(r["range_sigma_mm"]) for r in rows])
            if rows
            else None,
        }
    out["zones"] = zone_out

    cz = CENTRAL[nz]
    per_frame_central = []
    for f in frames:
        seq = f["seq"]
        vals = [
            float(z["distance_mm"])
            for z in zones
            if z["seq"] == seq and int(z["zone"]) in cz and z["status_class"] == "valid"
        ]
        if len(vals) == len(cz):
            per_frame_central.append(statistics.mean(vals))
    central_all = [
        float(z["distance_mm"])
        for z in zones
        if int(z["zone"]) in cz and z["status_class"] == "valid"
    ]
    central = {
        "zones": list(cz),
        "frames_with_all_central_valid": len(per_frame_central),
        "distance_valid_mm": _stats(central_all),
        "per_frame_central_mean_mm": _stats(per_frame_central),
    }
    ref = (meta.get("operator") or {}).get("ref_distance_mm")
    if ref is not None and central_all:
        central["operator_reference_mm"] = ref
        central["median_minus_reference_mm"] = statistics.median(central_all) - float(ref)
        central["note"] = (
            "raw reading minus operator-measured reference; the reference's own uncertainty "
            "and the "
            "sensor's optical reference plane are not accounted for; no correction is derived"
        )
    out["central"] = central
    out["datasheet_context"] = (
        "DS14161 Rev 7 Table 18 (continuous mode; target filling 100 % of the FoV, AVDD 3.3 V, "
        "no cover "
        "glass, default calibration, 2.7 sigma): 200-4000 mm +/-3-7 % at 4x4/30 Hz and +/-5-8 % at "
        "8x8/15 Hz; 20-200 mm +/-8-14 mm. Corner zones may degrade by up to 4 % more; add 1-2 "
        "% for "
        "assembly tilt. A bench setup that differs from these conditions is not comparable "
        "one-to-one."
    )
    return out


def _count(values) -> dict:
    d: dict[str, int] = defaultdict(int)
    for v in values:
        d[str(v)] += 1
    return dict(sorted(d.items(), key=lambda kv: int(kv[0])))


def to_markdown(a: dict) -> str:
    L = [f"# ToF session analysis: {a.get('label') or a['session']}", ""]
    L.append(
        f"Analysis `{a['analysis']}`. Raw sensor readings; nothing corrected. Gates are closed "
        "by the operator."
    )
    L.append("")
    L.append(
        f"- frames: {a['frames']}; CRC/format rejects: {a.get('crc_or_format_rejects')}; "
        f"sequence gaps: {len(a.get('sequence_gaps') or [])}; "
        f"sensor streamcount gaps: {a.get('sensor_streamcount_gaps')}"
    )
    L.append(
        "- configuration read back from the sensor: "
        f"`{json.dumps(a.get('cfg_read_back_from_sensor'))}`"
    )
    for e in a.get("errors") or []:
        L.append(f"- error event: {e['code']} {e['fields']}")
    if not a["frames"]:
        L.append("")
        L.append("No frames. See session.json `diagnosis`.")
        return "\n".join(L) + "\n"
    t = a["timing"]

    def s(d, key="median"):
        v = d.get(key)
        return "-" if v is None else f"{v:.2f}"

    L.append(
        f"- MCU frame interval median {s(t['mcu_frame_interval_ms'])} ms; host arrival "
        "interval median "
        f"{s(t['host_arrival_interval_ms'])} ms (p95 {s(t['host_arrival_interval_ms'], 'p95')}); "
        f"I2C read per frame median {s(t['mcu_i2c_read_ms'])} ms; silicon temp median "
        f"{s(t['silicon_temp_c'])} °C"
    )
    c = a["clock_relation"]
    L.append(
        f"- host-arrival minus MCU time, excess over lower envelope: median "
        f"{s(c['excess_over_envelope_us'])} µs, p95 {s(c['excess_over_envelope_us'], 'p95')} µs "
        f"(software correlation estimate)"
    )
    ce = a["central"]
    L.append(
        f"- central zones {ce['zones']}: valid median {s(ce['distance_valid_mm'])} mm, "
        f"stdev {s(ce['distance_valid_mm'], 'stdev')} mm over "
        f"{ce['distance_valid_mm'].get('n', 0)} readings"
    )
    if "operator_reference_mm" in ce:
        L.append(
            f"- operator reference {ce['operator_reference_mm']} mm; median minus reference "
            f"{ce['median_minus_reference_mm']:+.1f} mm ({ce['note']})"
        )
    L.append("")
    nz = len(a["zones"])
    side = 8 if nz == 64 else 4
    L.append(
        "Median valid distance (mm) per zone, raw ULD index order (row 0 = zones 0..; not flipped):"
    )
    L.append("")
    L.append("```")
    for r in range(side):
        cells = []
        for col in range(side):
            z = a["zones"][r * side + col]
            m = z["distance_valid_mm"].get("median")
            cells.append(f"{m:6.0f}" if m is not None else "     -")
        L.append(" ".join(cells))
    L.append("```")
    L.append("")
    L.append("Fraction of frames with status 5 (valid) per zone:")
    L.append("")
    L.append("```")
    for r in range(side):
        cells = []
        for col in range(side):
            z = a["zones"][r * side + col]
            frac = z["valid_frames"] / z["frames"] if z["frames"] else 0
            cells.append(f"{frac:5.2f}")
        L.append(" ".join(cells))
    L.append("```")
    L.append("")
    L.append(a["datasheet_context"])
    return "\n".join(L) + "\n"


def main(argv=None) -> int:
    p = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    p.add_argument("session", type=Path)
    args = p.parse_args(argv)
    a = analyze(args.session)
    (args.session / "analysis.json").write_text(json.dumps(a, indent=2) + "\n")
    md = to_markdown(a)
    (args.session / "analysis.md").write_text(md)
    print(md)
    return 0


if __name__ == "__main__":
    sys.exit(main())
