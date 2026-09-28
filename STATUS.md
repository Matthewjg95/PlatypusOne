# Project Status

Rolling snapshot of where Platypus One actually is. Updated as work lands —
if this file disagrees with the code, the code wins and this file is stale.

**Last updated: 2026-09-25**

## Right now

| | |
|---|---|
| **Autodesk hardware request** | **SELECTED — 2026-09-25** |
| **Mode** | **EXECUTION MODE** — shortest path to a demonstrable PlatypusOne core prototype |
| **Critical path** | Minimum viable enclosure/packaging → Rev A architecture lock → early PCB/fab orders → one physical end-to-end measurement workflow |
| **Hardware in hand** | Arduino UNO Q + UNO Media Carrier + Waveshare 5-DSI-TOUCH-A + USB webcam + printed 20 mm reference sheets. M5Stack Tab5 remains a fallback/dev fixture. |
| **Software state** | Host architecture, observation contract, Scout analyzer/classifier/UI and validation are substantially ahead of physical bring-up; the priority is now hardware proof, not more host-side feature breadth |
| **Shared Autodesk + Dream Lab slice** | camera → capture → useful measurement → observation record → UI → saved artifact |
| **Display** | Rev A display candidate is physically in hand: UNO Media Carrier + Waveshare 5-DSI-TOUCH-A. Tonight's gate is DSI/touch proof-of-life and Scout UI on this panel. |

## Execution rules

1. Finish the minimum viable enclosure/internal packaging model.
2. Lock Rev A hardware architecture, carrier/interconnect strategy, power, display, camera, controls, and sensor interfaces.
3. Release any PCB/fab items early enough that lead time cannot block December.
4. Build one complete physical engineering measurement workflow.
5. Generate test data, photos/video, and documentation while building.
6. Keep stretch features out of the critical path.
7. Prefer work that strengthens both Autodesk and DigiKey Dream Lab.

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

**Tonight: prove the complete physical loop on the UNO Q with the Media Carrier + Waveshare display + webcam + printed reference: touch trigger → capture → measure/classify → render result → save evidence.**

Do not improve calibration further until that end-to-end physical attempt has produced failure data.

## Existing software foundation

- M0 Foundation — complete.
- M1 Board bring-up — partial; physical UNO Q verification is the current priority.
- M2 Runtime maturity — complete host-side.
- Engineering Observation contract — merged.
- Engineering Scout host capture/analyzer/classifier/result UI — merged.
- 21-case synthetic validation battery — merged and useful as regression coverage.
- Webcam capture on UNO Q — already proven at 640×480 YUYV; reuse that path tonight.
- UNO Media Carrier + Waveshare 5-DSI-TOUCH-A — hardware in hand; proof-of-life still required.
- Touch input is the accepted first-pass trigger; MCU/physical trigger is deferred until after the end-to-end loop works.
- Printed reference sheets — ready for physical testing.
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
