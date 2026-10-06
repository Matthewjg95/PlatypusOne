"""CLI adapter for saved images or one webcam frame; emits contract v0.1."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
from datetime import UTC, datetime
from pathlib import Path
from typing import Any, cast
from uuid import uuid4

import cv2

from services.vision.python.ai01 import METHOD, Image, SceneError, analyze


def write_json(path: Path, value: dict[str, Any]) -> None:
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def write_image(path: Path, image: Image) -> None:
    if not cv2.imwrite(str(path), image):
        raise OSError(f"could not write {path}")


def capture(
    output: Path,
    reference_mm: float,
    image_path: Path | None = None,
    camera: int | None = None,
    *,
    timestamp: str | None = None,
    observation_id: str | None = None,
) -> dict[str, Any]:
    """Save a new capture directory; never overwrite prior evidence.

    Failed analysis leaves source/config/failure.json for diagnosis, no success JSON.
    Tests may inject event identity/time; production uses UUID and UTC capture time.
    """
    if (image_path is None) == (camera is None):
        raise ValueError("supply exactly one image path or camera index")
    output.mkdir(parents=True, exist_ok=False)
    if image_path is not None:
        raw = output / ("source" + image_path.suffix.lower())
        shutil.copyfile(image_path, raw)
        image = cv2.imread(str(raw), cv2.IMREAD_COLOR)
        source = {"app": "ai01_planar", "input": "image", "filename": image_path.name}
    else:
        device = cv2.VideoCapture(camera if camera is not None else 0)
        try:
            ok, image = device.read()
            if not ok:
                raise OSError(f"could not capture webcam {camera}")
        finally:
            device.release()
        raw = output / "source.png"
        write_image(raw, cast(Image, image))
        source = {"app": "ai01_planar", "input": "webcam", "camera": str(camera)}
    if image is None:
        raise ValueError("source image could not be decoded")
    timestamp = timestamp or datetime.now(UTC).isoformat().replace("+00:00", "Z")
    source.update(
        method=METHOD, opencv=cv2.__version__, sha256=hashlib.sha256(raw.read_bytes()).hexdigest()
    )
    write_json(
        output / "capture-config.json",
        {
            "reference_side_mm": reference_mm,
            "reference_kind": "filled_square",
            "reference_size_origin": "operator input; not measured by camera",
            "timestamp_semantics": "acquisition/processing event, not source file EXIF time",
            "source": source,
        },
    )
    try:
        evidence, mask, overlay = analyze(cast(Image, image), reference_mm)
    except (SceneError, ValueError) as error:
        write_json(output / "failure.json", {"error": str(error), "source": source})
        raise
    write_image(output / "subject-mask.png", mask)
    write_image(output / "debug.png", overlay)
    write_json(output / "geometry.json", evidence.pop("geometry"))
    unresolved = [
        {"name": "thickness", "reason": "single planar silhouette has no depth evidence"},
        {"name": "hidden_geometry", "reason": "rear surfaces and through-hole status not observed"},
        {"name": "material", "reason": "appearance does not establish material"},
        {
            "name": "absolute_accuracy",
            "reason": "coplanarity, lens distortion, perspective, "
            "reference scale and segmentation bias are unverified; sensitivity is not accuracy",
        },
        {"name": "part_class", "reason": "no semantic classifier invoked for generic planar parts"},
    ]
    record: dict[str, Any] = {
        "schema_version": "0.1",
        "observation_id": observation_id or "ai01-" + uuid4().hex,
        "timestamp_utc": timestamp,
        "source": source,
        "artifacts": [
            {"id": "raw-image", "kind": "image", "path": raw.name},
            {"id": "capture-config", "kind": "calibration", "path": "capture-config.json"},
            {"id": "geometry", "kind": "calibration/geometry", "path": "geometry.json"},
            {"id": "subject-mask", "kind": "image/png", "path": "subject-mask.png"},
            {"id": "debug", "kind": "image/png", "path": "debug.png"},
        ],
        **evidence,
        "inferred": [],
        "unresolved": unresolved,
        "recommended_next_observations": [
            {
                "action": "Capture a side view and measure thickness with calipers.",
                "resolves": ["thickness", "hidden_geometry"],
            },
            {
                "action": "Verify printed reference with calipers; place it in the same plane as "
                "the subject; capture perpendicular to the plane. Validate against a known part.",
                "resolves": ["absolute_accuracy"],
            },
        ],
        "human_review": {"state": "pending"},
    }
    write_json(output / "EngineeringObservation.json", record)
    return record


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    inputs = parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument("--image", type=Path)
    inputs.add_argument("--camera", type=int)
    parser.add_argument("--reference-mm", type=float, required=True)
    parser.add_argument("--output", type=Path, required=True, help="new capture directory")
    args = parser.parse_args()
    try:
        capture(args.output, args.reference_mm, args.image, args.camera)
    except (ValueError, OSError, cv2.error) as error:
        parser.exit(2, f"AI-01: {error}\n")
    print(args.output / "EngineeringObservation.json")
    print(f"Review {args.output / 'debug.png'}; dimensions are conditional on scene setup.")


if __name__ == "__main__":
    main()
