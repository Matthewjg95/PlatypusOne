"""Pure image-to-evidence prototype; no acquisition, model, or CAD dependencies."""

from __future__ import annotations

import math
from typing import Any

import cv2
import numpy as np
from numpy.typing import NDArray

Image = NDArray[np.uint8]
METHOD = "vision.ai01_planar.v1"


class SceneError(ValueError):
    """An unsupported scene must be recaptured, not silently measured."""


def claim(
    identifier: str,
    name: str,
    value: float | str | bool,
    unit: str | None,
    provenance: list[str],
    method: str = METHOD,
) -> dict[str, Any]:
    result: dict[str, Any] = dict(
        id=identifier, name=name, value=value, provenance=provenance, method=method
    )
    if unit is not None:
        result["unit"] = unit
    return result


def analyze(image: Image, reference_mm: float) -> tuple[dict[str, Any], Image, Image]:
    """Return evidence, subject mask, overlay for one square and one planar subject.

    Pixel-cell bounding rectangle differs from ScoutAnalyzer's minimum support
    width. IDs deliberately do not masquerade as its classifier inputs.
    """
    if not math.isfinite(reference_mm) or reference_mm <= 0:
        raise ValueError("reference_mm must be finite and positive")
    if image.dtype != np.uint8 or image.ndim != 3 or image.shape[2] != 3 or image.size == 0:
        raise ValueError("expected a nonempty uint8 BGR image")
    gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    threshold, binary = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY_INV | cv2.THRESH_OTSU)
    count, labels, stats, _ = cv2.connectedComponentsWithStats(binary, connectivity=8)
    blobs = [i for i in range(1, count) if stats[i, cv2.CC_STAT_AREA] >= 64]
    height, width = gray.shape
    for i in blobs:
        x, y, w, h, _ = map(int, stats[i])
        if x == 0 or y == 0 or x + w == width or y + h == height:
            raise SceneError("clipped foreground: keep reference and subject inside frame")
    squares = []
    for i in blobs:
        _, _, w, h, area = map(int, stats[i])
        if min(w, h) / max(w, h) >= 0.90 and area / (w * h) >= 0.85:
            squares.append(i)
    if not squares:
        raise SceneError("no reference: use a filled, nearly axis-aligned dark square")
    if len(squares) != 1:
        raise SceneError("ambiguous reference: more than one square candidate")
    reference = squares[0]
    subjects = [i for i in blobs if i != reference]
    if len(subjects) != 1:
        raise SceneError(
            "expected exactly one separate subject; remove clutter or separate objects"
        )
    subject = subjects[0]
    ref_area = int(stats[reference, cv2.CC_STAT_AREA])
    ref_side = math.sqrt(ref_area)
    scale = reference_mm / ref_side
    mask = np.where(labels == subject, 255, 0).astype(np.uint8)
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    contour = contours[0]
    # Bounding all four corners of boundary pixel cells avoids the N-1 pixel
    # centre extent error (including for rotated objects).
    points = contour.reshape(-1, 2).astype(np.float32)
    offsets = np.array([[-0.5, -0.5], [-0.5, 0.5], [0.5, -0.5], [0.5, 0.5]], dtype=np.float32)
    cell_corners = (points[:, None, :] + offsets).reshape(-1, 2)
    rect = cv2.minAreaRect(cell_corners)
    box = cv2.boxPoints(rect)
    length, breadth = sorted(map(float, rect[1]), reverse=True)
    overlay = image.copy()
    cv2.drawContours(overlay, [box.astype(np.int32)], 0, (0, 180, 0), 2)
    x, y, w, h, _ = map(int, stats[reference])
    cv2.rectangle(overlay, (x, y), (x + w - 1, y + h - 1), (255, 0, 0), 2)
    geometry: dict[str, Any] = {
        "method": METHOD,
        "coordinate_system": "image pixel centres; x right, y down; bounding box uses pixel cells",
        "threshold": threshold,
        "reference": {
            "bbox_xywh_px": [x, y, w, h],
            "area_px": ref_area,
            "side_mm_operator_input": reference_mm,
            "mm_per_pixel": scale,
        },
        "subject_contour_px": points.tolist(),
        "bounding_box_px": box.tolist(),
        "line_candidates": [],
        "circular_void_candidates": [],
        "parameters": {
            "min_component_area_px": 64,
            "reference_min_aspect": 0.90,
            "reference_min_fill": 0.85,
            "boundary_sensitivity_px": 1.0,
        },
    }
    observed = [
        claim("p-threshold", "binarization_threshold", float(threshold), None, ["raw-image"]),
        claim("p-ref-area", "reference_area", float(ref_area), "px^2", ["raw-image", "geometry"]),
        claim(
            "p-area",
            "subject_area",
            float(stats[subject, cv2.CC_STAT_AREA]),
            "px^2",
            ["subject-mask", "raw-image"],
        ),
    ]
    derived = [
        claim(
            "p-scale",
            "mm_per_pixel",
            scale,
            "mm/px",
            ["p-ref-area", "capture-config"],
            METHOD + "; operator reference_side_mm / sqrt(reference_area)",
        )
    ]

    def dimension(identifier: str, name: str, pixels: float) -> None:
        observed.append(claim(identifier + "-px", name, pixels, "px", ["geometry", "raw-image"]))
        derived.append(
            claim(
                identifier + "-mm",
                name,
                pixels * scale,
                "mm",
                [identifier + "-px", "p-scale"],
                METHOD + "; pixels * mm_per_pixel",
            )
        )
        # Sensitivity, not calibrated confidence or a metrology accuracy bound.
        low = max(0, pixels - 2) * reference_mm / (ref_side + 2)
        high = (pixels + 2) * reference_mm / (ref_side - 2)
        derived.append(
            claim(
                identifier + "-sensitivity",
                name + "_pixel_sensitivity",
                max(pixels * scale - low, high - pixels * scale),
                "mm",
                [identifier + "-px", "p-ref-area", "capture-config"],
                METHOD + "; max deviation for +/-1px per endpoint on subject and "
                "reference side; excludes print, lens, pose and segmentation bias",
            )
        )

    dimension("p-length", "subject_bounding_length", length)
    dimension("p-width", "subject_bounding_width", breadth)
    polygon = cv2.approxPolyDP(contour, 1.0, True).reshape(-1, 2)
    for a, b in zip(polygon, np.roll(polygon, -1, axis=0), strict=True):
        if np.linalg.norm(b - a) >= 10:
            geometry["line_candidates"].append(
                {
                    "endpoints_px": [a.tolist(), b.tolist()],
                    "method": "approxPolyDP epsilon=1px; candidate",
                }
            )
            cv2.line(overlay, tuple(map(int, a)), tuple(map(int, b)), (0, 200, 255), 1)
    # Enclosed light components are silhouette voids, not proof of through-holes.
    n_holes, hole_labels, hole_stats, _ = cv2.connectedComponentsWithStats(
        255 - mask, connectivity=8
    )
    hole_count = 0
    for i in range(1, n_holes):
        hx, hy, hw, hh, area = map(int, hole_stats[i])
        if hx == 0 or hy == 0 or hx + hw == width or hy + hh == height or area < 16:
            continue
        hole_count += 1
        hole_mask = np.where(hole_labels == i, 255, 0).astype(np.uint8)
        edges, _ = cv2.findContours(hole_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
        edge = edges[0]
        perimeter = cv2.arcLength(edge, True)
        circularity = 4 * math.pi * cv2.contourArea(edge) / max(perimeter**2, 1)
        if circularity < 0.80 or min(hw, hh) / max(hw, hh) < 0.90:
            continue
        moments = cv2.moments(hole_mask, binaryImage=True)
        center = [moments["m10"] / area, moments["m01"] / area]
        diameter = 2 * math.sqrt(area / math.pi)
        geometry["circular_void_candidates"].append(
            {
                "center_px": center,
                "area_px": area,
                "equivalent_diameter_px": diameter,
                "circularity": circularity,
                "contour_px": edge.reshape(-1, 2).tolist(),
            }
        )
        dimension(f"p-void-{hole_count}", "circular_void_equivalent_diameter", diameter)
        cv2.circle(overlay, tuple(map(round, center)), round(diameter / 2), (0, 0, 255), 2)
    observed.extend(
        [
            claim(
                "p-holes", "enclosed_light_region_count", float(hole_count), None, ["subject-mask"]
            ),
            claim(
                "p-lines",
                "line_candidate_count",
                float(len(geometry["line_candidates"])),
                None,
                ["geometry"],
            ),
        ]
    )
    cv2.putText(
        overlay,
        f"{length * scale:.2f} x {breadth * scale:.2f} mm (conditional)",
        (10, 25),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.6,
        (0, 100, 0),
        2,
    )
    return {"observed": observed, "derived": derived, "geometry": geometry}, mask, overlay
