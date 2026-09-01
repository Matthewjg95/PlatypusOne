# CAD Handoff Architecture

Status: **contest path selected; post-contest adapters planned**
Date: **2026-09-01**

## Decision

PlatypusOne's internal engineering record remains CAD-vendor neutral. The Autodesk
contest build must additionally demonstrate a real handoff into Autodesk Fusion.

The boundary is:

**sensors → Engineering Observation → reviewed measurement package → CAD adapter**

The Engineering Observation must not contain Fusion-only assumptions. A CAD adapter
translates the reviewed, unit-aware evidence into the target tool.

## Autodesk contest adapter

The bounded contest handoff is:

1. capture camera, multizone depth, orientation, and calibration evidence;
2. calculate measurements and retain uncertainty/provenance;
3. require user review of the values that will drive CAD;
4. export a Fusion-ready parameter package;
5. import/update named user parameters in a prepared Fusion design;
6. show the resulting parametric feature, sketch, or template update.

The minimum credible demonstration is named dimensional parameters with units,
source-observation identity, and confidence. Generating an arbitrary finished CAD
model is not required.

Candidate implementation:

- canonical Engineering Observation JSON remains the source record;
- a small adapter produces the CSV format consumed by Fusion Parameter I/O or a
  project-specific Fusion Python add-in;
- the Fusion-side template/add-in retains a link to the source observation and image;
- later work may translate confirmed contours or points into sketch geometry.

## Post-contest architecture

Post-contest PlatypusOne should support selectable adapters rather than make Fusion
the permanent data model.

Planned adapter classes may include:

- Autodesk Fusion;
- neutral CSV/JSON parameter export;
- DXF/SVG 2D geometry where the evidence supports it;
- FreeCAD or other CAD integrations;
- STEP or native solid generation only after a geometry pipeline can justify it.

Every adapter must preserve units, provenance, uncertainty, coordinate frames, and
the distinction between observed, measured, derived, inferred, and unresolved data.

## Guardrails

- Never allow a CAD adapter to promote an inference into a measured dimension.
- Never discard the original images, calibration evidence, or observation identity.
- User confirmation is required before measurements drive a CAD model.
- The contest Fusion adapter is Core; generalized CAD generation remains Stretch.
- A CAD-specific failure must not corrupt or invalidate the canonical observation.
