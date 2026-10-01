# Project Status

Rolling snapshot of where Platypus One actually is. Updated as work lands —
if this file disagrees with the code, the code wins and this file is stale.

**Last updated: 2026-09-30**

## Right now

| | |
|---|---|
| **Autodesk hardware request** | **SELECTED — 2026-09-25** |
| **Mode** | **EXECUTION MODE** — shortest path to a demonstrable PlatypusOne core prototype |
| **Critical path** | Minimum viable enclosure/packaging → Rev A architecture lock → early PCB/fab orders → one physical end-to-end measurement workflow |
| **Hardware in hand** | Arduino UNO Q + UNO Media Carrier + Waveshare 5inch DSI LCD Rev2.2 (800×480) + USB webcam + printed 20 mm reference sheets. M5Stack Tab5 remains a fallback/dev fixture. |
| **Software state** | Host architecture, observation contract, Scout analyzer/classifier/UI and validation are substantially ahead of physical bring-up; the priority is now hardware proof, not more host-side feature breadth |
| **Shared Autodesk + Dream Lab slice** | camera → capture → useful measurement → observation record → UI → saved artifact |
| **Display** | Physical display path is now identified correctly: UNO Media Carrier + Waveshare 5inch DSI LCD Rev2.2 (800×480, 15-pin FPC via 15→22 adapter). PR #26 records successful panel/touch detection after correcting the opposite-sided FFC orientation; local KMS display support is staged in PR #27. |

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

**Finish and preserve the Dream Lab evidence, then stabilize the proven physical loop before expanding scope.**

The current PR stack records the transition from bring-up to a local-display Scout kiosk and session/export work. Treat those capabilities as staged until their PRs merge; do not rewrite main's architecture around unmerged code.

## Existing software foundation

- M0 Foundation — complete.
- M1 Board bring-up — partial; physical UNO Q verification is the current priority.
- M2 Runtime maturity — complete host-side.
- Engineering Observation contract — merged.
- Engineering Scout host capture/analyzer/classifier/result UI — merged.
- 21-case synthetic validation battery — merged and useful as regression coverage.
- Webcam capture on UNO Q — already proven at 640×480 YUYV; reuse that path tonight.
- UNO Media Carrier + Waveshare 5inch DSI LCD Rev2.2 — hardware in hand; PR #26 records the successful FFC-orientation fix, ATTINY response at `0x45`, backlight, and touch at `0x38`.
- Touch input is the accepted first-pass trigger; dedicated MCU/physical trigger remains deferred until the local-display loop is stable.
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


## Staged work not yet on main (2026-09-30)

Do not mistake open-PR capability for merged baseline:

- PR #27 — local Linux/KMS `DrmDisplay` backend and display probe.
- PR #28 — Scout kiosk composition: live preview → touch/button capture → evidence card → saved record.
- PR #29 — multi-capture sessions, guidance, repeat statistics, and outline/DXF export.
- PR #30 — geometry header self-containment/formatting fix required by the stacked session work.
- PR #31 — Dream Lab Maker.io draft/readiness documentation.

Before the next architecture expansion, stabilize/merge the dependency stack in order and preserve the physical evidence that justified it.
