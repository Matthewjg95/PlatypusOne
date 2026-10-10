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

| Arduino's manual: "If you want to use just one camera, make sure to enable and connect it on CAMERA0 connector"; libcamera camera names `/base/soc@0/cci@5c1b000/i2c-bus@0/sensor@10` (camera0) and `…/i2c-bus@1/sensor@10` (camera1); snapshot via `gst-launch-1.0 libcamerasrc` (needs `gstreamer1.0-libcamera`) | [UNO Media Carrier user manual](https://docs.arduino.cc/tutorials/uno-media-carrier/user-manual/), rev. 09/22/2026 |
| Arduino's reference photo shows an M12-lens IMX219 module on an **orange flex** cable; the manual's "MIPI cable contacts orientation" diagram is an image only | same manual |
| B0394 package (retailer listing): a 15 cm **22-22-pin** cable and a 15 cm **yellow 15-22-pin** cable; Arducam's own page refused automated access | [welectron listing](https://www.welectron.com/Arducam-B0394-8MP-IMX219-Camera-Module-for-Raspberry-Pi_1) |

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

### Boot-time re-bind (installed once)

The imx219 probe fails on most boots because it runs inside the panel's flaky
CCI window (bench log below). `sudo tools/csi_bench/install_imx219_rebind.sh`
installs `platypus-imx219-rebind.service`, which runs after
`uno-q-dsi-panel-recover.service` (pulled in by it, not by
`multi-user.target`, to avoid an ordering cycle) and binds any unbound IMX219,
retrying for ~30 s. `--remove` uninstalls. Manual equivalent:
`sudo sh -c 'for d in /sys/bus/i2c/devices/*-0010; do [ -e $d/driver ] || basename $d > /sys/bus/i2c/drivers/imx219/bind; done'`.

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
| T6 focus / working distance | choose the working distance (record mm); B0394: lens staked by the vendor (fixed focus) - record the distance at which it is sharpest; B0393: record the focus control value (or "no lens control enumerated"); B0390: fixed | notes + `lens.txt` |
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
- **B0394:** the M12 lens is staked (fixed focus); find and record the
  distance at which it is sharpest rather than refocusing it.
- Vendor specifications (FOV, distortion, board size) are recorded as vendor
  data, never as bench results.
- No algorithm is tuned from these runs; they are fixtures.

## Bench log

### 2026-10-04 — B0394, T1 not yet passing

| Run (evidence dir) | Setup | Result |
|---|---|---|
| `20261004T220941Z-all` | ports `none` (before enabling) | no sensor entity; expected |
| `20261004T221912Z-all`, `20261004T222234Z-all` | both ports `type1-2lanes`; white AWM 20624 cable in **CAMERA1**, two carrier-end orientations (blue stiffener up / contacts up) | driver probes both ports: `imx219 N-0010: Error reading reg 0x0000: -6` / `failed to read chip id 219` (ENXIO: nothing acknowledges at 0x10); panel and touch unaffected on the shared CCI bus |
| `20261004T222854Z-all` | same, after re-seat | same ENXIO on both buses (`2-0010`, `3-0010`) |

| `20261004T224100Z-probe` (B0394) | CAMERA0, white cable re-seated per Arduino's diagram; yellow 15-22 cable does not physically fit | same ENXIO |
| `B0390/20261004T224613Z-all` — **module was the B0393** (see `CORRECTION.txt`) | B0393 on CAMERA0 with its own cable | same ENXIO |
| `B0390/20261004T224138Z-all` — module unconfirmed (`CORRECTION.txt`) | during the swap | same ENXIO |

