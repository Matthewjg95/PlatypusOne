# CSI camera bench-off — Arducam IMX219 on the UNO Media Carrier (#40)

Order: **B0394 → B0393 → B0390.** The USB webcam stays the working fallback
until a CSI camera has equivalent physical evidence (carrier gate 0).

Nothing here is a CSI result yet. Facts below are labelled by source.

## What is already established (UNO Q, 2026-10-04)

| Fact | Source |
|---|---|
| Kernel 7.0.0 ships `imx219.ko` and Arduino overlays `qrb2210-arduino-imola-carrier-media-camera-imx219-csi{0,1}-{2,4}lanes.dtbo` | board filesystem |
| `arduino-linux-config carrier list`: `camera0` / `camera1` each `none \| type1-2lanes \| type1-4lanes`; `type1` = the IMX219 overlays above | board CLI |
| Configuration on 2026-10-04: **camera0 = none, camera1 = none**, display = `5-dsi-touch-a` | `carrier show` |
| With both ports `none`, a connected B0394 cannot enumerate: no sensor entity in `media-ctl`, libcamera "No sensor found for /dev/media0", nothing at the IMX219 address on either CCI bus | baseline probe `B0394/20261004T220941Z-all` |
| The `5-dsi-touch-a` slot on the EFI partition holds the **patched** Waveshare 800×480 overlay (`waveshare,4-3-inch-dsi`, sha256 `b438db9b…`), with Arduino's original kept as `.arduino-orig` | `/boot/efi/dtb/qcom` |

Consequences:

- Enabling a camera port must restate `display=5-dsi-touch-a`; the tool then
  re-composes the boot device tree from the patched slot, so the panel stays.
- Pi-style IMX219 modules are 2-lane: use `type1-2lanes`.
- Which physical CSI connector is `csi0` vs `csi1` is **unresolved**. Enabling
  both ports lets whichever one holds the module enumerate; the probe records
  which.
- Cable orientation cannot be judged while the ports are `none`.

## One-time configuration (Matthew, sudo)

Board powered through the hub, not from a PC port. CSI is not hot-pluggable:
power off before connecting or swapping a module.

```bash
sudo arduino-linux-config carrier enable media-carrier camera0=type1-2lanes camera1=type1-2lanes display=5-dsi-touch-a && sync && sudo reboot
```

Rollback (cameras off, panel kept):

```bash
sudo arduino-linux-config carrier enable media-carrier camera0=none camera1=none display=5-dsi-touch-a && sync && sudo reboot
```

If the panel does not come back, recovery is in [DSI_BRINGUP.md](DSI_BRINGUP.md)
(the `.arduino-orig` restore and the stock-panel path).

## Per-camera procedure

`tools/csi_bench/csi_bench.sh <SKU> <probe|capture|all> "notes"` writes
`~/PlatypusOne/evidence/csi/<SKU>/<UTC>-<test>/`: system identity and source
SHA, carrier config, camera kernel log, media graph, libcamera view, sensor and
lens controls, display/touch coexistence, power telemetry if any, frames, and a
SHA256 manifest. Raw output is git-ignored; archive a run deliberately under
`docs/hardware/evidence/` with a run record.

Put in the notes: connector used, cable orientation at both ends (photograph
it), lens/focus state, working distance, lighting, what is in the scene.

| Step | Do | Evidence |
|---|---|---|
| T1 enumeration | power on, `csi_bench.sh <SKU> probe` | sensor subdev found, kernel log |
| T2 capture modes | `csi_bench.sh <SKU> capture` | frames + `cam --info` stream formats |
| T3 repeat / reboot | reboot, `capture` again; then a cold power cycle, `capture` again | three runs, three boot IDs |
| T4 DSI coexistence | during T1–T3: panel lit, touch responds, kiosk still runs | `coexistence.txt` + a photo of the lit panel |
| T5 physical envelope | calipers: board W×H, lens stack height above PCB, connector/cable exit, mounting holes | measured values; vendor drawing values recorded separately, labelled vendor |
| T6 focus / working distance | choose the working distance (record mm); B0394: set the M12 lens ring and lock it; B0393: record the focus control value (or "no lens control enumerated"); B0390: fixed | notes + `lens.txt` |
| T7 edge sharpness | calibration sheet flat at the working distance, centre and corner placements | frames; metric computed offline and archived with its script |
| T8 calibration / distortion | a printed planar grid at the working distance, ≥10 poses | frames; reprojection residual from an archived offline method |
| T9 exposure / lighting | the Scout lighting set: even desk, uneven overhead, hard shadow, dim | frames per condition; controls in `sensor.txt` |
| T10 power | inline USB-C meter: idle, streaming | readings in notes (board has no telemetry) |

Rules:

- **B0393 (autofocus):** quantitative captures (T7, T8, T9) need a recorded,
  fixed focus state. Refocusing moves the lens and changes the effective
  intrinsics, so a calibration from one focus state does not hold at another.
  If no lens control enumerates, record that and treat the module as fixed at
  whatever position it rests in; do not assume.
- **B0394 (manual focus):** once the ring is set for the working distance, do
  not touch it between T7 and T8; note any change.
- Vendor specifications (FOV, distortion, board size) are recorded as vendor
  data, never as bench results.
- No algorithm is tuned from these runs; they are fixtures.

## Comparison (fill from evidence only)

| Criterion | B0394 low-distortion, manual focus | B0393 autofocus | B0390 compact, fixed focus |
|---|---|---|---|
| T1 enumerates on Media Carrier | UNRESOLVED | UNRESOLVED | UNRESOLVED |
| CSI port / lanes | UNRESOLVED | UNRESOLVED | UNRESOLVED |
| T2 working modes (format, size, fps) | UNRESOLVED | UNRESOLVED | UNRESOLVED |
| T3 repeat capture after reboot / power cycle | UNRESOLVED | UNRESOLVED | UNRESOLVED |
| T4 DSI + touch coexistence | UNRESOLVED | UNRESOLVED | UNRESOLVED |
| T5 envelope (measured) | UNRESOLVED | UNRESOLVED | UNRESOLVED |
| T6 focus control / working distance | UNRESOLVED | UNRESOLVED | UNRESOLVED |
| T7 edge sharpness (centre / corner) | UNRESOLVED | UNRESOLVED | UNRESOLVED |
| T8 distortion residual | UNRESOLVED | UNRESOLVED | UNRESOLVED |
| T9 exposure / low light | UNRESOLVED | UNRESOLVED | UNRESOLVED |
| T10 power idle / streaming | UNRESOLVED | UNRESOLVED | UNRESOLVED |
| Cable orientation photo | UNRESOLVED | UNRESOLVED | UNRESOLVED |

Decision output for #33: reference/measurement camera, compact product
candidate, fallback.

## Software gap (not a bench result)

Engineering Scout captures through the UVC/YUYV `V4l2Camera`. A CSI IMX219 on
camss delivers Bayer through libcamera's simple pipeline, so Scout needs a
libcamera-backed `hal::ICamera` (or a documented `cam` capture bridge) before
it can measure through CSI. That work starts after T1–T2 show which formats
actually stream; until then the kiosk keeps the USB webcam.
