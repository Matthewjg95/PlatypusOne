# Engineering Scout Q — DigiKey Dream Lab MVP

Deadline: **September 30, 2026**

Engineering Scout Q is the first working PlatypusOne perception vertical slice. It is not a separate product architecture. It proves the core PlatypusOne thesis: convert imperfect physical observations into structured, useful engineering information.

## Contest demo

Place an unknown fastener beside a known-size reference, trigger a scan, and produce a saved Engineering Observation containing a physical measurement, a likely part classification, confidence/provenance, unresolved information, and a recommended next observation when evidence is insufficient.

The goal is **not laboratory metrology** and it is not a calibration showcase. The reference is an in-frame scale anchor that lets the device turn pixels into useful engineering dimensions while preserving uncertainty.

## Core proof

The demo must prove this complete workflow on real hardware:

**physical part → capture → find reference + subject → derive scale → measure → classify → show confidence/uncertainty → save image + record**

A judge should be able to understand that loop even if the measurement is not yet production-metrology grade.

## Robustness requirement — real bench conditions

The workflow must not depend on a perfectly lit calibration scene.

Engineering Scout should work under **ordinary imperfect indoor conditions** such as:
- uneven overhead lighting
- moderate shadows
- different table/background brightness
- reasonable exposure variation
- small object rotations and placement variation

We do **not** need to solve darkness, severe glare, motion blur, transparent/reflective parts, or arbitrary clutter for the Dream Lab MVP.

### Design rule

Prefer algorithms and capture guidance that **degrade honestly** instead of demanding a laboratory setup.

The pipeline should:
1. attempt detection/measurement with the available frame;
2. calculate a simple scene/measurement quality score;
3. return a useful result when evidence is sufficient;
4. refuse or request another observation when it is not.

Controlled illumination is useful and should be demonstrated if available, but **it is an aid, not a prerequisite for the workflow to function**.

## Must ship

1. **UNO Q capture path** — camera image acquired on the real UNO Q.
2. **Physical trigger** — a real button/MCU event initiates capture.
3. **In-frame scale evidence** — known-size reference detected and retained in the record.
4. **One physical measurement** — target shaft diameter and/or length with units.
5. **One classification** — target fastener class and likely nominal size.
6. **Engineering Observation v0.1** — output follows `docs/architecture/ENGINEERING_OBSERVATION_CONTRACT.md`.
7. **Honest uncertainty** — unresolved fields remain unresolved; inference never masquerades as measurement.
8. **Persistence** — JSON + referenced image saved as a reproducible record.
9. **Human-readable result** — UI clearly separates OBSERVED / DERIVED / INFERRED / UNRESOLVED.
10. **Real-world validation** — repeatable physical captures across varied lighting/placement, not only synthetic scenes or one cherry-picked setup.

## Winning target

- 15–20 physical fasteners / captures, with deliberate variation in orientation and lighting.
- Report dimensional error and classification hit rate **by condition**, not just one aggregate.
- Include at least one successful capture in noticeably imperfect lighting.
- Include at least one intentional refusal/failure case.
- Provide active guidance such as `move closer`, `reduce glare`, `reference not found`, or `rotate part 90 degrees` when evidence is insufficient.
- Show meaningful use of both UNO Q compute domains: Linux perception/inference and STM32 deterministic I/O.
- Submission video communicates the complete scan-to-engineering-record loop in under one minute.

## Validation matrix

Do not over-invest in calibration refinement before this matrix exists.

| Condition | Minimum target |
|---|---|
| Even desk lighting | baseline measurement + classification |
| Uneven overhead light | completes workflow or gives useful retry guidance |
| Moderate shadow across scene | completes workflow or gives useful retry guidance |
| Brighter/darker exposure | completes workflow across a reasonable range |
| Part rotated | measurement remains useful |
| Reference partially difficult to see | honest refusal rather than false precision |

Track:
- capture success rate
- reference-detection success rate
- measurement error
- classification result
- quality/refusal reason
- whether retry guidance led to a successful second capture

## Scope lock

### In scope

- bolts / screws / nuts / washers as the initial object family
- single-view measurement using an in-frame scale reference
- robust-enough segmentation for ordinary indoor bench conditions
- simple nominal-size matching
- uncertainty/provenance
- capture-quality checks and one follow-up observation recommendation
- local storage
- physical trigger; controlled light if available

### Explicitly deferred

- precision camera calibration as a project unto itself
- sub-millimeter metrology claims
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

If a feature does not directly improve the physical end-to-end demo, varied-condition robustness, validation, or reusable PlatypusOne perception stack, it waits until after September 30.

## Reuse into Autodesk build

The DigiKey work must land behind reusable PlatypusOS boundaries:

- `platform`: camera + MCU/illumination/trigger hardware interfaces
- `services/vision`: reference detection, segmentation, feature extraction, capture-quality checks
- `services/ai`: classification/inference interface
- engineering observation model: common evidence contract
- project/filesystem service: saved record + artifacts
- `apps/engineering_scout`: contest-facing workflow/UI

After DigiKey, these same components feed PlatypusOne measurement, inspection, documentation, and ShadowScan work. The canonical Engineering Observation remains CAD-neutral; the Autodesk build adds the bounded Fusion adapter defined in [CAD Handoff Architecture](../architecture/CAD_HANDOFF.md). No contest-only rewrite should be required.

## Build order — execution mode

1. **Real UNO Q camera capture.**
2. **Run the existing analyzer on a real fastener + reference image.**
3. **Make the full record/UI/save loop work once end-to-end.**
4. **Repeat under 3–4 ordinary lighting conditions and log failures.**
5. Fix only the failure modes that block the workflow most often.
6. Add physical trigger.
7. Add controlled illumination only if it materially improves repeatability.
8. Add capture-quality/refusal guidance for remaining weak conditions.
9. Freeze functionality.
10. Spend remaining time on real test data, photos/video, reliability, and documentation.

Do not delay step 3 while chasing better calibration mathematics.

## Definition of done

A judge can watch one uninterrupted run and understand:

**physical part → capture → evidence → useful measurement → inference → uncertainty → saved engineering record**

Then the demo repeats under a visibly less-than-perfect lighting condition and still either:
- produces a useful result, or
- refuses cleanly and tells the user what observation to improve.

The result must feel like the first function of a real engineering instrument, not an object-detection demo and not a laboratory calibration exercise.