**Likely cause found (2026-10-04): the installed overlays predate
[arduino/linux-qcom PR #5](https://github.com/arduino/linux-qcom/pull/5)**
(merged 2026-09-28). Their `reset-gpios` drives the sensor's XCLR through the
PCA9555, which sits on the same CCI bus as CAMERA0's sensor; third-party
IMX219 modules load that bus while XCLR is low, the expander stops
answering, and XCLR is never released. The PR's testing used this exact
kernel (`7.0.0-g122c2c22d838`) and third-party modules on both ports. The
live device tree here still has `reset-gpios = <pca9555 0>` / `<pca9555 2>`,
and the bench shows CCI queue timeouts. Recovery needs a **cold** power
cycle: the expander keeps its state across a warm reboot, and every reboot
in the runs above was warm.

Local application until Arduino ships it:
`tools/csi_bench/imx219_overlay_fix.sh build` (user: rebuilds the four
overlays without `reset-gpios` and test-composes them with the base,
carrier and current panel overlays) → `sudo … install` (keeps originals as
`.arduino-orig`, re-applies the current camera/display options) → unplug
~10 s → `csi_bench.sh`. `sudo … rollback` restores Arduino's files.

**After the fix (overlays installed, cold power cycle, 2026-10-04 22:54):**
the B0393 on CAMERA0 **answered its chip ID** — `1-0010` bound to the imx219
driver (run `B0393/20261004T230019Z-probe`, `i2c_sensors.txt`). CAMERA1,
enabled but empty, still fails as expected. No media graph appears because
camss waits for every enabled port's sensor: an empty enabled port blocks the
other. With one camera, configure `camera1=none` (Arduino's manual: single
camera on CAMERA0). Panel note: the ATTINY's first writes fail on several
boots (also before cameras were enabled); `uno-q-dsi-panel-recover` brings
the display up about 30 s after boot.

**CAMERA0 is the wrong port for these modules on this board (2026-10-04
23:01–23:03, camera1=none, blue stiffener up at the carrier):** run
`B0393/20261004T230239Z-all` and the next boot show
`imx219_power_on: failed to enable regulators` (-110), every ATTINY panel
write timing out, and 156 CCI timeouts. CAMERA0's sensor shares CCI bus 0
with the PCA9555 that switches camera power, the panel's ATTINY and touch.
A connected but unpowered third-party module loads that bus, so the
expander cannot switch the camera on and the panel cannot be initialised
(the "display dark with blue side up" report). The 22:54 success was a
timing win, not a stable state. CAMERA1's sensor is on CCI bus 1, away from
the expander and the panel: use `camera0=none camera1=type1-2lanes` for a
single third-party IMX219 on this board, contrary to the manual's
"single camera on CAMERA0" (written for genuine modules).

**Boot-time probe race, and first frames (2026-10-04 23:12–23:18).** With
`camera0=none camera1=type1-2lanes` the B0393 on CAMERA1 still failed at boot
with `imx219_power_on: failed to enable regulators` (-110): the probe runs at
~7 s, while CCI bus 0 (PCA9555 + panel ATTINY + touch) is in its flaky
start-up window that `uno-q-dsi-panel-recover` exists for (every ATTINY write
timing out until ~24 s), so the expander write that switches camera power
times out and the probe is not retried. A manual re-bind after the panel
recovered (`echo 3-0010 > /sys/bus/i2c/drivers/imx219/bind`) succeeded:
run `B0393/20261004T231534Z-all`, sensor `/dev/v4l-subdev12`, libcamera camera
`/base/soc@0/cci@5c1b000/i2c-bus@1/sensor@10`, pixel array 3280×2464,
SRGGB10/SRGGB8.

First frames: the simple pipeline's software ISP delivers ABGR8888 at
**640×480 and 1280×720, ~60 fps**; 1640×1232 and larger fail with
`dma-heap allocation failure` (CMA total 32 MB). The first-light run
`B0393/20261004T231740Z-all` kept three 1280×720 frames: auto-exposure ran
the analogue gain to its maximum (232) and the image is green-tinted noise —
almost no light (lens cap or covered view, to confirm) and no colour tuning
(`imx219.yaml` absent, libcamera falls back to `uncalibrated.yaml`).

**Real scene and the raw path (2026-10-04 23:24–23:30).** Run
`B0393/20261004T232424Z-all` shows recognisable structure (a ceiling, room
lights off — `CORRECTION.txt`). libcamera's simple pipeline does not offer a
raw stream here (a `role=raw` request returns ABGR8888), so raw Bayer was
read directly, as Arduino's manual recommends for quality images:

```sh
media-ctl -d /dev/media0 -V '"imx219 3-0010":0[fmt:SRGGB10_1X10/1640x1232]'   # bus number varies per boot
media-ctl -d /dev/media0 -V '"msm_csiphy1":0[fmt:SRGGB10_1X10/1640x1232]'
media-ctl -d /dev/media0 -V '"msm_csid0":0[fmt:SRGGB10_1X10/1640x1232]'
media-ctl -d /dev/media0 -V '"msm_vfe0_rdi0":0[fmt:SRGGB10_1X10/1640x1232]'
v4l2-ctl -d /dev/v4l-subdev12 -c exposure=1700 -c analogue_gain=100
v4l2-ctl -d /dev/video0 --set-fmt-video=width=1640,height=1232,pixelformat=pRAA     --stream-mmap=4 --stream-count=8 --stream-skip=4 --stream-to=raw.bin
```

`pRAA` = SRGGB10 MIPI-packed (5 bytes per 4 pixels), 2056 bytes per line,
2 532 992 bytes per frame, black level ~64 of 1023. This path fits the CMA
pool at 1640×1232 and is the one for measurement: fixed exposure/gain, no
uncalibrated processing. Archived: `B0393/20261004T232839Z-raw`.

**First lit capture (2026-10-04 23:32, run `B0393/20261004T233254Z-all`):**
calibration sheet at ~15 cm under room light, 1280×720 software-ISP preview:
the 20 mm square is recognisable but **out of focus**, and the image is dim
(auto-exposure at the 60 fps exposure limit, gain 192 of 232). No lens/VCM
subdevice enumerated (`lens: none`): the B0393's autofocus cannot be driven on
this board as configured, so its lens rests at a far focus. For quantitative
use it is a fixed-focus camera focused beyond the bench working distance
until a focus driver exists.

Reading before the fix: the camera bus and the carrier's camera power switching are
configured (`cam-pwr-csi0`, `cam-pwr-csi1` on the TCA9555); the sensor never
answers. Untested so far: CAMERA0 (Arduino's single-camera port), the
camera-end contact orientation, and the yellow cable (Arduino's reference
photo uses an orange flex). Do not conclude the module is faulty until those
are excluded.

### 2026-10-09 — B0394 enumerates; kiosk CSI path

- B0394 on CAMERA1 failed `failed to read chip id` (ENXIO) on every re-bind
  with the cable as first fitted (`B0394/20261010T015449Z-probe`). Fitted
  in the same orientation as the working B0393 (same cable type), its boot
  probe hit the usual bus window and `platypus-imx219-rebind.service` bound
  it automatically (`3-0010 driver=imx219`): the service's first real use.
  Run `B0394/20261010T020409Z-all`: frames of a room scene.
- The B0394's M12 lens is **staked** (thread fixed by the vendor): a
  fixed-focus camera at Arducam's set distance. Its sharpness at the bench
  working distance is to be measured, not assumed.
- Live preview and capture on the panel now come from the kiosk's own CSI
  raw camera (PR #48): raw SRGGB10 from the RDI node, 2×2 binning to
  820×616, gray-world white balance, auto-exposure on the sensor controls.
- Evidence backed up off the board: `Documents/PlatypusOne-board-backup/2026-10-09-csi/`.

Quantitative characterization (calibration, focus, distortion, exposure,
repeatability, the three-role decision) uses
[CAMERA_CHARACTERIZATION.md](CAMERA_CHARACTERIZATION.md) and
`tools/camera_characterize/` (#46); this document covers bring-up.

## Comparison (fill from evidence only)

| Criterion | B0394 low-distortion, manual focus | B0393 autofocus | B0390 compact, fixed focus |
|---|---|---|---|
| T1 enumerates on Media Carrier | **yes on CAMERA1** (2026-10-09) after the overlay fix, cable orientation as the B0393, auto re-bind | **yes on CAMERA1** after the PR #5 overlay fix + a manual re-bind past the boot-time bus window (2026-10-04) | UNRESOLVED |
| CSI port / lanes | UNRESOLVED | UNRESOLVED | UNRESOLVED |
| T2 working modes (format, size, fps) | UNRESOLVED | ABGR8888 640×480 and 1280×720 at ~60 fps; ≥1640×1232 blocked by 32 MB CMA (2026-10-04) | UNRESOLVED |
| T3 repeat capture after reboot / power cycle | UNRESOLVED | UNRESOLVED | UNRESOLVED |
| T4 DSI + touch coexistence | UNRESOLVED | UNRESOLVED | UNRESOLVED |
| T5 envelope (measured) | UNRESOLVED | UNRESOLVED | UNRESOLVED |
| T6 focus control / working distance | lens **staked** by the vendor: fixed focus, not adjustable (2026-10-09) | **no lens control enumerated**; rests at far focus, blurry at ~15 cm (2026-10-04) | UNRESOLVED |
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
