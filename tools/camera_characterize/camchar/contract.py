"""Camera evidence contract: experiment and dataset manifests, and their validation.

Standard library only: this module also runs on the UNO Q during capture.

Layout (see docs/hardware/CAMERA_EVIDENCE_CONTRACT.md):

    <experiment>/
      experiment.json               which datasets are compared, criteria file
      datasets/<dataset_id>/
        dataset.json                camera, platform, targets, frames
        frames/...                  immutable source images (never rewritten)
        photos/...                  setup / silkscreen photos (optional)
      derived/<ANALYSIS_VERSION>/   everything computed; safe to delete

A field that is not known is the literal string "UNKNOWN" - never a guess and
never silently omitted. Required fields must be present (possibly UNKNOWN);
an UNKNOWN that a later analysis step needs is reported by that step.
"""

from __future__ import annotations

import hashlib
import json
import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from . import UNKNOWN

DATASET_SCHEMA = "platypus.camera_dataset/1"
EXPERIMENT_SCHEMA = "platypus.camera_experiment/1"

EVIDENCE_TYPES = ("physical_capture", "synthetic_fixture")

CAMERA_FIELDS = ("sku", "sensor", "silkscreen", "revision", "unit_id", "lens", "focus_type")
PLATFORM_FIELDS = (
    "host",
    "repo_sha",
    "kernel",
    "image",
    "csi_port",
    "enumeration",
    "capture_stack",
)
# Per-frame capture state. Each may be given once in "defaults" and overridden
# per frame; the resolved frame must carry all of them (UNKNOWN allowed).
CAPTURE_FIELDS = (
    "capture_command",
    "pixel_format",
    "frame_rate",
    "exposure_us",
    "analogue_gain",
    "white_balance",
    "focus_mode",
    "focus_position",
    "working_distance_mm",
    "scene",
    "placement",
    "lighting",
    "targets",
    "timestamp_utc",
)
IMAGE_FIELDS = ("frame_id", "path", "sha256", "format", "width", "height")

ENUMS: dict[str, tuple[str, ...]] = {
    "focus_type": ("manual", "fixed", "motorized", UNKNOWN),
    "scene": (
        "calibration",
        "planar_part",
        "dimensional_reference",
        "repeatability",
        "lighting",
        "clutter",
        "geometry",
        "focus",
        "other",
    ),
    "placement": ("center", "edge", "corner", "rotated", "varied", UNKNOWN),
    "lighting": ("even", "hard_shadow", "low", "bright_overhead", "room", UNKNOWN),
    # not_exposed: the platform offers no focus control (e.g. no lens subdev).
    "focus_mode": ("manual_locked", "fixed", "motorized_position", "auto", "not_exposed", UNKNOWN),
    "format": ("png", "pgm", "ppm", "raw10p", "raw16", "abgr8888", "yuyv"),
    "outcome": ("ok", "capture_failed", "discarded"),
}

RAW_FORMATS = ("raw10p", "raw16", "abgr8888", "yuyv")
BAYER_PATTERNS = ("RGGB", "BGGR", "GRBG", "GBRG")

TARGET_KINDS = ("charuco", "scout_reference", "planar_part")
ARUCO_DICTIONARIES = ("DICT_4X4_50", "DICT_4X4_100", "DICT_5X5_100")
# A physical dimension is "verified" only after someone measured the printed or
# machined object (record how in "source"). Anything else is nominal.
DIMENSION_STATUS = ("verified", "nominal_unverified", UNKNOWN)


@dataclass
class Issue:
    level: str  # ERROR blocks use of the item; WARN is recorded and carried
    where: str
    message: str

    def as_dict(self) -> dict[str, str]:
        return {"level": self.level, "where": self.where, "message": self.message}


@dataclass
class Frame:
    """One resolved frame: defaults merged, path made absolute."""

    dataset_id: str
    frame_id: str
    data: dict[str, Any]
    abs_path: Path | None
    usable: bool = True  # False when an ERROR applies to this frame
    issues: list[Issue] = field(default_factory=list)

    def get(self, key: str, default: Any = UNKNOWN) -> Any:
        return self.data.get(key, default)

    @property
    def ref(self) -> str:
        return f"{self.dataset_id}/{self.frame_id}"


