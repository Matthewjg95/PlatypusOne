# Engineering Scout Q — DigiKey Dream Lab MVP

Deadline: **September 30, 2026**

Engineering Scout Q is the first working PlatypusOne perception vertical slice. It is not a separate product architecture. It proves the core PlatypusOne thesis: convert imperfect physical observations into structured, useful engineering information.

## Contest demo

Place an unknown fastener beside a calibration reference, trigger a scan, and produce a saved Engineering Observation containing a calibrated physical measurement, a likely part classification, confidence/provenance, unresolved information, and a recommended next observation when evidence is insufficient.

## Must ship

1. **UNO Q capture path** — camera image acquired on the real UNO Q.
2. **Physical trigger** — a real button/MCU event initiates capture; controlled illumination is preferred if hardware allows.
3. **Calibration evidence** — reference target detected and retained in the record.
4. **One physical measurement** — target shaft diameter and/or length with units.
5. **One classification** — target fastener class and likely nominal size.
6. **Engineering Observation v0.1** — output follows `docs/architecture/ENGINEERING_OBSERVATION_CONTRACT.md`.
7. **Honest uncertainty** — unresolved fields remain unresolved; inference never masquerades as measurement.
8. **Persistence** — JSON + referenced image saved as a reproducible record.
9. **Human-readable result** — UI clearly separates OBSERVED / DERIVED / INFERRED / UNRESOLVED.
10. **Validation** — repeatable test set, not a single cherry-picked demonstration.

## Winning target

- Fastener dataset of roughly 20 varied objects/captures.
- Report dimensional error and classification hit rate.
- Demonstrate at least one intentional failure case.
- Provide active guidance such as `rotate part 90 degrees` when current evidence cannot answer a question.
- Show meaningful use of both UNO Q compute domains: Linux perception/inference and STM32 deterministic I/O.
- Submission video communicates the complete scan-to-engineering-record loop in under one minute.

## Scope lock

### In scope

- bolts / screws / nuts / washers as the initial object family
- single-view calibrated measurement
- simple nominal-size matching
- uncertainty/provenance
- one follow-up observation recommendation
- local storage

### Explicitly deferred

- recognize arbitrary engineering objects
- full ShadowScan / 3D reconstruction
- automatic CAD generation
- cloud services
- knowledge graph / generalized ontology
- multi-view fusion
- polished PlatypusOne enclosure
- production display integration
- ToF/IMU/color-sensor feature suite
- autonomous robotics

If a feature does not directly improve the contest demo, validation, or reusable PlatypusOne perception stack, it waits until after September 30.

## Reuse into Autodesk build

The DigiKey work must land behind reusable PlatypusOS boundaries:

- `platform`: camera + MCU/illumination/trigger hardware interfaces
- `services/vision`: calibration, segmentation, contour/feature extraction
- `services/ai`: classification/inference interface
- engineering observation model: common evidence contract
- project/filesystem service: saved record + artifacts
- `apps/engineering_scout`: contest-facing workflow/UI

After DigiKey, these same components feed PlatypusOne measurement, inspection, documentation, and ShadowScan work. The canonical Engineering Observation remains CAD-neutral; the Autodesk build adds the bounded Fusion adapter defined in [CAD Handoff Architecture](../architecture/CAD_HANDOFF.md). No contest-only rewrite should be required.

## Build order

1. Freeze Engineering Observation v0.1 and create serializer/test fixtures.
2. Build a host-side Scout pipeline using stored test images.
3. Implement calibration + one deterministic measurement.
4. Implement fastener classification / nominal matching.
5. Render the evidence classes in the Scout UI.
6. Bring up UNO Q camera capture.
7. Add MCU trigger / illumination path.
8. Run the validation set and record failures.
9. Add active next-observation guidance if the core loop is stable.
10. Freeze functionality and spend final days on documentation/video/reliability.

## Definition of done

A judge can watch one uninterrupted run and understand:

**physical part → controlled capture → evidence → measurement → inference → uncertainty → saved engineering record**

The result must feel like the first function of a real engineering instrument, not an object-detection demo.