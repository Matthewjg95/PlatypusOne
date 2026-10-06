# Camera characterization — evidence to decision (#40)

Status, 2026-10-06:

- **The pipeline is built and tested on synthetic fixtures only.**
- **No camera has been characterized.** No camera role is chosen.
- Nothing on this page is a measured camera result unless it is marked MEASURED, with its source.

```
physical capture (UNO Q)          capture_raw.sh + `camera_characterize capture`
  → immutable source evidence     frames/ + sha256 in dataset.json; never rewritten
  → metadata validation           `validate`: contract, files, sizes, hashes, plan coverage
  → calibration / image quality   `analyze`: ChArUco intrinsics, centre/edge geometry,
                                  edge-rise focus, exposure, Scout measurements
  → repeatability                 unmoved series → spread; verified truth → accuracy
  → comparable metrics + gate     `compare`: per-camera metrics, three roles or
                                  INSUFFICIENT EVIDENCE (decision_criteria.json)
  → report                        `report`: derived/<version>/report.md
```

One command, once the datasets are in place:
`python3 tools/camera_characterize/camera_characterize.py all <experiment>`.
The tool is in [`tools/camera_characterize/`](../../tools/camera_characterize/README.md);
the capture plan is in [CAMERA_CAPTURE_MATRIX.md](CAMERA_CAPTURE_MATRIX.md).

## 1. What is known about the three cameras

