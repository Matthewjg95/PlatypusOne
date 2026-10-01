# Demo readiness — every contest, what its demo needs, where it stands

Working map across the sub-projects and their contests. Update as work lands;
STATUS.md stays the single source for overall project state — this document
answers one question per contest: **what does the demo still need?**

Last reviewed: **2026-10-01**.

## 1. AU 2027 product contest (Hackster × Autodesk) — the flagship

| | |
|---|---|
| Hardware request | **SELECTED — 2026-09-25** |
| Submission | Dec 20, 2026 |

**Execution mode is active.** Selection removes the application gate; the critical path is now a credible physical core prototype.

Shortest-path prototype:
- minimum viable enclosure/internal packaging
- Rev A architecture lock: compute, display, camera, ToF, controls, power, carrier/interconnect
- early PCB/fab release where lead time matters
- one complete physical engineering measurement workflow
- real test data + photos/video captured during development

Optional/stretch capabilities stay out of the critical path.

## 2. DigiKey Dream Lab — Engineering Scout Q

**Submitted September 30, 2026** (Matthew's confirmation, October 1).
The deadline sprint is over. Remaining checks belong to [#32 stabilization](POST_DREAMLAB_STABILIZATION.md), not a new submission sprint.

**Demo = real fastener + in-frame scale reference → trigger/capture → useful measurement + classification + uncertainty → saved evidence record.**

The Dream Lab build now explicitly optimizes for **workflow robustness, not calibration perfection**.

| Step | State |
|---|---|
| Observation contract + serializer | ✅ merged |
| Host-side capture/analyzer/classifier | ✅ |
| Scout result UI | ✅ |
| Synthetic validation battery | ✅ baseline 21/21; #35's 24-case battery is staged |
| UNO Q camera capture | ✅ USB webcam, 640×480 YUYV (2026-09-28) |
| DSI panel + touch on the Media Carrier | ✅ 2026-09-29 (cable orientation, see `docs/hardware/DSI_BRINGUP.md`) |
| Real part → record → UI/save loop | reported on glass, touch-triggered; #28 baseline merged; final raw records/tested SHA pending archive |
| Sessions: repeat captures → CAD bundle | host-verified (PR #29); on-board run pending |
| Varied-lighting physical validation | **open**; September 29 first light and September 30 focused/tilted comparisons are recorded in the write-up, but no raw captures or complete lighting matrix are archived |
| MCU physical trigger | open (bridge written, not on the board) |
| Controlled illumination | open |
| Documentation/video | original draft plus [later branch snapshot and evidence gaps](evidence/dreamlab-2026-09-30/README.md); final published URL/video reference still needed |

**Submission deadline (historical):** 11:59 p.m. CT, 2026-09-30.
Submission completion does not establish physical accuracy, completion of the
session/export workflow, or that a repository draft matches the posted text.

Acceptance is ordinary bench reality: uneven lighting, moderate shadows, exposure differences, and rotated parts. Severe glare/darkness/arbitrary clutter are outside MVP scope.

The system should degrade honestly: produce a useful result when evidence is sufficient; otherwise refuse and recommend the next observation.

See [Engineering Scout Q MVP](DIGIKEY_ENGINEERING_SCOUT_MVP.md).

## 3. Shared Autodesk × Dream Lab execution path

The highest-value shared vertical slice is:

**camera → physical capture → reference/subject extraction → measurement → observation record → result UI → saved artifact**

Work on that slice strengthens both submissions.

Avoid pulling these onto the shared critical path:
- radar
- thermal
- generalized object recognition
- full 3D reconstruction
- automatic CAD generation
- production-polish enclosure work
- precision metrology calibration work beyond what the physical demo proves necessary

## 4. element14 RoadTest — PlatypusVision

Application submitted 2026-08-30 (record kept in the private planning
overlay). Nothing to do until element14 responds; if selected, the RoadTest
review is its own deliverable with its own timeline.

## 5. Renesas Robotics Design Contest — PlatypusTail (separate repo)

Tracked separately. Do not let it consume PlatypusOne / Dream Lab critical-path time.

## Shared infrastructure

- Host CI
- `tools/ui_preview`
- `tools/scout_validation`
- `hal::testing` fakes
- Engineering Observation evidence contract
- camera/HAL seams reusable on real hardware
