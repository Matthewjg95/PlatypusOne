# Platypus One — Hardware Test Checklists

Work each checklist top to bottom; do not proceed to the next stage until the
current one passes. Record date/initials/notes inline (copy this file per unit
if building more than one). Contest tie-in: photograph each stage — bring-up
photos feed the 20-point documentation score.

**Post-submission update, 2026-10-01:** Dream Lab was submitted September 30.
Session A/B are historical checklists; unchecked items are not silently
promoted to PASS. Use [UNO_Q_SCOUT_BASELINE.md](UNO_Q_SCOUT_BASELINE.md) for the
single pinned build/launch sequence and [the evidence index](../contest/evidence/dreamlab-2026-09-30/README.md)
for final records. The fitted display is the **Waveshare 5inch DSI LCD
Rev2.2, 800×480**, not the 720×1280 5-DSI-TOUCH-A.

## First UNO Q session — status board

The bring-up-brief sequence for the current physical session. The primary path is now the UNO Q + UNO Media Carrier + Waveshare 5inch DSI LCD Rev2.2 (800×480); the M5Stack Tab5 remains a fallback/dev display fixture. Update the
Status column as physical testing occurs: `UNTESTED`, `PASS`, `FAIL`, or
`BLOCKED (reason)`. Nothing is claimed compiled or run on the UNO Q until it
actually has. `REPORTED` identifies a result in bench notes whose raw run
evidence and exact tested SHA have not yet been archived.

| # | Step | Detail | Status |
|---|---|---|---|
| 1 | Board/environment verification | Checklist §1 (boot, network, `uname -a`, storage) | PASS 2026-09-19 — boots, Debian 13 aarch64 4-core/3.6 GB, 3.0 GB free on `/`; Wi-Fi joined, `ping 8.8.8.8` OK; sshd enabled (needed `ssh-keygen -A`, factory image ships no host keys); key login from the PC |
| 2 | Hello-world | Any trivial program compiles + runs on the Linux side | UNTESTED |
| 3 | Linux-side Platypus executable | On-device native build: cmake + full `platypus_tests` + launcher headless (§2) | PASS 2026-09-19 — GCC 14.2 aarch64, Ninja, 2m44s on 4 cores; `platypus_tests` 14/14; `--fake` harness writes a valid record. Launcher headless soak not yet run |
| 4 | MCU ↔ Linux communication | Flash `firmware/mcu_bridge`; Ping/Pong + GPIO loopback over `/dev/ttyRPMSG0` (§2) | UNTESTED |
| 5 | Waveshare DSI proof-of-life | Media Carrier + 5inch DSI LCD Rev2.2 (800×480), backlight/touch | REPORTED 2026-09-29 — ATTINY `0x45`, touch `0x38`, correct FFC orientation; working cable photos retained in DSI_BRINGUP.md |
| 6 | Touch → app response | Touch press reaches the app and can act as the first-pass Scout trigger | REPORTED 2026-09-29 — on-glass kiosk loop in DEMO_READINESS.md; final run archive pending |
| 7 | Camera capture | `engineering_scout_capture --device /dev/videoN` produces a real observation (§4). Select by capability; device numbers change between boots | PASS 2026-09-21 — UGREEN hub + Adesso CyberTrack H4 on `/dev/video2`, 640x480 YUYV, valid record written (`scan-0001`); YUYV also at 1280x720/1920x1080. Measurement was pending the printed sheet at that session |
| 8 | Scout pipeline on a real image | Analyzer + classifier over a physical fastener beside the 20 mm reference | REPORTED first light — 49.6 × 12.1 mm vs 44.45 mm overall length / 9.48 mm head; actual screw 1/4-20 UNC. Workflow ran; accuracy not accepted. Later focused/tilted comparisons are preserved as narrative/offline results |
| 9 | Result on Waveshare | Scout result card renders on the 800×480 panel and remains touch-usable | REPORTED on glass in DEMO_READINESS.md; physical result media/raw record and tested SHA pending archive |

Session prerequisites: UNO Media Carrier + Waveshare 5inch DSI LCD Rev2.2 installed with power removed; webcam available on USB; printed 20 mm reference sheet verified at 100% scale. Touch remains the accepted trigger; MCU bridge/arduino-cli work is not a blocker for baseline replay.

