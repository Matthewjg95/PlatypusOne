# PlatypusOne AI Pipeline

Status: **architecture baseline**
Date: **2026-09-22**

## North star

PlatypusOne converts imperfect observations of the physical world into useful engineering artifacts.

**Observation -> Engineering Artifact**

The product is not defined by a particular AI model. Models are replaceable components inside the system.

## AI doctrine

PlatypusOne should own the difficult boundary between the physical world and engineering information:

- sensor acquisition
- calibration
- measurement
- geometry reconstruction
- sensor fusion
- uncertainty
- engineering context
- design constraints
- provenance
- CAD interoperability
- human verification
- physical-system interfaces

AI should primarily interpret, reason over, organize, and transform the structured information produced by those systems:

- object and feature interpretation
- engineering-semantic understanding
- design-intent hypotheses
- knowledge retrieval
- missing-information identification
- measurement planning
- explanation
- proposed engineering actions
- translation from structured geometry into candidate engineering outputs

### Architectural rule

Never make PlatypusOne dependent on one foundation model.

The stable boundary is:

**Physical sensors -> deterministic processing -> structured engineering state -> interchangeable AI -> engineering artifact**

As models improve, PlatypusOne should become more capable without changing its fundamental architecture.

### Product rule

Do not spend scarce development effort recreating capabilities frontier models are likely to commoditize.

Spend it on measurement, calibration, geometry, uncertainty, sensor fusion, engineering constraints, provenance, deterministic tooling, and physical interaction.

## Relationship to existing contracts

The canonical structured engineering state is the existing
[Engineering Observation Contract](ENGINEERING_OBSERVATION_CONTRACT.md).

That contract already separates:

- observed evidence
- derived measurements
- inferred claims
- unresolved questions
- recommended next observations
- provenance
- human review

The AI pipeline must consume and produce information through that contract rather than inventing a separate AI-only representation.

The CAD boundary remains the existing
[CAD Handoff Architecture](CAD_HANDOFF.md):

**sensors -> Engineering Observation -> reviewed measurement package -> CAD adapter**

AI may propose design intent or candidate geometry, but deterministic software and reviewed measurements remain responsible for creating engineering geometry.

## Functional pipeline

```text
RGB / ToF / IMU / future sensors
        |
        v
Capture + calibration
        |
        v
Deterministic perception
        |
        v
EngineeringObservation
        |
        +---- observed
        +---- derived
        +---- unresolved
        |
        v
EngineeringReasoner
        |
        +---- inferred
        +---- recommended_next_observations
        +---- design-intent hypotheses
        |
        v
Human review / correction
        |
        v
EngineeringArtifactSpec
        |
        v
CAD / documentation / fixture / inspection / test adapters
```

## Layer 1 — Perception

This is primarily deterministic CV and sensor processing, not an LLM task.

Initial responsibilities:

- camera calibration
- scale-reference detection
- segmentation
- edge detection
- line and circle detection
- hole/feature candidates
- OCR where useful
- pose estimation
- depth alignment when ToF is available
- physical dimension estimation
- confidence/uncertainty assignment

Initial implementation should work with a webcam or saved image before final PlatypusOne cameras arrive.

## Layer 2 — Structured engineering state

Every capture produces a valid `EngineeringObservation`.

Example derived state for a simple bracket:

```text
Object: candidate sheet-metal mounting bracket

Plane A:
  estimated size: 84.2 x 41.7 mm
  confidence: 0.91

Hole 1:
  diameter: 5.8 mm
  center: [12.4, 10.1]
  confidence: 0.82

Hole 2:
  diameter: 5.9 mm
  center: [71.8, 10.0]
  confidence: 0.79

Unresolved:
  material thickness
  bend radius
  hidden rear geometry
```

The key result is not a prose description. It is a machine-readable engineering record with provenance and uncertainty.

## Layer 3 — EngineeringReasoner

The model-facing boundary should be an interface, not a specific provider.

Conceptually:

```text
EngineeringReasoner
    |
    +-- CloudModelAdapter
    +-- LocalModelAdapter
    +-- MockModelAdapter
```

The reasoner receives an `EngineeringObservation` plus optional image artifacts and project context.

It may produce:

- object/class hypotheses
- design-intent hypotheses
- ambiguity analysis
- missing measurements
- recommended next capture
- candidate engineering primitives
- knowledge-retrieval queries
- explanation for the user

It must never rewrite observed or derived evidence as if it were measured fact.

### Example interaction

Input:

- calibrated front image
- two measured holes
- rectangular contour
- unresolved thickness/bend geometry