| Fact | Class | Source |
|---|---|---|
| All three are Sony IMX219 8 MP modules | DOCUMENTED | Arducam listings via #40 |
| B0394: M12 manual focus, ~75° H / 60° V, "<3 %" lens distortion, 24 × 25 mm board | DOCUMENTED (vendor) | #40. The "<3 %" figure is the vendor's definition on the vendor's sample, **not our calibrated distortion** |
| B0390: fixed focus, ~62.2° H, 25 × 24 mm | DOCUMENTED (vendor) | #40 |
| B0393: motorized focus. On Raspberry Pi, autofocus needs `dtoverlay=imx219,vcm` plus the rpicam/libcamera RPi IPA tuning `imx219_af.json` | DOCUMENTED | [Arducam IMX219 motorized-focus guide](https://docs.arducam.com/Raspberry-Pi-Camera/Motorized-Focus-Camera/Quick-Start-Guide/IMX219-Motorized-Focus-Camera/) (lists B0393) |
| On the Pi, that overlay declares the lens as an `adi,ad5398` VCM at I2C `0x0c`, linked to the sensor through `lens-focus` | DOCUMENTED | [raspberrypi/linux `imx219-overlay.dts`](https://github.com/raspberrypi/linux/blob/rpi-6.12.y/arch/arm/boot/dts/overlays/imx219-overlay.dts) |
| UNO Q kernel 7.0.0 has `imx219.ko` and Arduino `imx219-csi{0,1}-{2,4}lanes` overlays | MEASURED | board filesystem, PR #42 bench log |
| B0393 enumerates on CAMERA1 after the overlay fix + re-bind; ABGR8888 640×480 / 1280×720 at ~60 fps via libcamera's simple pipeline; ≥1640×1232 fails (32 MB CMA) | MEASURED | PR #42 bench log 2026-10-04 (raw run dirs not yet archived in the repo) |
| Raw SRGGB10 packed 1640×1232 (2056 B/line, black ~64) streams via camss/v4l2 | MEASURED | PR #42, run `B0393/20261004T232839Z-raw` |
| **B0393: no lens/VCM subdevice enumerates on UNO Q; lens rests at far focus, blurry at ~15 cm** | MEASURED (compatibility result) | PR #42, run `B0393/20261004T233254Z-all` |
| CSI camera "up" on UNO Q | REPORTED, unarchived | Matthew, #40 comment 2026-10-04. SKU/still/reboot/mode not recorded |
| B0394 enumeration on UNO Q | UNKNOWN | ENXIO before the overlay fix; not re-tested in the record |
| B0390 on UNO Q | UNKNOWN | never confirmed (one run was actually the B0393) |
| UNO Q kernel has an `adi,ad5398` lens driver; Arduino overlays describe a lens node | UNKNOWN → BENCH | `modinfo ad5398`, `find /lib/modules -name '*ad5398*'`, live DT. The empty lens subdev suggests no (INFERRED) |
| Calibrated intrinsics, distortion, edge geometry, focus envelope, repeatability, accuracy, lighting robustness | BENCH REQUIRED | this pipeline, once the matrix is captured |

**Do not install Raspberry Pi autofocus software on the UNO Q.** `imx219_af.json`
belongs to the RPi vc4 IPA, and the UNO Q runs libcamera's simple pipeline
with no AF algorithm. Until a lens subdevice exists on the UNO Q, the B0393 is
a fixed-focus camera resting at an unknown far position (`focus_mode: not_exposed`).
Recording that is a result in itself.

## 2. Evidence contract

Schemas: `platypus.camera_experiment/1` and `platypus.camera_dataset/1`. The
code is [`camchar/contract.py`](../../tools/camera_characterize/camchar/contract.py).

```
<experiment>/experiment.json          datasets compared, representative distance
<experiment>/datasets/<id>/dataset.json
<experiment>/datasets/<id>/frames/    immutable sources (+ capture logs)
<experiment>/datasets/<id>/capture_plan.json   planned cells (coverage)
<experiment>/derived/<ANALYSIS_VERSION>/       everything computed; deletable
```

| Block | Fields (all required; `"UNKNOWN"` allowed and reported) |
|---|---|
| `camera` | sku, sensor, silkscreen, revision, unit_id, lens, focus_type |
| `platform` | host, repo_sha, kernel, image, csi_port, enumeration, capture_stack |
| `bringup` (gate input) | enumerates, reboot_repeat, dsi_coexistence, focus_control, evidence (csi_bench run dirs) |
| `physical` (gate + handoff) | board_w_mm, board_h_mm, lens_stack_mm, mounting_holes, optical_center_offset_mm, connector_side, cable. Each is `{value, status, source}`; only `status: measured` counts |
| `targets` | `charuco` (squares, square_mm, marker_to_square, dictionary), `scout_reference` (reference_mm), `planar_part` (truth length/width). Every dimension carries a status: `verified` / `nominal_unverified` / UNKNOWN |
| per frame (or `defaults`) | capture_command, pixel_format, frame_rate, exposure_us, analogue_gain, white_balance, focus_mode, focus_position, working_distance_mm, scene, placement, lighting, targets, timestamp_utc |
| per ok frame | frame_id, path, sha256, format (`raw10p` / `raw16` / `abgr8888` / `yuyv` / `png` / `pgm` / `ppm`), width, height; raw Bayer also stride, bayer_pattern, black_level, white_level |
| optional | plan_cell, plan_index, repeat_series, repeat_index, calibration_set, notes |
| failures | `outcome: capture_failed / discarded` + `failure_reason`. Kept in the record, never analysed, listed in the report |

**Rules:**

- Unknown means `"UNKNOWN"`. The validator warns, and the gate treats it as missing evidence.
- A sha256 mismatch, a missing file, a wrong size, or a path outside the dataset
  excludes that frame.
- A schema or section error refuses the whole analysis.
- `analyze` never opens a source for writing. It re-hashes every source after
  the run and aborts on any change.
- Adding a camera means adding a dataset with a new `sku`. The architecture does not change.

**Storage (decision for Matthew):** one raw frame is 2.53 MB, so about 75 frames
per camera comes to roughly 190 MB per camera. Recommendation:

- commit `experiment.json`, the `dataset.json` files, `capture_plan.json`, the
  capture logs and `derived/`;
- archive each dataset's `frames/` as a zip on a GitHub release (or Git LFS);
- record the zip's sha256 in the experiment notes.

The per-frame hashes in `dataset.json` keep the evidence verifiable either way.

## 3. Analyses and their limits

| Analysis | Output | Limits (by design) |
|---|---|---|
| Exposure | mean, median, p01/p99, RMS contrast, clipped % (≥99.5 % full scale), crushed % (≤0.5 %) | raw is linear; ABGR/YUYV/PNG are tone-mapped. Compare within one format only |
| Region sharpness | normalised Tenengrad and Laplacian variance on a 3×3 grid; corner/centre ratio | scene-dependent: compare the same scene and placement only |
| Edge rise (focus) | median 10–90 % width (px) of the 20 mm reference square's edges, 9 profiles per edge | blur of the whole chain at that pixel grid. Comparable across distances, at the same resolution only. Not MTF and not a resolution claim |
| Calibration | OpenCV pinhole k1 k2 p1 p2 k3 from ChArUco: K, distortion, std devs, per-view RMS, accepted/rejected views with reasons, coverage, model displacement at image corners, undistortion preview | no post-hoc view rejection. A set is invalid if the focus state changes inside it. Minimum 10 views |
| Calibration stability | max relative fx/fy difference between sets A and B (B after remount) | needs both sets at the same size |
| Centre / edge geometry | per ChArUco frame: reprojection RMS by zone; line-straightness RMS as captured and after undistortion; planar residual (mm) | the planar residual uses the board's own scale: shape, not absolute accuracy |
| Repeatability | Scout length/width through `scout_measure` (the product analyzer, unchanged): n accepted/refused, mean, std, range, relative spread; as captured and undistorted | unmoved series only |
| Accuracy | mean (bias), mean \|error\| and max \|error\| against the part's **verified** truth | never computed from nominal dimensions. **No correction factor is ever applied** |
| Temporal noise | median per-pixel std across an unmoved series | only meaningful when nothing moved |

Zones: radius from the principal point, normalised to the farthest image
corner. Centre is below 1/3; edge is 2/3 and above.

## 4. Decision gate

[`decision_criteria.json`](../../tools/camera_characterize/decision_criteria.json)
fixes every threshold **before any physical frame was analysed**. Every report
carries its sha256.

**Roles:**

- **Reference / measurement:** stable intrinsics, calibratable distortion,
  centre ≈ edge geometry, repeatable focus, a working envelope, and Scout repeatability.
- **Compact product:** packaging limits plus adequate geometry, envelope,
  focus behaviour, stability, and bring-up.
  - The board and lens-stack limits are deliberately `null` until Fusion
    supplies them (#41/#33).
  - So the product role is INSUFFICIENT EVIDENCE by construction until then.
- **Fallback:** known working path, predictable over reboots and lighting, and
  distinct from the product camera.

**Outcomes:**

- **PROPOSED:** exactly one eligible camera. Matthew confirms it.
- **CANDIDATES:** several are eligible; there is no automatic tie-break.
- **INSUFFICIENT EVIDENCE:** a needed metric or threshold is missing.
- **NO ELIGIBLE CAMERA:** every camera failed at least one criterion.

There is no weighted score. Synthetic datasets can never produce a role.

## 5. Reuse — what already existed

| Existing | Used as |
|---|---|
| `tools/csi_bench/csi_bench.sh` + CSI_CAMERA_BENCH.md (PR #42, open) | bring-up evidence (T1–T4, T10) feeding `bringup`; not duplicated |
| Raw capture recipe (PR #42 bench log) | packaged as `capture_raw.sh`. **Never run as a script** |
| `services/vision` ScoutAnalyzer | measured through `tools/scout_measure` (new ~140-line bridge, no new measurement logic) |
| `docs/hardware/calibration_sheet.pdf` (20 mm square) | Scout reference + edge-rise target |
| Engineering Observation evidence classes | observed (pixel facts), derived (mm), unresolved (UNKNOWN) |
| `hardware/pcb/CAMERA_EVIDENCE_INTAKE.md` | the PCB consumer; the report's "PCB / Fusion handoff" block maps onto its tables B/C |

New: a ChArUco target, because the existing sheet cannot constrain
distortion. 7×10 squares, 20 mm, 14 mm DICT_4X4_50 markers:
[`targets/charuco_7x10_20mm.svg`](../../tools/camera_characterize/targets/charuco_7x10_20mm.svg).

- Print at 100 % on matte paper and mount it flat.
- Measure 6 squares with calipers before setting `square_mm_status: verified`.
- Its dimensions are not claimed until measured. Intrinsics do not depend on
  the square size; mm residuals and accuracy do.
