# Camera capture matrix — IMX219 bench-off (#40)

Generated from [`tools/camera_characterize/matrix_spec.json`](../../tools/camera_characterize/matrix_spec.json)
(`camera_characterize.py plan`). Edit the spec, not this table, then regenerate.

**Distances (150 / 250 / 400 / 600 mm) are bench study points, not product
working distances.** The representative distance (250 mm) carries calibration,
repeatability, lighting and clutter cells.

## Operator rules (every camera)

1. Power off before connecting or swapping a module (CSI is not hot-pluggable).
2. Bring-up first: `tools/csi_bench/csi_bench.sh <SKU> all "notes"` (PR #42).
   Record enumeration, reboot repeat, cold power cycle and DSI/touch
   coexistence in `dataset.json → bringup` with the run directories as evidence.
3. `camera_characterize.py init datasets/<sku>-<date> --camera <SKU> --dataset-id <sku>-<date>`,
   then fill `camera` (silkscreen photo!), `platform` (repo SHA, `uname -a`,
   image, CSI port, enumeration), `physical` (calipers; `status: measured`)
   and the exposure/gain/focus defaults. Anything not known stays `UNKNOWN`.
4. Fixed exposure and gain for the whole dataset unless a cell says otherwise;
   record any change on the frame.
5. Focus: set once, record it, never touch it during the dataset
   (B0394 ring locked; B0390 fixed; B0393 `not_exposed` unless a lens control
   exists, then record its position on every frame).
6. `camera_characterize.py capture datasets/<dir> --command 'tools/camera_characterize/capture_raw.sh {out}'`
   walks the cells below. Failed or bad frames are discarded *with a reason*,
   never deleted.
7. Calibration set B comes after removing and re-installing the camera in its
   fixture with focus untouched: that is the stability test.
8. Measure and verify the printed targets (ChArUco square pitch over 6 squares,
   20 mm reference square, part with calipers) and set `status: verified` +
   `source`. Until then the analysis reports shape, never absolute accuracy.

Issue #40 IMX219 bench-off capture matrix. Distances are bench study points, not final product working distances.

## B0394 — 74 frames

Before the first frame: Set the M12 focus at the representative distance on the 20 mm square, lock it, photograph the ring. Record focus_mode=manual_locked and the ring position description. Do not touch focus until the dataset is finished.

| Cell | Distance (mm) | Scene | Placement | Lighting | Count |
|---|---|---|---|---|---|
| d150-ref-center | 150 | dimensional_reference | center | even | 2 |
| d150-ref-edge | 150 | dimensional_reference | edge | even | 2 |
| d150-charuco-center | 150 | geometry | center | even | 1 |
| d150-charuco-edge | 150 | geometry | edge | even | 1 |
| d250-ref-center | 250 | dimensional_reference | center | even | 2 |
| d250-ref-edge | 250 | dimensional_reference | edge | even | 2 |
| d250-charuco-center | 250 | geometry | center | even | 1 |
| d250-charuco-edge | 250 | geometry | edge | even | 1 |
| d400-ref-center | 400 | dimensional_reference | center | even | 2 |
| d400-ref-edge | 400 | dimensional_reference | edge | even | 2 |
| d400-charuco-center | 400 | geometry | center | even | 1 |
| d400-charuco-edge | 400 | geometry | edge | even | 1 |
| d600-ref-center | 600 | dimensional_reference | center | even | 2 |
| d600-ref-edge | 600 | dimensional_reference | edge | even | 2 |
| d600-charuco-center | 600 | geometry | center | even | 1 |
| d600-charuco-edge | 600 | geometry | edge | even | 1 |
| d250-calib-A | 250 | calibration | varied | even | 15 |
| d250-repeat | 250 | repeatability | center | even | 10 |
| d250-rotated | 250 | planar_part | rotated | even | 2 |
| d250-light-hard-shadow | 250 | lighting | center | hard_shadow | 2 |
| d250-light-low | 250 | lighting | center | low | 2 |
| d250-light-bright-overhead | 250 | lighting | center | bright_overhead | 2 |
| d250-clutter | 250 | clutter | center | even | 2 |
| d250-calib-B | 250 | calibration | varied | even | 15 |

## B0390 — 74 frames

Before the first frame: Fixed focus: record focus_mode=fixed.

| Cell | Distance (mm) | Scene | Placement | Lighting | Count |
|---|---|---|---|---|---|
| d150-ref-center | 150 | dimensional_reference | center | even | 2 |
| d150-ref-edge | 150 | dimensional_reference | edge | even | 2 |
| d150-charuco-center | 150 | geometry | center | even | 1 |
| d150-charuco-edge | 150 | geometry | edge | even | 1 |
| d250-ref-center | 250 | dimensional_reference | center | even | 2 |
| d250-ref-edge | 250 | dimensional_reference | edge | even | 2 |
| d250-charuco-center | 250 | geometry | center | even | 1 |
| d250-charuco-edge | 250 | geometry | edge | even | 1 |
| d400-ref-center | 400 | dimensional_reference | center | even | 2 |
| d400-ref-edge | 400 | dimensional_reference | edge | even | 2 |
| d400-charuco-center | 400 | geometry | center | even | 1 |
| d400-charuco-edge | 400 | geometry | edge | even | 1 |
| d600-ref-center | 600 | dimensional_reference | center | even | 2 |
| d600-ref-edge | 600 | dimensional_reference | edge | even | 2 |
| d600-charuco-center | 600 | geometry | center | even | 1 |
| d600-charuco-edge | 600 | geometry | edge | even | 1 |
| d250-calib-A | 250 | calibration | varied | even | 15 |
| d250-repeat | 250 | repeatability | center | even | 10 |
| d250-rotated | 250 | planar_part | rotated | even | 2 |
| d250-light-hard-shadow | 250 | lighting | center | hard_shadow | 2 |
| d250-light-low | 250 | lighting | center | low | 2 |
| d250-light-bright-overhead | 250 | lighting | center | bright_overhead | 2 |
| d250-clutter | 250 | clutter | center | even | 2 |
| d250-calib-B | 250 | calibration | varied | even | 15 |

## B0393 — 79 frames

Before the first frame: First run tools/csi_bench/csi_bench.sh B0393 probe and record whether a lens/VCM subdevice exists. If none: focus_mode=not_exposed for every frame (compatibility result). If one exists: record its focus position control value on every quantitative frame (motorized_position).

| Cell | Distance (mm) | Scene | Placement | Lighting | Count |
|---|---|---|---|---|---|
| d150-ref-center | 150 | dimensional_reference | center | even | 2 |
| d150-ref-edge | 150 | dimensional_reference | edge | even | 2 |
| d150-charuco-center | 150 | geometry | center | even | 1 |
| d150-charuco-edge | 150 | geometry | edge | even | 1 |
| d250-ref-center | 250 | dimensional_reference | center | even | 2 |
| d250-ref-edge | 250 | dimensional_reference | edge | even | 2 |
| d250-charuco-center | 250 | geometry | center | even | 1 |
| d250-charuco-edge | 250 | geometry | edge | even | 1 |
| d400-ref-center | 400 | dimensional_reference | center | even | 2 |
| d400-ref-edge | 400 | dimensional_reference | edge | even | 2 |
| d400-charuco-center | 400 | geometry | center | even | 1 |
| d400-charuco-edge | 400 | geometry | edge | even | 1 |
| d600-ref-center | 600 | dimensional_reference | center | even | 2 |
| d600-ref-edge | 600 | dimensional_reference | edge | even | 2 |
| d600-charuco-center | 600 | geometry | center | even | 1 |
| d600-charuco-edge | 600 | geometry | edge | even | 1 |
| d250-calib-A | 250 | calibration | varied | even | 15 |
| d250-repeat | 250 | repeatability | center | even | 10 |
| d250-rotated | 250 | planar_part | rotated | even | 2 |
| d250-light-hard-shadow | 250 | lighting | center | hard_shadow | 2 |
| d250-light-low | 250 | lighting | center | low | 2 |
| d250-light-bright-overhead | 250 | lighting | center | bright_overhead | 2 |
| d250-clutter | 250 | clutter | center | even | 2 |
| d250-calib-B | 250 | calibration | varied | even | 15 |
| d250-focus-rest | 250 | focus | center | even | 5 |

