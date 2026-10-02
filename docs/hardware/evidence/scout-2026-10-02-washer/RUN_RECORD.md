# Scout run record — flat washer, 2026-10-02

One real capture of a flat steel washer on the UNO Q kiosk, preserved because
it exposed a pipeline defect: the record asked fastener-thread questions of a
washer and sized it with the wrong standard. This is the first physical
fixture for that fix and for issue #37's circle primitives. It is **not** an
accuracy acceptance test.

Follows [SCOUT_BASELINE_RUN_RECORD.md](../../SCOUT_BASELINE_RUN_RECORD.md).
`UNRESOLVED` means not recorded, not a pass.

| Field | Recorded value |
|---|---|
| Session date/time/timezone and operator | 2026-10-02 17:43:58 UTC (record `timestamp_utc`); Matthew operating; kiosk session `sess-0009` |
| Exact source SHA built/tested, plus local changes | Kiosk binary built on the board from branch `claude/no-fastener-assumption` at **72cd885** (`feat(scout): say when the camera is tilted`), built 2026-10-01 ~03:20 UTC. Consistent with the record: it carries `camera_tilt` and `reference_keystone`, which first appear in 72cd885. Board HEAD at capture time **not re-verified** (board offline when this was archived). |
| Kiosk binary SHA256 / build log | UNRESOLVED |
| Kernel / compiler / panel driver version | Arduino kernel 7.0.0; GCC 14.2; `dcuartielles/uno_q_dsi_displays` waveshare-800x480 profile |
| Display connector/mode and touch device | UNO Media Carrier DSI, Waveshare 5inch DSI LCD Rev2.2, 800×480; ft5x06 touch |
| Webcam identity/node/mode | Adesso CyberTrack H4, `/dev/video10`, 640×480 YUYV (record `source`) |
| Reference dimensions / print-scale verification | Printed 20 mm square. Print scale not caliper-verified in this record (UNRESOLVED) |
| Object identity and verification method | Flat steel washer. Calipers, Matthew, 2026-10-02: **outer diameter 19.12 mm, bore 8.71 mm**. Thickness not measured. Standard/designation unknown |
| Lighting / focus / camera pose / part placement | Indoor room light, no controlled illumination; lens focus ring set by hand (2026-09-30); frame sharpness (Laplacian variance) 49.9. Camera tilt from the square **8.82°** (record), below the 12° on-screen warning. Washer flat on white paper left of the square |
| Native build / asserts / synthetic battery | Built natively on the board (GCC 14.2, zero warnings); host suite passed at 1a63ece. Board-side test run for this binary UNRESOLVED |
| Live preview → touch → real capture → card → saved pair | **Yes**: touch capture in `sess-0009`, card shown, `scan-0053/{observation.json, source.yuyv}` saved (copied here byte-for-byte) |
| Intentional refusal / guidance / saved raw pair | Not part of this record |
| Photo/video references | Matthew viewed the on-glass card (2026-10-02); no photo archived here |
| Sessions FINISH/export exercised? | No FINISH for this capture recorded |
| Blocker and single next action | Defect below; next action: implement (a) and re-capture this washer |

## Capture

| Capture ID + paths | Condition | Measurand | Caliper truth (mm) | On-device result | Error (mm) | Class/nominal + unresolved |
|---|---|---|---|---|---|---|
| `scan-0053/observation.json`, `scan-0053/source.yuyv` | indoor light, 8.8° tilt, hand-focused | outer diameter (as subject length / width extents) | 19.12 | 18.78 / 18.10 | −0.34 / −1.02 | `nut_or_washer` 0.75; nominal **M12** 0.90; unresolved `thread_pitch`, `nut_vs_washer` |
| same | same | bore diameter | 8.71 | not reported | — | — |

The extents are not diameters: `subject_length` and `subject_width` are
rotating-caliper support widths of the silhouette, so a disc yields two
readings of its outer diameter.

## Defect this record exposes

1. **Fastener-thread questions for a washer.** The analyzer writes
   `thread_pitch` and "capture a side-on view so the thread profile is
   visible" into every record before classification; the classifier only
   removes them for out-of-library parts. A washer has no thread; a nut's is
   internal and invisible from the side.
2. **Wrong sizing basis.** `nominal_size = M12` at 0.90 came from the ISO 4032
   hex across-flats table applied to the outer diameter. A washer's size is
   set by its bore (the bolt it fits). The bore is measured (the outline's
   hole) but not reported or used.
3. **Nut vs washer left open although the top view separates them.** A round
   outer edge versus a hex one is visible from above; only thickness needs a
   side view.

## Offline re-analysis (separate, not on-device)

`offline/washer_circle_fit.py` (self-contained, numpy) on the preserved raw
frame, on-device mm/px unchanged. Output: `offline/washer_circle_fit.out.txt`.

| | Offline circle fit | Calipers | Error |
|---|---|---|---|
| Outer diameter | 18.61 mm (edge rms 0.27 mm) | 19.12 | −0.51 |
| Bore diameter | 8.58 mm (edge rms 0.21 mm) | 8.71 | −0.13 |
| Bore offset from outer centre | 0.15 mm | — | — |
| 6-fold harmonic of the outer edge | 0.43 % of radius (a regular hexagon ≈ 7 %) | — | round, not hex |

A square-corner (homography) correction tried during the same session gave
17.92 / 8.25 mm, worse than the area scale at this small tilt; that quick
corner extraction is not robust and is **not** adopted or archived as a method.

Do not tune thresholds from this single capture (AGENTS.md rule 6). It is a
regression fixture: the fix must make the record stop asking thread questions,
size from the bore with candidates, and separate round from hex.
