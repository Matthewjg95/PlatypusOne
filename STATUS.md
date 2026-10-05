# Project Status

Rolling snapshot of where Platypus One actually is. Updated as work lands —
if this file disagrees with the code, the code wins and this file is stale.

**Last updated: 2026-10-05**

## Right now

| | |
|---|---|
| **Autodesk hardware request** | **SELECTED — 2026-09-25** |
| **Dream Lab** | **SUBMITTED — 2026-09-30**, confirmed by Matthew on October 1. Final public post/form evidence is not yet archived here. |
| **Mode** | **EXECUTION MODE** — shortest path to a demonstrable PlatypusOne core prototype |
| **Critical path** | Preserve/replay the Scout baseline (#32) → minimum viable enclosure/packaging → Rev A architecture lock → early PCB/fab orders → physical measurement validation |
| **Hardware in hand** | Arduino UNO Q + UNO Media Carrier + Waveshare 5inch DSI LCD Rev2.2 (800×480) + USB webcam + printed 20 mm reference sheets. M5Stack Tab5 remains a fallback/dev fixture. |
| **Software state** | The single-capture Scout kiosk is the stabilization baseline. Keep host/synthetic checks, reported physical runs, and archived physical evidence distinct; see [post-submission stabilization](docs/contest/POST_DREAMLAB_STABILIZATION.md). |
| **Shared Autodesk + Dream Lab slice** | camera → capture → useful measurement → observation record → UI → saved artifact |
| **Display** | UNO Media Carrier + Waveshare 5inch DSI LCD Rev2.2 (800×480, 15-pin FPC via 15→22 adapter). Panel/touch detection and the on-glass kiosk run are recorded in the bench notes; #26 and #27 are merged. Final run logs/captures and the exact tested SHA still need archiving. |

## Execution rules

1. Finish the minimum viable enclosure/internal packaging model.
2. Lock Rev A hardware architecture, carrier/interconnect strategy, power, display, camera, controls, and sensor interfaces.
3. Release any PCB/fab items early enough that lead time cannot block December.
4. Build one complete physical engineering measurement workflow.
5. Generate test data, photos/video, and documentation while building.
6. Keep stretch features out of the critical path.
7. Carry the submitted Dream Lab proof into Autodesk development through a reproducible, evidence-backed baseline.

**Scope test:** if a task does not move the first physical `Point → Measure → Display → Save` loop closer, it needs a strong reason to enter the critical path.

## Dream Lab correction — calibration is not the product

Engineering Scout must prove a useful engineering workflow under ordinary imperfect bench conditions.

The in-frame reference is a **scale anchor**, not a reason to turn the project into a precision camera-calibration exercise. Controlled illumination can improve repeatability but is not required for the first successful workflow.

Near-term physical validation must include:
- even desk lighting
- uneven overhead lighting
- moderate shadow
- brighter/darker exposure
- rotated part placement

The desired behavior is:
- useful result when evidence is sufficient;
- honest refusal + actionable retry guidance when it is not.

See [Dream Lab MVP](docs/contest/DIGIKEY_ENGINEERING_SCOUT_MVP.md).

## Current architecture focus

Rev A core only:
- Arduino UNO Q
- display/UI
- RGB camera
- ToF/depth interface
- trigger + rotary encoder
- power/battery
- simple carrier/interconnect
- controlled illumination if it earns its place through testing

Rev A PCB workstream (#33): Perception Head Rev A pre-layout schematic in KiCad 9 (ERC 0/0) with ICD, power tree, decision register and bring-up plan in [`hardware/pcb/`](hardware/pcb/README.md). **No layout or fabrication** until the camera (#40), ToF (Platypus Lab #3), power and Fusion-datum gates release.

Deferred from critical path:
- radar
- thermal
- color sensing
- generalized AI/object recognition
- full ShadowScan/3D reconstruction
- automatic CAD generation
- energy harvesting
- production-polish features

## Immediate next action

**Preserve the submission-night captures, JSON, conditions, caliper truth, media, and actual tested SHA, then replay the pinned baseline on the UNO Q.**

Use [the evidence index](docs/contest/evidence/dreamlab-2026-09-30/README.md)
before changing the board checkout, and [the UNO Q sequence](docs/hardware/UNO_Q_SCOUT_BASELINE.md)
for build, checks, kiosk launch, and desktop recovery. The source pin is a
post-submission reproduction candidate, not a claim about the final contest-night SHA.

## Existing software foundation

- M0 Foundation — complete.
- M1 Board bring-up — partial; physical UNO Q verification is the current priority.
- M2 Runtime maturity — complete host-side.
- Engineering Observation contract — merged.
- Engineering Scout host capture/analyzer/classifier/result UI — merged.
- 21-case synthetic validation battery — merged and useful as regression coverage.
- Webcam capture on UNO Q — already recorded at 640×480 YUYV; preserve the raw frame and JSON when replaying it.
- UNO Media Carrier + Waveshare 5inch DSI LCD Rev2.2 — hardware in hand; PR #26 records the successful FFC-orientation fix, ATTINY response at `0x45`, backlight, and touch at `0x38`.
- Touch input is the accepted first-pass trigger; dedicated MCU/physical trigger remains deferred until the local-display loop is stable.
- Printed reference sheets — ready for physical testing.
- Real touch → capture → result/save loop — reported on glass in the September 29 notes. The 1/4-20 UNC test screw over-read at first light; this is workflow evidence, not validated measurement accuracy.
- Final Dream Lab raw capture archive / tested commit / submitted post URL — missing from accessible repository evidence.
- Linked-display / Tab5 path — fallback development fixture, not final product dependency.

## Evidence discipline

From this point forward, every physical session should leave durable evidence:
- source images
- observation JSON
- test condition / lighting note
- pass/fail or measured error
- photo/video of setup when useful
- blocker and next action

The repo should tell the build story as it happens, not reconstruct it at the end.


## Merged baseline and staged work (2026-10-01)

Do not mistake open-PR capability for merged baseline:

- Merged: #25 clipped-blob refusal; #26 correct DSI hardware/bring-up; #27 Linux/KMS display; #28 single-capture kiosk; #30 geometry header fix; #31 initial write-up/readiness draft.
- PR #29 (draft) — sessions, repeat statistics and outline/DXF export. Reconciled with main; fresh Windows/Linux CI and synthetic finish/export pass. A preserved physical finish/export run is still required before merge.
- PR #35 — positive-evidence fastener classification and associated analyzer changes, stacked on #29; currently conflicts with that updated base and needs separate reconciliation/fresh checks. Keep its 24-case battery and UNC/metric candidate behavior separate from the baseline's 21-case battery and metric classifier.
- The newer write-up on the already-merged #31 branch was never merged into main. Its exact [branch snapshot](docs/contest/evidence/dreamlab-2026-09-30/WRITEUP_BRANCH_SNAPSHOT.md) is retained as reported evidence, not final submitted copy.

#32 remains open for final evidence and native replay; deadline issues #9 and
#23 are superseded, with unfinished checks carried into the stabilization plan.
No new measurement algorithm tuning is part of this package.