## Session A — first camera-on-board run (Dream Lab MVP step 6)

Goal: one real `observations/scan-*/` record with a calibrated measurement,
produced on the UNO Q from the Adesso CyberTrack H4. That artifact is the core
of the DigiKey Dream Lab demo (deadline Sep 30). Budget: 1–2 h once the hub is
on the bench. Prerequisites and the pre-session steps are done over ADB while
the board is still on the PC's USB-C.

### A0. Pre-session (board on USB, tonight)

Bundled ADB: `%LOCALAPPDATA%\Arduino15\packages\arduino\tools\adb\32.0.0\adb.exe`
(installed by the `arduino:zephyr` core). `adb devices` must list the board.

- [x] Wi-Fi joined (the `arduino` user is in `netdev`, no sudo needed):
      `adb shell nmcli device wifi connect "<SSID>" password "<pw>"` — the owner
      types this; the password never goes through an agent
- [x] SSH enabled (needs the `arduino` user's sudo password — owner types it):
      `adb shell -t sudo systemctl enable --now ssh`. A factory board first forces
      a password reset here, and the image ships without SSH host keys, so follow
      with `sudo ssh-keygen -A && sudo systemctl restart ssh`
- [x] Board IP recorded: `adb shell ip -4 -br addr show wlan0` → **192.168.1.32**
      (or `hostname -I`); reserve it in the router if convenient
- [x] `ssh arduino@<ip>` works from the PC with the board still on USB
- [x] Build toolchain installed on the board (**none of these are present on the
      stock image**): `sudo apt install -y build-essential cmake ninja-build`
      (~250 MB; 3.0 GB free on `/`). `git`, `python3`, `v4l2-ctl` are already there
- [x] Repo on the board: `git clone https://github.com/Matthewjg95/PlatypusOne.git
      && cd PlatypusOne && git checkout main && git pull --ff-only`
- [x] First on-device build while still on USB (catches aarch64/GCC surprises
      before the hub session): `cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release
      && cmake --build build-bench -j4 && ./build-bench/tests/platypus_tests`
      → status-board step 3 **PASS** (2026-09-19, 14/14 suites)
- [x] Print [calibration_sheet.pdf](calibration_sheet.pdf) at **100 % / actual size**
      (generator: `tools/calibration_sheet/make_calibration_sheet.py`); check the
      100 mm bar with a ruler; cut the strip off. Page 1 = validation (20 mm square +
      40x8 mm printed bar, expect 40.0 x 8.0), page 2 = working sheet. One M-series
      bolt and one nut to hand
- [ ] USB-C PD charger ≥ 20 W located for the hub's PD port

### A1. Rig (hub arrived)

Powering the board through the hub means the PC link over USB-C is gone — this
session is **SSH over Wi-Fi only**. If the board does not come up on Wi-Fi,
fall back to the USB-C link, fix networking, and retry.

- [ ] Unplug from PC. PD charger → hub PD-in; hub → UNO Q USB-C; **camera into
      a USB-A port**. Power LED on; platypus paddling (MCU alive)
- [ ] `ssh arduino@<ip>` reachable within 60 s of power-on
- [ ] `v4l2-ctl --list-devices` shows the webcam as its own entry (expect
      `/dev/video2` capture + `/dev/video3` metadata) alongside the Venus codec
- [ ] `v4l2-ctl -d /dev/video2 --list-formats-ext` lists **YUYV 640x480**
      (the analyzer needs YUYV; MJPEG-only would be capture-only). Note the
      highest YUYV mode and its fps: ______________
- [ ] Camera 15–25 cm above the sheet, square + fastener in frame, not
      touching, lit from the side (shadow-free); `v4l2-ctl -d /dev/video2
      --stream-mmap --stream-count=30` streams 30 frames without USB errors
      in `dmesg`

### A2. Run

- [ ] `cd ~/PlatypusOne && ./tools/bringup/bench_session.sh --no-build`
      (`--no-build` if A0's build passed; the script now picks the capture
      node by capability and pins `--mode 640x480`)
- [ ] Step 5 prints the harness's own mode list for `/dev/video2` with a
      `yuyv` entry
- [ ] Step 6 writes `observations/scan-NNNN/` containing `observation.json`
      plus the frame artifact; `ok "wrote ..."` in the summary
- [ ] `observation.json` has a DERIVED width claim in mm for the fastener and
      an INFERRED classification with confidence. Compare the width to
      calipers: measured ______ mm vs caliper ______ mm (target ≤ 0.5 mm)
- [ ] If the analyzer rejects the scene (no reference / ambiguous / no
      subject) the record is capture-only and says so — fix lighting or
      spacing and rerun with `engineering_scout_capture --device /dev/video2
      --mode 640x480 --reference-mm 20 --out observations`
- [ ] Repeat for the second fastener; keep the best two `scan-*` directories

### A3. Close-out

- [ ] Paste the whole `bench_session.sh` output + both `observation.json`
      files into the session log; status-board steps 1, 3, 7, 8 → PASS/FAIL
- [ ] `scp` the two `scan-*` directories to the PC and put them under
      `docs/contest/` evidence (they are the Dream Lab demo's proof)
- [ ] Photograph the rig: hub + board + camera over the square, and the
      terminal showing the measurement. These are the write-up's hero shots
- [ ] Note what failed or was flaky for the Session B trigger work
      (`firmware/mcu_bridge` Ping/Pong; flash with the same `arduino-cli` flow
      — over Wi-Fi the upload target is the board itself, not `COM7`)

## 0. Bench safety / prerequisites

- [ ] Multimeter available and battery-checked
- [ ] ESD-sensitive work is performed on a grounded static-dissipative mat with
      a verified wrist strap/common-point ground (methodology in the private
      planning overlay)
- [ ] Loose ICs remain in shielding/dry packaging until the controlled bench and
      assembly window are ready; record MSL opening time where applicable
- [ ] USB-C supply verified 5.0–5.2 V open-circuit before first connection
- [ ] LiPo visually inspected (no puffing/damage); never charge unattended

## 1. UNO Q first boot (no peripherals)

- [ ] Board powers from USB-C; power LED on; no hot components (touch test after 60 s)
- [ ] Current draw at idle noted: ______ mA (expect < 500 mA)
- [ ] Linux boots; reachable via App Lab / USB gadget / UART console
- [ ] `uname -a` recorded; OS image version noted: ____________
- [ ] Wi-Fi joins network; `ping 8.8.8.8` OK
- [ ] microSD detected, formatted, mounted; write+read 100 MB test file
- [ ] Date/time syncs (NTP)
- [ ] Reboot 3× — boots cleanly every time
- [x] MCU side alive: stock LED matrix demo (or blink) runs — **2026-09-19: replaced by
      [`firmware/led_matrix_platypus`](../../firmware/led_matrix_platypus), flashed via
      `arduino-cli upload -p COM7 --fqbn arduino:zephyr:unoq`; platypus paddles, shaded**

## 2. PlatypusOS on target

- [ ] Cross-compile (or on-device build) of platypus_launcher succeeds
- [ ] `platypus_tests` passes on target: ____ / ____
- [ ] Launcher runs headless (HostSim fallback) without crash for 10 min
- [ ] MCU bridge: flash protocol firmware; `ping` round-trip < 50 ms over /dev/ttyRPMSG0
- [ ] GPIO loopback: digitalWrite pin → jumper → digitalRead pin reads back both levels
- [ ] analogRead on a divider reads mid-scale ±5%

## 3. Display + touch

- [ ] Display wired per schematic; supply rail correct **before** power-on
- [ ] Backlight lights; full-white and full-black frames show no dead rows/columns
- [ ] RGB565 test pattern: red/green/blue quadrants render correct colors (byte order!)
- [ ] Full-frame present ≥ 15 fps sustained; rate noted: ______ fps
- [ ] Touch: taps in all 4 corners + center report within ±5 px after calibration
- [ ] Launcher UI visible and navigable on the physical panel
- [ ] 30-min soak: no display corruption, no watchdog resets

## 4. Camera

- [ ] Enumerates as a V4L2 **capture** node (`v4l2-ctl --list-devices`). On the UNO Q
      `/dev/video0`/`video1` are the Qualcomm Venus codec, always present; a UVC
      webcam appears as the next pair (capture + metadata), typically `/dev/video2`
- [ ] 640×480 capture produces a well-exposed frame (save as bring-up artifact)
- [ ] Sustained streaming 30 s without USB dropouts; fps noted: ______
- [ ] ICamera driver: capture() returns valid Frame; startStream callback fires
- [ ] Focus acceptable at 10–30 cm working distance (ShadowScan range)

## 5. Sensors (per sensor: repeat for ToF, IMU, color)

- [ ] I²C address ACKs on bus scan (`i2cdetect` or MCU probe): addr ______
- [ ] Driver registers with ISensorHub; descriptor fields correct (unit, channels)
- [ ] Static sanity: ToF vs tape measure at 100/500/1000 mm within datasheet spec
- [ ] IMU: flat surface reads ~0° pitch/roll; 90° box test within ±1°
- [ ] Color: white paper reads ≈ white balance; red/green/blue cards separate cleanly
- [ ] 1000-sample streaming run: zero dropouts, no NaNs, rate matches requested Hz
- [ ] Inspection app displays the sensor live with no code changes (plugin proof)

## 6. Power & battery

- [ ] Charge board outputs 5.0–5.25 V under 1.5 A load (resistor or board load)
- [ ] System runs from battery; runtime from full charge: ______ h (target ≥ 2 h)
- [ ] Charging while operating works; charge LED states verified
- [ ] Brown-out test: Linux shuts down/recovers without SD corruption (3 cycles)
- [ ] No component exceeds warm-to-touch after 30 min battery operation

## 7. Audio & haptics

- [ ] beep() produces tone at commanded frequency (spot-check by ear/app)
- [ ] Volume 0 is silent; max volume undistorted
- [ ] Vibration motor pulses on command (if fitted)

## 8. Carrier PCB (per PCBWay spin)

- [ ] Visual inspection: no solder bridges, correct component orientation
- [ ] **Before first power:** continuity 5V↔GND open; 3V3↔GND open
- [ ] Rails at spec under load: 5 V = ______, 3V3 = ______
- [ ] Every connector pinout beeped out against the Fusion schematic
- [ ] All I²C devices enumerate through the carrier
- [ ] Buttons register; no ghosting
- [ ] Defects logged → schematic/layout fixes recorded for respin decision

## 9. Enclosure fit (per PCBWay/print iteration)

- [ ] All boards seat on their bosses; no flex on standoff tightening
- [ ] Display aperture aligned; touch usable to panel edges
- [ ] Camera and ToF windows unobstructed (check vignetting in a capture)
- [ ] Buttons actuate through the shell; positive click
- [ ] USB-C, SD accessible assembled; battery secured, wires strain-relieved
- [ ] Assembled weight ______ g; drop-check from 30 cm onto desk (unit survives)

## 10. System integration soak (pre-submission gate)

- [ ] Fully assembled unit: 2-hour continuous run on battery — launcher +
      every app exercised — zero crashes/resets
- [ ] ShadowScan end-to-end: physical object → mesh export → file opens on PC
- [ ] Measurement app vs reference tools: distance ±5 mm, angle ±1°, recorded in docs
- [ ] Cold boot to launcher time: ______ s
- [ ] All contest video/photo shots captured (device in action, in hand)
- [ ] Final BOM reconciled: every physical part appears in BOM.md with real price/source
- [ ] Submission checklist in the contest snapshot doc 100% ticked


## Session B — Dream Lab first end-to-end pass (2026-09-28)

**Goal:** one uninterrupted physical workflow using the hardware currently in hand:

`touch trigger → webcam capture → reference + fastener analysis → measurement/classification → result on Waveshare → saved observation`

This session is allowed to be ugly. It is not allowed to expand in scope.

### B0. Before power

- [ ] UNO Q powered down before attaching/removing the Media Carrier or DSI FFC.
- [ ] Verify FFC orientation against the carrier/display markings; photograph both ends before assembly.
- [ ] Media Carrier seated; Waveshare 5inch DSI LCD Rev2.2 (800×480) connected.
- [x] Printed 20 mm reference is ready and scale-checked.
- [ ] One bolt and one nut are ready; calipers available for truth measurements.
- [ ] Webcam + powered/PD-capable USB path ready.

### B1. Display proof-of-life

The successful panel installation is recorded in [DSI_BRINGUP.md](DSI_BRINGUP.md).
The former `display=5-dsi-touch-a` command in this checklist configured the
wrong 720×1280 panel and has been removed. The fitted panel uses the
`waveshare-800x480` definition from the recorded driver/overlay project.
Do not reinstall the panel or upgrade its OS merely to replay the working rig.

Record the current configuration on the UNO Q:

```bash
uname -a
ls /sys/class/drm
cat /sys/class/drm/card*-DSI-*/status
cat /sys/class/drm/card*-DSI-*/modes
```

After inspecting the current configuration:

- [ ] Display shows a full frame with no persistent flicker/clipping.
- [ ] Touch enumerates and responds.
- [ ] Record `uname -a`, `ls /sys/class/drm`, and relevant `dmesg` panel/touch lines.
- [ ] Confirm intended landscape/portrait orientation is usable.
- [ ] Take one photo of the working display.

**If DSI fails:** spend at most 30 minutes on configuration/cable/orientation. Then continue the Scout pipeline headless/SSH and keep display debugging as a separate blocker. Do not lose the whole Dream Lab session to the panel.

### B2. Sync and build

Follow [the pinned UNO Q build/launch sequence](UNO_Q_SCOUT_BASELINE.md).
It records the SHA, builds Linux tools, runs asserts and the synthetic battery,
then installs/launches the kiosk. It also keeps synthetic records out of the
physical observation directory.

- [ ] Build passes on the UNO Q.
- [ ] Tests pass.
- [ ] Record the commit SHA: `git rev-parse HEAD`.

### B3. Camera sanity

```bash
v4l2-ctl --list-devices
v4l2-ctl -d /dev/video2 --list-formats-ext
v4l2-ctl -d /dev/video2 --stream-mmap --stream-count=30
```

- [ ] Webcam capture node confirmed (do not assume `/dev/video2` if enumeration changed).
- [ ] 640×480 YUYV available.
- [ ] 30-frame stream completes without USB errors.

### B4. First real Scout run

Place one fastener beside the 20 mm reference under normal room lighting.

Preferred command:

```bash
engineering_scout_capture --device /dev/video2 \
  --mode 640x480 \
  --reference-mm 20 \
  --out observations
```

If the executable lives under the build tree, use the built path shown by the CMake output / existing bench script.

Acceptance for the **first run**:
- [ ] source frame saved
- [ ] reference detected
- [ ] subject detected
- [ ] at least one physical dimension reported in mm
- [ ] classification/nominal result or honest unresolved state
- [ ] observation JSON saved
- [ ] compare one dimension against calipers and record the error

**Do not stop to improve calibration math unless the workflow cannot complete at all.**

### B5. Touch trigger + result display

- [ ] Wire the existing Scout action to a touch press on the Waveshare.
- [ ] One press initiates capture/analysis.
- [ ] Result card renders on the Waveshare.
- [ ] Saved observation path is visible/logged.

A dedicated hardware trigger is explicitly **not required tonight**.

### B6. Robustness pass

Repeat the same object under:
1. normal desk lighting
2. uneven overhead lighting
3. moderate shadow
4. one rotated placement

For each capture record:
- success/refusal
- measured dimension + caliper truth
- classification
- failure/refusal reason
- whether retry guidance was useful

The goal is not perfect accuracy. The goal is a useful result or an honest, actionable refusal.

### B7. Evidence before teardown

Before changing the rig:
- [ ] copy `scan-*` observation directories off the UNO Q
- [ ] photograph the full rig
- [ ] photograph/video the touch-triggered run
- [ ] save terminal output / commit SHA
- [ ] note the best run and worst run
- [ ] record one blocker and the single next action

### Stop conditions / scope guard

Do **not** add tonight:
- ToF
- IMU
- radar
- thermal
- carrier PCB work
- enclosure work
- generalized object recognition
- ShadowScan / mesh generation
- dedicated MCU trigger

Those can only enter after the physical Scout loop above works or a specific blocker proves one is necessary.