Reasoner output:

```text
Hypothesis:
Mounting bracket, likely symmetric about X centerline.

Design-intent hypothesis:
Hole pair likely shares nominal diameter and horizontal datum.

Missing measurements:
1. material thickness
2. bend radius
3. rear flange length

Recommended next observation:
Capture right-side profile approximately perpendicular to Plane A.

Reason:
This view should resolve flange length and bend geometry.
```

This is the beginning of active measurement guidance.

## Layer 4 — EngineeringArtifactSpec

AI should not directly emit arbitrary CAD commands.

A deterministic intermediate representation should sit between reasoning and CAD.

Example:

```text
EngineeringArtifactSpec

SketchPlane: XY

Rectangle:
  width = 84.2 mm
  height = 41.7 mm

HolePattern:
  diameter = 6.0 mm
  count = 2
  spacing = 59.4 mm
  centered = true

Extrusion:
  thickness = 1.6 mm

Bend:
  angle = 90 deg
  radius = unresolved
```

Adapters can later translate reviewed artifact specifications into Fusion parameters, DXF/SVG, FreeCAD scripts, STEP-generation workflows, Mesh2CAD intermediates, documentation, fixtures, or test artifacts.

## Milestones

### AI-01 — Observation -> Structured Engineering State

Goal: prove the deterministic perception/evidence pipeline.

Input:

- RGB image
- known scale reference / fiducial

Output:

- valid `EngineeringObservation` JSON

Minimum content:

- source image artifact
- calibration information
- object mask or contour
- bounding dimensions
- primitive feature candidates
- estimated measurements
- confidence where meaningful
- unresolved information
- provenance/method identity

First benchmark:

> Place a simple planar mechanical part beside a known fiducial, capture it with a webcam, and produce numerical geometry with uncertainty.

Recommended targets:

- flat plate
- washer
- simple bracket
- PCB
- enclosure panel
- wrench silhouette

Do not generate CAD in AI-01.

### AI-02 — Engineering Reasoner

Goal: turn structured evidence into useful engineering interpretation.

Input:

- `EngineeringObservation`
- referenced image artifacts
- optional project/user intent

Output:

- inferred claims
- design-intent hypotheses
- unresolved questions
- recommended next observations
- candidate engineering primitives

Success gate:

The system identifies one missing piece of evidence and recommends a concrete next capture likely to reduce that uncertainty.

### AI-03 — Artifact Generator

Goal: convert reviewed engineering state into a deterministic artifact specification and adapter output.

Initial path:

**EngineeringObservation -> human review -> EngineeringArtifactSpec -> Fusion-ready parameter package**

Later paths:

- DXF/SVG
- sketch geometry
- Mesh2CAD
- FreeCAD
- STEP/solid generation when justified
- fixture/tool concepts
- inspection plans
- work instructions

## First implementation slice

Build the smallest end-to-end AI-01 prototype before final hardware integration:

```text
camera/saved image
  -> calibration target detection
  -> object segmentation
  -> contour extraction
  -> primitive detection
  -> scale conversion
  -> dimension estimates
  -> uncertainty/confidence
  -> EngineeringObservation.json
  -> simple human-readable renderer
```

The first host prototype now lives in [tools/ai01](../../tools/ai01/README.md).
It emits contract v0.1, preserves debug/evidence artifacts, and documents the
compatibility boundary with the existing C++ Scout path. Real-capture acceptance
remains pending.

Suggested initial dependency set:

- Python
- OpenCV
- NumPy
- existing PlatypusOne Engineering Observation JSON contract

Keep this prototype isolated enough that algorithms can later be moved behind PlatypusOS services.

## Scope guardrail

Before adding a technology to PlatypusOne, ask:

> Does this materially improve Observation -> Engineering Artifact?

Examples:

- RGB camera: core
- ToF: core when it improves geometry/measurement
- trigger/ergonomic acquisition UI: core
- calibration: core
- Mesh2CAD integration: core to long-term artifact generation
- local AI: useful implementation choice, not product identity
- cloud AI: useful implementation choice, not product identity
- radar: only after a specific engineering observation use case is demonstrated
- thermal imaging: candidate future sensor with strong inspection use cases
- generic chatbot features: non-core unless they directly advance the engineering workflow

## Long-term workflow

**Observe -> Measure -> Understand -> Reconstruct -> Retrieve -> Design -> Validate -> Act**

Early versions only need to establish the evidence chain and progressively close this loop.

The project should continually remain one layer closer to the physical engineering problem than the foundation model itself.
