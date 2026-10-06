"""Capture matrix: expand matrix_spec.json into planned shots; start datasets.

Standard library only (runs on the UNO Q).
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from . import UNKNOWN
from .contract import CAMERA_FIELDS, DATASET_SCHEMA, PLATFORM_FIELDS

# Target definitions matching targets/ (printed sheets). Dimensions start
# nominal_unverified; Matthew sets "verified" + source after measuring the print.
DEFAULT_TARGETS: dict[str, dict[str, Any]] = {
    "charuco-7x10-20": {
        "kind": "charuco",
        "squares": [7, 10],
        "square_mm": 20.0,
        "square_mm_status": "nominal_unverified",
        "marker_to_square": 0.7,
        "dictionary": "DICT_4X4_50",
        "source": "tools/camera_characterize/targets/charuco_7x10_20mm.svg - "
        "measure printed square pitch with calipers over 6 squares",
    },
    "scout-ref-20": {
        "kind": "scout_reference",
        "reference_mm": 20.0,
        "reference_mm_status": "nominal_unverified",
        "source": "docs/hardware/calibration_sheet.pdf page 1 - "
        "verify the printed square with calipers",
    },
    "part-1": {
        "kind": "planar_part",
        "description": UNKNOWN,
        "reference_target": "scout-ref-20",
        "truth": {"length_mm": UNKNOWN, "width_mm": UNKNOWN, "status": UNKNOWN, "source": UNKNOWN},
    },
}

CAMERA_DEFAULTS = {
    "B0394": {
        "sensor": "IMX219",
        "lens": "M12 manual focus, low distortion (vendor)",
        "focus_type": "manual",
    },
    "B0390": {"sensor": "IMX219", "lens": "stock fixed focus (vendor)", "focus_type": "fixed"},
    "B0393": {"sensor": "IMX219", "lens": "motorized focus (vendor)", "focus_type": "motorized"},
}


def expand(spec: dict[str, Any], camera: str) -> list[dict[str, Any]]:
    shots: list[dict[str, Any]] = []

    def add(cell: dict[str, Any], distance: Any, prefix: str) -> None:
        for i in range(int(cell["count"])):
            s = {
                "plan_cell": f"{prefix}{cell['cell']}",
                "plan_index": i + 1,
                "working_distance_mm": distance,
                "scene": cell["scene"],
                "placement": cell["placement"],
                "lighting": cell["lighting"],
                "targets": list(cell["targets"]),
                "instruction": cell["instruction"],
            }
            for k in ("calibration_set", "mount_id", "repeat_series"):
                if k in cell:
                    s[k] = cell[k]
            if "repeat_series" in cell:
                s["repeat_index"] = i + 1
            shots.append(s)

    for d in spec["distances_mm"]:
        for cell in spec["per_distance"]:
            add(cell, d, f"d{d}-")
    rep = spec["representative_distance_mm"]
    for cell in spec["at_representative"]:
        add(cell, rep, f"d{rep}-")
    for cell in spec.get("camera_specific", {}).get(camera, {}).get("extra", []):
        add(cell, rep, f"d{rep}-")
    return shots


def plan_markdown(spec: dict[str, Any]) -> str:
    L = [f"# Camera capture matrix ({spec['matrix_version']})", "", spec["purpose"], ""]
    for cam in spec["cameras"]:
        shots = expand(spec, cam)
        L += [f"## {cam} — {len(shots)} frames", ""]
        before = spec.get("camera_specific", {}).get(cam, {}).get("before")
        if before:
            L += [f"Before the first frame: {before}", ""]
        L += [
            "| Cell | Distance (mm) | Scene | Placement | Lighting | Count |",
            "|---|---|---|---|---|---|",
        ]
        seen: dict[str, dict[str, Any]] = {}
        counts: dict[str, int] = {}
        for s in shots:
            seen.setdefault(s["plan_cell"], s)
            counts[s["plan_cell"]] = counts.get(s["plan_cell"], 0) + 1
        for cell, s in seen.items():
            L.append(
                f"| {cell} | {s['working_distance_mm']} | {s['scene']} | "
                f"{s['placement']} | {s['lighting']} | {counts[cell]} |"
            )
        L.append("")
    return "\n".join(L) + "\n"


def init_dataset(spec: dict[str, Any], camera: str, out: Path, dataset_id: str) -> None:
    """Create dataset.json (no frames yet) + capture_plan.json. Refuses to overwrite."""
    out.mkdir(parents=True, exist_ok=True)
    if (out / "dataset.json").exists():
        raise FileExistsError(f"{out / 'dataset.json'} exists; datasets are never re-initialised")
    camera_block = {k: UNKNOWN for k in CAMERA_FIELDS}
    camera_block.update({"sku": camera, **CAMERA_DEFAULTS.get(camera, {})})
    manifest = {
        "schema": DATASET_SCHEMA,
        "dataset_id": dataset_id,
        "evidence_type": "physical_capture",
        "camera": camera_block,
        "platform": {k: UNKNOWN for k in PLATFORM_FIELDS},
        "bringup": {
            "enumerates": UNKNOWN,
            "reboot_repeat": UNKNOWN,
            "dsi_coexistence": UNKNOWN,
            "focus_control": UNKNOWN,
            "evidence": UNKNOWN,
        },
        "physical": {
            "board_w_mm": {"value": UNKNOWN, "status": UNKNOWN, "source": UNKNOWN},
            "board_h_mm": {"value": UNKNOWN, "status": UNKNOWN, "source": UNKNOWN},
            "lens_stack_mm": {"value": UNKNOWN, "status": UNKNOWN, "source": UNKNOWN},
            "mounting_holes": {"value": UNKNOWN, "status": UNKNOWN, "source": UNKNOWN},
            "optical_center_offset_mm": {"value": UNKNOWN, "status": UNKNOWN, "source": UNKNOWN},
            "connector_side": {"value": UNKNOWN, "status": UNKNOWN, "source": UNKNOWN},
            "cable": {"value": UNKNOWN, "status": UNKNOWN, "source": UNKNOWN},
        },
        "targets": {k: DEFAULT_TARGETS[k] for k in spec["targets"]},
        "defaults": {
            "capture_command": UNKNOWN,
            "pixel_format": UNKNOWN,
            "frame_rate": UNKNOWN,
            "exposure_us": UNKNOWN,
            "analogue_gain": UNKNOWN,
            "white_balance": UNKNOWN,
            "focus_mode": UNKNOWN,
            "focus_position": UNKNOWN,
        },
        "frames": [],
        "notes": "",
    }
    (out / "dataset.json").write_text(json.dumps(manifest, indent=2) + "\n")
    shots = expand(spec, camera)
    for s in shots:
        # A mounting is a physical event of THIS dataset: namespace it so two
        # sessions' "mount-1" can never be mistaken for the same mounting.
        if "mount_id" in s:
            s["mount_id"] = f"{dataset_id}/{s['mount_id']}"
    plan = {"matrix_version": spec["matrix_version"], "camera": camera, "shots": shots}
    (out / "capture_plan.json").write_text(json.dumps(plan, indent=2) + "\n")
    (out / "frames").mkdir(exist_ok=True)


def coverage(dataset_root: Path, manifest: dict[str, Any]) -> dict[str, Any] | None:
    """Planned vs captured cells (ok frames only)."""
    p = dataset_root / "capture_plan.json"
    if not p.is_file():
        return None
    plan = json.loads(p.read_text())
    want: dict[str, int] = {}
    for s in plan["shots"]:
        want[s["plan_cell"]] = want.get(s["plan_cell"], 0) + 1
    got: dict[str, int] = {}
    for f in manifest.get("frames", []):
        if f.get("outcome", "ok") == "ok" and f.get("plan_cell"):
            got[f["plan_cell"]] = got.get(f["plan_cell"], 0) + 1
    missing = {c: n - got.get(c, 0) for c, n in want.items() if got.get(c, 0) < n}
    return {
        "planned_frames": sum(want.values()),
        "captured_ok_frames": sum(min(got.get(c, 0), n) for c, n in want.items()),
        "cells_complete": sum(1 for c, n in want.items() if got.get(c, 0) >= n),
        "cells_total": len(want),
        "missing": missing,
    }