@dataclass
class Dataset:
    root: Path
    manifest: dict[str, Any]
    frames: list[Frame]
    issues: list[Issue]

    @property
    def dataset_id(self) -> str:
        return str(self.manifest.get("dataset_id", self.root.name))

    @property
    def camera(self) -> dict[str, Any]:
        return self.manifest.get("camera", {})

    @property
    def sku(self) -> str:
        return str(self.camera.get("sku", UNKNOWN))

    @property
    def evidence_type(self) -> str:
        return str(self.manifest.get("evidence_type", UNKNOWN))

    @property
    def targets(self) -> dict[str, dict[str, Any]]:
        return self.manifest.get("targets", {})

    @property
    def ok(self) -> bool:
        return not any(i.level == "ERROR" for i in self.issues)


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:  # read-only, always
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def is_number(value: Any) -> bool:
    """A real engineering number. bool is an int subclass in Python and is rejected."""
    return isinstance(value, int | float) and not isinstance(value, bool)


def is_unknown(value: Any) -> bool:
    return value == UNKNOWN or value is None


def _image_header_dims(path: Path, fmt: str) -> tuple[int, int] | None:
    """Width/height from a PNG or PNM header (stdlib only)."""
    with path.open("rb") as f:
        head = f.read(64)
    if fmt == "png":
        if head[:8] != b"\x89PNG\r\n\x1a\n" or head[12:16] != b"IHDR":
            return None
        w, h = struct.unpack(">II", head[16:24])
        return int(w), int(h)
    if fmt in ("pgm", "ppm"):
        tokens: list[bytes] = []
        for line in head.split(b"\n"):
            line = line.split(b"#", 1)[0]
            tokens.extend(line.split())
            if len(tokens) >= 3:
                break
        if len(tokens) < 3 or tokens[0] not in (b"P5", b"P6"):
            return None
        return int(tokens[1]), int(tokens[2])
    return None


def expected_raw_bytes(fmt: str, width: int, height: int, stride: int | None) -> int | None:
    """Exact byte count for headerless raw frames, or None if not checkable."""
    if fmt == "raw10p":
        line = stride or (width * 5 + 3) // 4
        return line * height
    if fmt == "raw16":
        return (stride or width * 2) * height
    if fmt == "abgr8888":
        return (stride or width * 4) * height
    if fmt == "yuyv":
        return (stride or width * 2) * height
    return None


def _check_enum(key: str, value: Any, where: str, issues: list[Issue]) -> None:
    allowed = ENUMS.get(key)
    if allowed is not None and value not in allowed:
        issues.append(Issue("ERROR", where, f"{key}={value!r} not in {list(allowed)}"))


def _validate_targets(targets: Any, issues: list[Issue]) -> None:
    if not isinstance(targets, dict):
        issues.append(Issue("ERROR", "targets", "must be an object keyed by target id"))
        return
    for tid, t in targets.items():
        where = f"targets.{tid}"
        kind = t.get("kind")
        if kind not in TARGET_KINDS:
            issues.append(Issue("ERROR", where, f"kind={kind!r} not in {list(TARGET_KINDS)}"))
            continue
        if kind == "charuco":
            sq = t.get("squares")
            if not (
                isinstance(sq, list)
                and len(sq) == 2
                and all(isinstance(v, int) and not isinstance(v, bool) and v >= 3 for v in sq)
            ):
                issues.append(Issue("ERROR", where, "squares must be [cols, rows], each >= 3"))
            if t.get("dictionary") not in ARUCO_DICTIONARIES:
                issues.append(Issue("ERROR", where, f"dictionary must be in {ARUCO_DICTIONARIES}"))
            ratio = t.get("marker_to_square")
            if not is_number(ratio) or not 0.3 <= ratio <= 0.9:
                issues.append(Issue("ERROR", where, "marker_to_square must be in [0.3, 0.9]"))
            _dimension(t, "square_mm", where, issues)
        elif kind == "scout_reference":
            _dimension(t, "reference_mm", where, issues)
        elif kind == "planar_part":
            truth = t.get("truth", {})
            for k in ("length_mm", "width_mm"):
                if k in truth and not is_unknown(truth[k]):
                    _dimension(truth, k, f"{where}.truth", issues)
            ref = t.get("reference_target")
            if ref is not None and ref not in targets:
                issues.append(Issue("ERROR", where, f"reference_target {ref!r} is not a target"))


def _dimension(obj: dict[str, Any], key: str, where: str, issues: list[Issue]) -> None:
    value = obj.get(key)
    if is_unknown(value):
        issues.append(Issue("WARN", where, f"{key} is UNKNOWN"))
        return
    if not is_number(value) or value <= 0:
        issues.append(Issue("ERROR", where, f"{key} must be a positive number"))
    status = obj.get(f"{key}_status", obj.get("status", UNKNOWN))
    if status not in DIMENSION_STATUS:
        issues.append(Issue("ERROR", where, f"{key}_status={status!r} not in {DIMENSION_STATUS}"))
    elif status != "verified":
        issues.append(
            Issue(
                "WARN",
                where,
                f"{key}={value} is {status}: usable for shape, not for absolute accuracy",
            )
        )


