# Engineering Observation Contract v0.1

Status: proposed foundation for Engineering Scout Q and PlatypusOne perception workflows.

## Purpose

PlatypusOne must distinguish what the device directly observed from what software calculated or AI inferred. This contract is the common evidence model used by Engineering Scout Q first, then measurement, inspection, ShadowScan, documentation, and future sensing apps.

The core rule is:

> Never present inference as measurement. Preserve enough provenance that a human can understand how every engineering claim was produced.

## Minimal record

Each `EngineeringObservation` represents one capture/inspection event and contains:

- `observation_id` — stable unique identifier
- `timestamp_utc`
- `source` — camera/sensor/app identity and device metadata
- `artifacts` — raw images, crops, sensor samples, calibration target detections
- `observed[]` — facts directly returned by a sensor or deterministic detector
- `derived[]` — values calculated from observed data
- `inferred[]` — classifications/interpretations produced by AI or heuristic reasoning
- `unresolved[]` — information the system cannot support with current evidence
- `recommended_next_observations[]` — specific actions that would reduce uncertainty
- `human_review` — accept/reject/correct state plus optional notes

Every claim in `observed`, `derived`, or `inferred` carries:

- `name`
- `value`
- `unit` when applicable
- `confidence` in the range 0–1 when meaningful
- `provenance[]` linking the claim to source artifacts/claims
- `method` describing the measurement, algorithm, model, or rule used

## Evidence classes

### Observed

Directly acquired evidence. Examples:

- pixel coordinates of detected edges
- ToF range sample
- IMU orientation sample
- calibration marker corners
- OCR text returned from an image region

An observed value may still have uncertainty, but it should not require semantic interpretation.

### Derived

Deterministic calculation from observed evidence. Examples:

- shaft diameter calculated from calibrated image scale
- object length calculated from two detected endpoints
- angle calculated from IMU samples
- estimated physical area from segmented pixels and scale

A derived value must point to the observations used to calculate it and record the method/version.

### Inferred

Interpretation that could be wrong even if the underlying observations are correct. Examples:

- `socket_head_cap_screw`
- likely nominal size `M5 x 20`
- probable material `steel`
- possible standard `ISO 4762`
- suspected corrosion

Inference must never overwrite observed or derived data.

### Unresolved

Questions for which current evidence is insufficient. Examples:

- thread pitch not visible
- hidden feature geometry unknown
- material cannot be established visually
- head height cannot be measured from current view

An unresolved item should explain why it is unresolved when possible.

### Recommended next observation

A concrete acquisition action tied to one or more unresolved questions. Examples:

- `Rotate part approximately 90 degrees and scan again to measure head height.`
- `Move camera closer until the calibration marker occupies at least 15% of frame width.`
- `Place the part flat beside the reference marker.`

This is the beginning of active measurement guidance: PlatypusOne should know what evidence it still needs.

## Engineering Scout Q v0.1 example

```json
{
  "observation_id": "scan-0042",
  "source": {"app": "engineering_scout", "camera": "uvc0"},
  "observed": [
    {"id": "obs-marker", "name": "aruco_marker_side", "value": 20.0, "unit": "mm", "method": "aruco_4x4"}
  ],
  "derived": [
    {
      "id": "drv-diameter",
      "name": "shaft_diameter",
      "value": 4.94,
      "unit": "mm",
      "confidence": 0.96,
      "provenance": ["obs-marker", "artifact:image-0042"],
      "method": "calibrated_contour_v1"
    },
    {
      "id": "drv-length",
      "name": "overall_length",
      "value": 19.8,
      "unit": "mm",
      "confidence": 0.94,
      "provenance": ["obs-marker", "artifact:image-0042"],
      "method": "calibrated_contour_v1"
    }
  ],
  "inferred": [
    {
      "id": "inf-class",
      "name": "part_class",
      "value": "socket_head_cap_screw",
      "confidence": 0.93,
      "provenance": ["artifact:image-0042"],
      "method": "classifier_v1"
    },
    {
      "id": "inf-nominal",
      "name": "likely_nominal_size",
      "value": "M5 x 20",
      "confidence": 0.89,
      "provenance": ["drv-diameter", "drv-length", "inf-class"],
      "method": "fastener_nominal_match_v1"
    }
  ],
  "unresolved": [
    {"name": "thread_pitch", "reason": "not resolvable from current view"}
  ],
  "recommended_next_observations": [
    {"action": "Capture a closer side view of the threaded region to estimate pitch."}
  ],
  "human_review": {"state": "pending"}
}
```

## v0.1 design constraints

1. Keep the contract serialization-friendly: JSON first; C++ structs mirror it later.
2. Do not require a database for the DigiKey milestone. A directory containing JSON plus referenced image files is sufficient.
3. Claims are append-only during processing. Corrections create review/correction records rather than silently replacing source evidence.
4. Confidence is optional for deterministic raw observations but required for AI inference.
5. Every inferred engineering claim must expose provenance and method/model identity.
6. The contract is sensor-agnostic. Camera, ToF, IMU, color, RF, and future modules should all be able to contribute evidence.
7. The contract should support multiple captures being grouped into one project/object record later; v0.1 does not need to solve object-level fusion.

## DigiKey success gate

By September 30, Engineering Scout Q should be able to create one valid record from a real UNO Q capture containing at least:

- source image artifact
- calibration evidence
- one derived physical measurement
- one inferred classification
- confidence/provenance
- at least one unresolved field when appropriate
- saved JSON record
- human-readable UI rendering of the same evidence classes

## Deferred until after the DigiKey submission

- multi-observation object fusion
- schema migration/version negotiation
- database/indexing layer
- cryptographic evidence signing
- generalized ontology/knowledge graph
- cloud synchronization
- automatic CAD generation

Those may become useful for PlatypusOne, but none are needed to prove the core evidence-chain concept in September.
