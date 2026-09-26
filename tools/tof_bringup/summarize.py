#!/usr/bin/env python3
"""Summarize one boot's complete native zone rows; preserve raw input unchanged."""
import argparse
import csv
import json
import statistics
from collections import Counter, defaultdict
from pathlib import Path


def percentile(values, fraction):
    values = sorted(values)
    pos = (len(values) - 1) * fraction
    lo = int(pos)
    hi = min(lo + 1, len(values) - 1)
    return values[lo] + (values[hi] - values[lo]) * (pos - lo)


def summarize(text, reference_mm):
    zones = defaultdict(list)
    frame_times = {}
    pairs = set()
    events = Counter()
    for line in text.splitlines():
        row = next(csv.reader([line]))
        if not row:
            continue
        if row[0] in ('HALT', 'TIMEOUT', 'tof_bringup_v1'):
            events[row[0]] += 1
        if row[0] != 'zone' or row[1:2] == ['frame']:
            continue
        if len(row) != 13:
            raise ValueError('Incomplete zone row; preserve log and recapture')
        _, frame, ms, latency, zone, targets, status, mm, sigma, signal, ambient, valid, errors = row
        frame, ms, zone = int(frame), int(ms), int(zone)
        key = (frame, zone)
        if key in pairs:
            raise ValueError('Duplicate frame/zone; possible reboot or concatenated runs')
        pairs.add(key)
        if frame in frame_times and frame_times[frame] != ms:
            raise ValueError('Inconsistent frame timestamps')
        frame_times[frame] = ms
        zones[zone].append((int(mm), int(targets) > 0 and int(status) == 5, int(status), int(errors)))
    if not frame_times:
        raise ValueError('No range frames')
    if events['tof_bringup_v1'] > 1:
        raise ValueError('Multiple boots; split into separate runs')
    native = set(zones)
    if native not in (set(range(16)), set(range(64))):
        raise ValueError('Missing or unexpected native zone indices')
    if any(sum(f == frame for f, _ in pairs) != len(native) for frame in frame_times):
        raise ValueError('Incomplete frame; log truncation is not a valid-zone failure')
    timestamps = [frame_times[f] for f in sorted(frame_times)]
    if any(b <= a for a, b in zip(timestamps, timestamps[1:])):
        raise ValueError('Nonmonotonic timestamps; split reboots or uptime wrap')
    summaries = {}
    for zone, samples in sorted(zones.items()):
        valid = [mm for mm, good, _, _ in samples if good]
        median = statistics.median(valid) if valid else None
        summaries[zone] = {
            'samples': len(samples), 'strict_valid_pct': 100 * len(valid) / len(samples),
            'median_mm': median,
            'bias_mm': median - reference_mm if median is not None else None,
            'p95_minus_p5_mm': percentile(valid, .95) - percentile(valid, .05) if valid else None,
            'status_counts': dict(Counter(s for _, _, s, _ in samples)),
        }
    span = timestamps[-1] - timestamps[0]
    return {'method': 'tof_summary_v1_linear_percentile', 'reference_mm': reference_mm,
            'frames': len(frame_times), 'observed_span_ms': span,
            'achieved_hz': (len(frame_times) - 1) * 1000 / span if span else None,
            'max_io_errors': max(e for rows in zones.values() for _, _, _, e in rows),
            'events': dict(events), 'zones': summaries,
            'decision': 'human_review_required; off-axis bias is not axial accuracy'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--reference-mm', required=True, type=float)
    args = parser.parse_args()
    if args.reference_mm <= 0:
        parser.error('reference must be positive')
    print(json.dumps(summarize(args.log.read_text(), args.reference_mm), indent=2))

if __name__ == '__main__':
    main()
