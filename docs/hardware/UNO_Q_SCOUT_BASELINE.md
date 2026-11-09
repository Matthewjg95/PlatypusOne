# UNO Q Scout baseline — build, launch, preserve

Source pin: **ea5e82a36c21de395f347425b8bfa5a7e1638fc2** (merged #25/#28).
This is the post-submission **reproduction candidate**: host build/tests and
synthetic kiosk capture passed; this exact combined SHA has not been replayed
on the physical UNO Q in this stabilization task. It is not the final
Dream Lab tested SHA. A recorded native pass below promotes it to a known-good
physical baseline.

Hardware: UNO Q + UNO Media Carrier + **Waveshare 5inch DSI LCD Rev2.2,
800×480** + powered USB-C hub + YUYV webcam + verified 20 mm reference.
The working panel/overlay installation is described in [DSI_BRINGUP.md](DSI_BRINGUP.md).
No panel reinstall, kernel upgrade or MCU flash is part of this sequence.
The accepted trigger is touch. This source supports single captures; session
FINISH/export, #35's classifier and tilt correction are outside this pin.

First [preserve the submission-night evidence](../contest/evidence/dreamlab-2026-09-30/README.md)
from the existing contest checkout. The new checkout below leaves that
checkout and its observation directories available. Run on the UNO Q over
SSH/Wi-Fi; the kiosk service switches USB-C to host mode for the webcam.
Install `build-essential cmake ninja-build v4l-utils` if not already present.

## One command sequence

Run these blocks in the same Bash session. Every run gets a fresh log/synthetic
directory; synthetic evidence never goes into the physical `observations/`.

```bash
set -euo pipefail
BASELINE_SHA=ea5e82a36c21de395f347425b8bfa5a7e1638fc2
BASELINE_DIR="$HOME/PlatypusOne-baseline-20261001"
if [ ! -d "$BASELINE_DIR/.git" ]; then
  git clone https://github.com/Matthewjg95/PlatypusOne.git "$BASELINE_DIR"
fi
cd "$BASELINE_DIR"
test -z "$(git status --porcelain)" || { echo "baseline checkout has local changes" >&2; exit 1; }
git fetch origin main
git checkout --detach "$BASELINE_SHA"
test "$(git rev-parse HEAD)" = "$BASELINE_SHA"
RUN_DIR="$(mktemp -d "$HOME/platypus-baseline-run.XXXXXXXX")"
git rev-parse HEAD > "$RUN_DIR/built-source-sha.txt"
git status --porcelain > "$RUN_DIR/checkout-state.txt"
uname -a | tee "$RUN_DIR/uname.txt"
g++ --version > "$RUN_DIR/compiler.txt"
cmake --version > "$RUN_DIR/cmake-version.txt"
date -u > "$RUN_DIR/collection-time.txt"
ls /sys/class/drm > "$RUN_DIR/drm-nodes.txt"

cmake -S . -B build-bench -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DPLATYPUS_TARGET_HOST=ON -DPLATYPUS_BUILD_APPS=ON \
  -DPLATYPUS_BUILD_TESTS=ON 2>&1 | tee "$RUN_DIR/configure.log"
cmake --build build-bench --parallel 4 2>&1 | tee "$RUN_DIR/build.log"
ctest --test-dir build-bench --output-on-failure 2>&1 | tee "$RUN_DIR/tests.log"
./build-bench/tools/scout_validation/scout_validation "$RUN_DIR/synthetic-validation.md"
sha256sum build-bench/tools/scout_kiosk/scout_kiosk > "$RUN_DIR/kiosk-binary.sha256"

mkdir "$RUN_DIR/synthetic-ui"
./build-bench/tools/scout_kiosk/scout_kiosk --fake \
  --offscreen "$RUN_DIR/synthetic-ui" \
  --out "$RUN_DIR/synthetic-observations" 2>&1 | tee "$RUN_DIR/synthetic-kiosk.log"
test -s "$RUN_DIR/synthetic-ui/preview.ppm"
test -s "$RUN_DIR/synthetic-ui/card.ppm"
test -s "$RUN_DIR/synthetic-observations/scan-0001/observation.json"
```

`PLATYPUS_TARGET_HOST=ON` matches CI and the established default build.
The Linux `scout_kiosk` still uses the real V4L2 and DRM drivers unless
`--fake` / `--offscreen` are explicitly supplied. Expected synthetic output:
`Saved scan-0001`, **21/21** validation cases, 800×480 preview/card PPMs and a
640×480 YUYV frame. Its JSON must say `camera: synthetic`; this is not a bench pass.

Continue only after the build/test block succeeds:

```bash
systemctl get-default > "$RUN_DIR/boot-target-before.txt"
if systemctl cat platypus-kiosk.service >/dev/null 2>&1; then
  sudo systemctl stop platypus-kiosk.service
fi
sudo sh tools/kiosk/install.sh
sudo platypus-mode kiosk --once
platypus-mode status | tee "$RUN_DIR/mode.txt"
sudo journalctl -u platypus-kiosk.service -b --no-pager > "$RUN_DIR/kiosk-startup.log"
v4l2-ctl --list-devices | tee "$RUN_DIR/camera-devices.txt"
printf 'Run evidence directory: %s\n' "$RUN_DIR"
```

Stopping an existing kiosk before installation matters: `systemctl start`
does not replace an already-running old binary. `--once` keeps the **existing**
boot configuration, including a previously enabled persistent kiosk. Use
`platypus-mode status` to inspect it; this flag does not promise a desktop reboot.
The unit grants video/input groups, stops the competing display manager,
selects the camera by YUYV capability and prefers 640×480. Do not hard-code
`/dev/video2`; codec and metadata nodes can enumerate first.

## Physical pass and close-out

On glass, require a live preview, a logged physical camera identity and touch
response. Put a known part beside the complete reference and tap CAPTURE;
photograph the result, record the new scan ID, caliper measurand, lighting,
focus and placement. Hide the reference for a second capture: require useful
retry guidance and a saved capture-only record. Neither successful software
checks nor a plausible nominal label proves dimensional accuracy. The
September 30 screw is 1/4-20 UNC; this pin's metric-only classifier can mislabel it.

Then stop the capture process before copying complete records and restore
the desktop (now and on boot):

```bash
sudo platypus-mode desktop
sudo journalctl -u platypus-kiosk.service -b --no-pager > "$RUN_DIR/kiosk-complete.log"
test -d observations
cp -a observations "$RUN_DIR/physical-observations"
git show origin/main:docs/hardware/SCOUT_BASELINE_RUN_RECORD.md > "$RUN_DIR/run-record.md"
```

The template is read from fetched main without changing the pinned runtime
checkout. Fill actual results and add photos/video references before
hashing/copying the bundle as described in the evidence index.

| Symptom | Check / action |
|---|---|
| DRM `Busy` | Check `systemctl status display-manager.service platypus-kiosk.service`; use the mode switch to give the kiosk DRM master. |
| No connected DSI display | Recheck the photographed FFC seating/orientation and existing `waveshare-800x480` installation; record blocker, do not install the 5-dsi-touch-a profile. |
| Waiting for webcam | Inspect `/sys/class/usb_role/*/role`, powered hub and `v4l2-ctl --list-devices`; the kiosk waits and retries rather than inventing a capture. |
| No reference / no subject | Keep the entire square and part separate/in frame; check focus/light. Preserve the refusal; do not change thresholds. |
| Numbers over-read | Record the caliper comparison and frame; do not tune tilt, height or shadow correction without preserved new physical evidence. |