def dimension_verified(obj: dict[str, Any], key: str) -> bool:
    status = obj.get(f"{key}_status", obj.get("status", UNKNOWN))
    return status == "verified" and not is_unknown(obj.get(key))


def load_dataset(root: Path, verify_hashes: bool = True) -> Dataset:
    root = root.resolve()
    issues: list[Issue] = []
    path = root / "dataset.json"
    try:
        manifest = json.loads(path.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        return Dataset(root, {}, [], [Issue("ERROR", str(path), f"cannot read manifest: {exc}")])

    if manifest.get("schema") != DATASET_SCHEMA:
        issues.append(Issue("ERROR", "schema", f"expected {DATASET_SCHEMA!r}"))
    if manifest.get("evidence_type") not in EVIDENCE_TYPES:
        issues.append(Issue("ERROR", "evidence_type", f"must be one of {EVIDENCE_TYPES}"))
    if not manifest.get("dataset_id"):
        issues.append(Issue("ERROR", "dataset_id", "missing"))

    for section, keys in (("camera", CAMERA_FIELDS), ("platform", PLATFORM_FIELDS)):
        block = manifest.get(section)
        if not isinstance(block, dict):
            issues.append(Issue("ERROR", section, "missing section"))
            continue
        for k in keys:
            if k not in block:
                issues.append(Issue("ERROR", f"{section}.{k}", 'required (use "UNKNOWN")'))
            elif is_unknown(block[k]):
                issues.append(Issue("WARN", f"{section}.{k}", "UNKNOWN"))
            else:
                _check_enum(k, block[k], f"{section}.{k}", issues)

    targets = manifest.get("targets", {})
    _validate_targets(targets, issues)

    defaults = manifest.get("defaults", {})
    frames: list[Frame] = []
    seen: set[str] = set()
    raw_frames = manifest.get("frames")
    if not isinstance(raw_frames, list) or not raw_frames:
        issues.append(Issue("ERROR", "frames", "must be a non-empty list"))
        raw_frames = []

    dataset_id = str(manifest.get("dataset_id", root.name))
    for idx, raw in enumerate(raw_frames):
        data = {**defaults, **raw}
        fid = str(data.get("frame_id", f"#{idx}"))
        fr = Frame(dataset_id, fid, data, None)
        where = f"frames.{fid}"
        if fid in seen:
            fr.issues.append(Issue("ERROR", where, "duplicate frame_id"))
        seen.add(fid)
        outcome = data.get("outcome", "ok")
        _check_enum("outcome", outcome, where, fr.issues)
        for k in CAPTURE_FIELDS:
            if k not in data:
                fr.issues.append(Issue("ERROR", f"{where}.{k}", 'required (use "UNKNOWN")'))
            elif k in ENUMS and not is_unknown(data[k]):
                _check_enum(k, data[k], f"{where}.{k}", fr.issues)
        tlist = data.get("targets")
        if isinstance(tlist, list):
            for t in tlist:
                if t not in targets:
                    fr.issues.append(Issue("ERROR", where, f"unknown target {t!r}"))
        elif not is_unknown(tlist):
            fr.issues.append(Issue("ERROR", where, "targets must be a list of target ids"))

        if outcome != "ok":
            # Failures are preserved and reported, never analysed.
            if not data.get("failure_reason"):
                fr.issues.append(Issue("WARN", where, f"outcome={outcome} without failure_reason"))
            fr.usable = False
            frames.append(fr)
            continue

        for k in IMAGE_FIELDS:
            if k not in data:
                fr.issues.append(Issue("ERROR", f"{where}.{k}", "required for an ok frame"))
        fmt = data.get("format")
        _check_enum("format", fmt, where, fr.issues)
        rel = data.get("path")
        if isinstance(rel, str):
            ap = (root / rel).resolve()
            if root not in ap.parents:
                fr.issues.append(Issue("ERROR", where, "path escapes the dataset directory"))
            elif not ap.is_file():
                fr.issues.append(Issue("ERROR", where, f"source file missing: {rel}"))
            else:
                fr.abs_path = ap
        if fr.abs_path is not None and fmt in ENUMS["format"]:
            _verify_file(fr, fmt, verify_hashes)
        fr.usable = not any(i.level == "ERROR" for i in fr.issues)
        frames.append(fr)

    if frames and not any(f.usable for f in frames):
        issues.append(Issue("ERROR", "frames", "no usable frame"))
    return Dataset(root, manifest, frames, issues)


def _verify_file(fr: Frame, fmt: str, verify_hashes: bool) -> None:
    where = f"frames.{fr.frame_id}"
    assert fr.abs_path is not None
    data = fr.data
    w, h = data.get("width"), data.get("height")
    if not all(isinstance(v, int) and not isinstance(v, bool) and v > 0 for v in (w, h)):
        fr.issues.append(Issue("ERROR", where, "width/height must be positive integers"))
        return
    if fmt in RAW_FORMATS:
        stride = data.get("stride")
        want = expected_raw_bytes(fmt, w, h, int(stride) if stride else None)
        got = fr.abs_path.stat().st_size
        if want is not None and got != want:
            fr.issues.append(Issue("ERROR", where, f"{fmt} {w}x{h}: {got} bytes, expected {want}"))
        if fmt in ("raw10p", "raw16"):
            if data.get("bayer_pattern") not in BAYER_PATTERNS:
                fr.issues.append(
                    Issue("ERROR", where, f"bayer_pattern must be in {BAYER_PATTERNS}")
                )
            for k in ("black_level", "white_level"):
                if not is_number(data.get(k)):
                    fr.issues.append(Issue("ERROR", where, f"{k} required for raw Bayer"))
    else:
        dims = _image_header_dims(fr.abs_path, fmt)
        if dims is None:
            fr.issues.append(Issue("ERROR", where, f"not a readable {fmt} file"))
        elif dims != (w, h):
            fr.issues.append(Issue("ERROR", where, f"header says {dims}, manifest says {(w, h)}"))
    if verify_hashes:
        actual = sha256_file(fr.abs_path)
        recorded = data.get("sha256")
        if is_unknown(recorded):
            fr.issues.append(Issue("WARN", where, f"no recorded sha256 (actual {actual})"))
        elif recorded != actual:
            fr.issues.append(
                Issue("ERROR", where, "sha256 mismatch: source changed since it was recorded")
            )
        data["_sha256_actual"] = actual


def load_experiment(
    root: Path, verify_hashes: bool = True
) -> tuple[dict[str, Any], list[Dataset], list[Issue]]:
    root = root.resolve()
    issues: list[Issue] = []
    try:
        exp = json.loads((root / "experiment.json").read_text())
    except (OSError, json.JSONDecodeError) as exc:
        return {}, [], [Issue("ERROR", "experiment.json", f"cannot read: {exc}")]
    if exp.get("schema") != EXPERIMENT_SCHEMA:
        issues.append(Issue("ERROR", "schema", f"expected {EXPERIMENT_SCHEMA!r}"))
    if exp.get("evidence_type") not in EVIDENCE_TYPES:
        issues.append(Issue("ERROR", "evidence_type", f"must be one of {EVIDENCE_TYPES}"))
    datasets: list[Dataset] = []
    for rel in exp.get("datasets", []):
        ds = load_dataset(root / rel, verify_hashes)
        if ds.evidence_type != exp.get("evidence_type"):
            issues.append(
                Issue(
                    "ERROR",
                    rel,
                    f"dataset evidence_type {ds.evidence_type!r} differs from experiment's",
                )
            )
        datasets.append(ds)
    if not datasets:
        issues.append(Issue("ERROR", "datasets", "experiment lists no datasets"))
    return exp, datasets, issues


def validation_summary(exp_issues: list[Issue], datasets: list[Dataset]) -> dict[str, Any]:
    out: dict[str, Any] = {"experiment_issues": [i.as_dict() for i in exp_issues], "datasets": []}
    for ds in datasets:
        frames = []
        for f in ds.frames:
            frames.append(
                {
                    "frame_id": f.frame_id,
                    "usable": f.usable,
                    "outcome": f.get("outcome", "ok"),
                    "sha256": f.get("_sha256_actual", f.get("sha256")),
                    "issues": [i.as_dict() for i in f.issues],
                }
            )
        out["datasets"].append(
            {
                "dataset_id": ds.dataset_id,
                "sku": ds.sku,
                "evidence_type": ds.evidence_type,
                "ok": ds.ok,
                "issues": [i.as_dict() for i in ds.issues],
                "frames": frames,
                "usable_frames": sum(f.usable for f in ds.frames),
                "failed_or_discarded_frames": sum(
                    f.get("outcome", "ok") != "ok" for f in ds.frames
                ),
            }
        )
    errors = sum(i.level == "ERROR" for i in exp_issues) + sum(
        (not d["ok"]) + sum(any(x["level"] == "ERROR" for x in fr["issues"]) for fr in d["frames"])
        for d in out["datasets"]
    )
    out["error_count"] = int(errors)
    return out
