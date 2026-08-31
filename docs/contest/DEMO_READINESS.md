# Demo readiness — every contest, what its demo needs, where it stands

Working map across the sub-projects and their contests. Update as work lands;
STATUS.md stays the single source for overall project state — this document
answers one question per contest: **what does the demo still need?**

Last reviewed: 2026-08-31.

## 1. AU 2027 product contest (Hackster × Autodesk) — the flagship

| | |
|---|---|
| Application deadline | **Sep 7, 2026** (target submit Sep 4) |
| Recipients announced | Sep 25 (verified) |
| Submission | Dec 20, 2026 |

**Demo = the Hackster page + application now; the full device by Dec 20.**

Still needed for the application (all owner-only):
- Fusion `.f3d` first pass — the critical path
- Hackster account + project page (copy drafted in
  [HARDWARE_APPLICATION_CHECKLIST.md](HARDWARE_APPLICATION_CHECKLIST.md) §3–5)
- Original cover image

Software story available today for the page: host-sim launcher with a real
bitmap font, the Engineering Scout evidence card (render via
`ui_preview --scout-card-demo`), the capture → observation pipeline, CI-gated
tests, and the committed [validation report](VALIDATION_REPORT.md).

For Dec 20: hardware bring-up after Sep 25 per
[ACQUISITION_ROADMAP.md](../hardware/ACQUISITION_ROADMAP.md); DSI panel
proof-of-life gate Sep 30 ([candidates](../hardware/DSI_PANEL_CANDIDATES.md)).

## 2. DigiKey Dream Lab — Engineering Scout Q (own contest, Sep 30)

**Demo = fastener beside the calibration square → trigger → saved evidence
record with calibrated measurement, classification, uncertainty, guidance.**

MVP build order ([source](DIGIKEY_ENGINEERING_SCOUT_MVP.md)):

| Step | State |
|---|---|
| 1. Observation contract + serializer | ✅ merged (PR #10) |
| 2. Host-side pipeline on stored images | ✅ capture slice (PR #11) + analyzer |
| 3. Calibration + deterministic measurement | ✅ `vision.scout_analyzer.v2` (min-support width) |
| 4. Classification / nominal matching | ✅ `ai.fastener_classifier.v2` |
| 5. Scout result UI (evidence classes) | ✅ single-card `apps/engineering_scout` |
| 6. UNO Q camera capture | code written (`V4l2Camera`); **needs the board** |
| 7. MCU trigger / illumination | **open** — smallest software start: STM32 mcu-bridge v1 Ping/Pong |
| 8. Validation set + failures | ✅ 21-case battery, CI-gated, [report](VALIDATION_REPORT.md) |
| 9. Active guidance | partial — recommendations emitted + shown on the card |
| 10. Docs / video / reliability freeze | not started (needs hardware first) |

Hardware-blocked items (6, 7, camera/lighting rig) are the entire remaining
risk. Everything else can only polish.

## 3. element14 RoadTest — PlatypusVision

Application submitted 2026-08-30
([record](ELEMENT14_PLATYPUSVISION_ROADTEST_APPLICATION.md)). Nothing to do
until element14 responds; if selected, the RoadTest review is its own
deliverable with its own timeline.

## 4. Renesas Robotics Design Contest — PlatypusTail (separate repo)

Robotic-joint characterization platform on the FPB-RA6T3, staged in
`Matthewjg95/PlatypusTail` as the diagnostics layer for PlatypusOne's future
actuation. Its demo needs live in that repository — tracked here only so the
contest calendar is complete.

## 5. Deferred extensions (documented, no demo commitments)

- [24 GHz radar](../roadmap/24GHZ_RADAR_EXTENSION.md) — deferred by design.
- [Energy harvesting subsystem](../hardware/ENERGY_HARVESTING_SUBSYSTEM.md) —
  architecture note only.

## Shared infrastructure both active demos ride on

- Host CI (build + tests + format) on every push/PR.
- `tools/ui_preview` — offline PNG-able renders of any UI surface.
- `tools/scout_validation` — deterministic quality gate; regenerate the
  committed report when the pipeline changes.
- `hal::testing` fakes (camera, scenes) — every pipeline stage runs
  hardware-free.
