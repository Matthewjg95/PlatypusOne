# Camera evidence intake → Perception Head

The camera bench-off itself lives in #40 and its comparison table in
`docs/hardware/CSI_CAMERA_BENCH.md` (PR #42, tests T1–T10). **Do not duplicate
results here.** This page lists only what the head PCB consumes, so a result
can be dropped in and immediately unblock a design item. Fill a cell only with
a link to preserved evidence (image/JSON/photo/commit); otherwise leave `—`.

**Producer (2026-10-06):** `tools/camera_characterize` turns the #40 capture
matrix into `derived/<version>/report.md` + `decision.json`
([CAMERA_CHARACTERIZATION.md](../../docs/hardware/CAMERA_CHARACTERIZATION.md)).
Its per-camera **"PCB / Fusion handoff"** block carries the table B fields
(measured envelope, holes, optical-centre offset, connector side, cable),
the working-distance envelope, focus-access need and calibration stability
after remove/reinstall. Copy values here only from a physical-capture report
(never from the synthetic CI report) and link the report + its analysis
version. Role outputs map to table A: PROPOSED → value; CANDIDATES /
INSUFFICIENT EVIDENCE → leave `—`.

Additional head-relevant fields the report provides:

| Field | Report source | Feeds |
|---|---|---|
| Focus access (ring reachable / sealed / none / software-recorded) | handoff block, `focus_type` + `bringup.focus_control` | window/bezel, service cover |
| Calibration survives remove/reinstall (focal Δ between sets A/B) | `calibration_focal_rel_delta` | datum-hole scheme (Rev A goal 3) |
| Working-distance envelope (distances passing the edge-rise rule) | `working_envelope_distances` | baseline P2, illumination L2 |
| B0393 focus control on UNO Q | `focus_control_exposed` / `bringup.focus_control` (PR #42: no lens subdev) | whether AF is a product option at all |

## A. Decisions the head needs

| Output | Value | Evidence link | Unblocks |
|---|---|---|---|
| Reference / measurement camera | — | — | H4–H7 pattern, optical centre (M1) |
| Product candidate | — | — | Fusion head envelope |
| Fallback | — | — | — |
| Preferred working distance (mm) | — | — | baseline P2, illumination L2 |
| Focus behaviour accepted (fixed / locked manual / AF with recorded state) | — | — | calibration record fields |

## B. Physical data per module (MEASURE, calipers + photo over grid)

| Field | B0394 | B0393 | B0390 | Feeds |
|---|---|---|---|---|
| Board W × H × t (mm) | — | — | — | outline |
| Mounting hole centres (mm, from board corner) + Ø | — | — | — | H4–H7 footprint positions |
| Lens/holder height above board (mm) | — | — | — | front height limit, window gap |
| Lens outer Ø / holder footprint (mm) | — | — | — | keep-out |
| Optical centre offset from board centre (mm) | — | — | — | baseline, extrinsic seed |
| FFC connector side + cable type (15/22-pin) | — | — | — | P9 bend volume |
| Required clear aperture at window (from FOV) | — | — | — | window/bezel |

## C. Electrical / behavioural data (from #40 tests)

| Field | B0394 | B0393 | B0390 | Feeds |
|---|---|---|---|---|
| Enumerates + streams on Media Carrier (T1/T2) | link | link | link | keeps "camera on Media Carrier" decision |
| DSI + touch coexistence (T4) | link | link | link | ICD §1 |
| Current: preview / capture (T10) | — | — | — | power tree §3 |
| Calibration residual (T8) and repeatability after refocus/power cycle | — | — | — | calibration record design |
| Low-light behaviour (T9) | — | — | — | illumination need (L1/L2) |
| Glare with a bench LED at 2–3 offsets (new, optional) | — | — | — | light-board geometry (L3) |

## D. How results flow

1. Matthew fills #40 / CSI_CAMERA_BENCH.md with evidence.
2. Copy the PCB-relevant numbers into tables B/C with links.
3. Update [DECISION_REGISTER.md](DECISION_REGISTER.md) rows M1/M2 (and L2/L3 if illumination data exists).
4. Fusion places the camera + ToF optical centres ([MECHANICAL_FUSION_INPUTS.md](MECHANICAL_FUSION_INPUTS.md) §4).
5. Move H4–H7 in layout; nothing in the schematic changes for any of the three modules.
