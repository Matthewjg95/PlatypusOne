#!/bin/sh
# csi_bench.sh — comparable evidence for the Arducam IMX219 bench-off (#40).
#
#   tools/csi_bench/csi_bench.sh <SKU> <probe|capture|all> ["notes"]
#
#   SKU    B0394 | B0393 | B0390 (the module actually connected)
#   probe  identity, carrier config, kernel log, media graph, libcamera view,
#          sensor/lens controls, display coexistence — no frames
#   capture  probe + frames through libcamera (`cam`), formats as available
#   notes  free text recorded verbatim: connector, cable orientation, focus
#          state, lighting, working distance, what is in the scene
#
# Writes ~/PlatypusOne/evidence/csi/<SKU>/<UTC stamp>-<test>/ (or under
# $CSI_EVIDENCE_ROOT) with a SHA256 manifest. Runs as the normal user; changing the carrier configuration is a
# separate, deliberate sudo step (docs/hardware/CSI_CAMERA_BENCH.md).
#
# Records facts only. A missing sensor, a failed capture or an absent focus
# control is written down, never retried into a pass.
set -u

SKU=${1:-}
TEST=${2:-}
NOTES=${3:-}
case "$SKU" in B0394 | B0393 | B0390) ;; *)
    echo "usage: $0 <B0394|B0393|B0390> <probe|capture|all> [notes]" >&2
    exit 2
    ;;
esac
case "$TEST" in probe | capture | all) ;; *)
    echo "usage: $0 <B0394|B0393|B0390> <probe|capture|all> [notes]" >&2
    exit 2
    ;;
esac

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO=$(CDPATH= cd -- "$HERE/../.." && pwd)
STAMP=$(date -u +%Y%m%dT%H%M%SZ)
OUT="${CSI_EVIDENCE_ROOT:-$REPO/evidence/csi}/$SKU/$STAMP-$TEST"
mkdir -p "$OUT"
cd "$OUT" || exit 1

say() { printf '%s\n' "$*"; }
run() { # run <file> <command...>: stdout+stderr and exit status into <file>
    f=$1
    shift
    { printf '$ %s\n' "$*"; "$@" 2>&1; printf '[exit %s]\n' "$?"; } >>"$f"
}

say "evidence -> $OUT"
printf '%s\n' "$NOTES" >notes.txt

# --- identity: what ran, on what ---------------------------------------
{
    say "sku: $SKU"
    say "test: $TEST"
    say "utc: $STAMP"
    say "boot: $(cat /proc/sys/kernel/random/boot_id) since $(uptime -s)"
    say "kernel: $(uname -a)"
    say "os: $(. /etc/os-release && printf '%s' "$PRETTY_NAME")"
    say "repo_sha: $(git -C "$REPO" rev-parse HEAD 2>/dev/null)"
    say "repo_dirty_files: $(git -C "$REPO" status --short --untracked-files=no 2>/dev/null | wc -l)"
} >system.txt

run carrier.json arduino-linux-config --format json carrier show

# --- kernel view: did the sensor (and a focus lens, for B0393) probe? -----
journalctl -k -b --no-pager 2>/dev/null |
    grep -i -E 'imx219|camss|csiphy|csid|vfe|cci|dw9714|dw9807|ak7375|vcm|lens' >kernel.log
grep -c -i 'imx219' kernel.log >kernel_imx219_lines.txt

# The chip-ID read is the real enumeration test: a sensor bound to the
# imx219 driver answered on I2C, even if camss has not registered its media
# graph yet (camss waits for every enabled port's sensor).
BOUND=""
for d in /sys/bus/i2c/devices/*-0010; do
    [ -e "$d" ] || continue
    drv=$(basename "$(readlink "$d/driver" 2>/dev/null)" 2>/dev/null)
    say "$(basename "$d"): ${drv:-unbound}" >>i2c_sensors.txt
    [ "$drv" = imx219 ] && BOUND="$BOUND $(basename "$d")"
done

run media.txt media-ctl -d /dev/media0 -p
SENSOR=""
LENS=""
for sd in /sys/class/video4linux/v4l-subdev*; do
    name=$(cat "$sd/name" 2>/dev/null)
    case "$name" in
        *imx219*) SENSOR="/dev/$(basename "$sd")" ;;
        *dw97* | *dw98* | *ak73* | *vcm* | *lens* | *focus*) LENS="/dev/$(basename "$sd")" ;;
    esac
done
say "sensor_subdev: ${SENSOR:-none}" >>system.txt
say "imx219_bound_i2c:${BOUND:- none}" >>system.txt
say "lens_subdev: ${LENS:-none}" >>system.txt
if [ -n "$SENSOR" ]; then
    run sensor.txt v4l2-ctl -d "$SENSOR" --list-subdev-mbus-codes
    run sensor.txt v4l2-ctl -d "$SENSOR" --list-ctrls
fi
# Focus state is part of the optics: record it whenever a lens control exists.
if [ -n "$LENS" ]; then
    run lens.txt v4l2-ctl -d "$LENS" --list-ctrls
fi

# --- libcamera view --------------------------------------------------------
run libcamera.txt timeout 20 cam --list
run libcamera.txt timeout 20 cam -c 1 --list-properties
run libcamera.txt timeout 20 cam -c 1 --info

# --- coexistence: display and touch must survive camera bring-up -------------
{
    say "mode: $(platypus-mode status 2>&1 | tr '\n' ' ')"
    say "kiosk: $(systemctl is-active platypus-kiosk 2>&1)"
    say "panel_recover: $(journalctl -u uno-q-dsi-panel-recover -b --no-pager 2>/dev/null | grep -E 'display is up|touch is up|FAILED' | tr '\n' ' ')"
    say "drm: $(ls /dev/dri 2>&1 | tr '\n' ' ')"
    for ev in /sys/class/input/event*; do
        say "input $(basename "$ev"): $(cat "$ev/device/name" 2>/dev/null)"
    done
} >coexistence.txt

# --- power: recorded only if the board exposes it ----------------------------
{
    found=0
    for ps in /sys/class/power_supply/*; do
        [ -d "$ps" ] || continue
        found=1
        say "== $(basename "$ps")"
        for k in voltage_now current_now power_now online type; do
            [ -r "$ps/$k" ] && say "$k: $(cat "$ps/$k")"
        done
    done
    [ "$found" = 1 ] || say "no power_supply telemetry on this board: measure with an inline USB-C meter and record in notes"
} >power.txt

# --- frames ----------------------------------------------------------------
if [ "$TEST" != probe ]; then
    if [ -z "$SENSOR" ]; then
        say "no imx219 sensor subdev: capture skipped" >capture.txt
    else
        # libcamera's simple pipeline delivers ABGR8888 through its software
        # ISP. 1280x720 fits the board's 32 MB CMA pool (1640x1232 and up fail
        # with "dma-heap allocation failure", 2026-10-04). Let auto-exposure
        # settle over 30 frames, keep the last three plus a viewable PPM.
        mkdir -p settle
        run capture.txt timeout 40 cam -c 1 --capture=30             --stream role=viewfinder,width=1280,height=720 --file=settle/frame-#.bin
        for f in $(ls settle/frame-* 2>/dev/null | tail -3); do mv "$f" .; done
        rm -rf settle
        run sensor_after.txt v4l2-ctl -d "$SENSOR" -C exposure -C analogue_gain
        last=$(ls frame-* 2>/dev/null | tail -1)
        if [ -n "$last" ]; then
            python3 - "$last" 1280 720 <<'PY'
import sys
path, w, h = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
data = open(path, "rb").read()
stride = len(data) // h
rgb = bytearray(w * h * 3)
for y in range(h):
    row = data[y * stride : y * stride + w * 4]
    out = rgb[y * w * 3 : (y + 1) * w * 3]
    out[0::3], out[1::3], out[2::3] = row[0::4], row[1::4], row[2::4]
    rgb[y * w * 3 : (y + 1) * w * 3] = out
with open("preview.ppm", "wb") as f:
    f.write(b"P6 %d %d 255" % (w, h) + bytes([10]))
    f.write(rgb)
PY
        fi
    fi
fi

# --- manifest ---------------------------------------------------------------
python3 - "$OUT" "$SKU" "$TEST" "$STAMP" <<'PY'
import hashlib, json, os, sys
out, sku, test, stamp = sys.argv[1:5]
files = []
for name in sorted(os.listdir(out)):
    path = os.path.join(out, name)
    if name == "manifest.json" or not os.path.isfile(path):
        continue
    data = open(path, "rb").read()
    files.append({"file": name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
manifest = {
    "schema_version": 1,
    "kind": "platypus.csi_bench/0.1",
    "sku": sku,
    "test": test,
    "utc": stamp,
    "evidence_type": "on_device_raw",
    "files": files,
}
with open(os.path.join(out, "manifest.json"), "w", newline="\n") as f:
    json.dump(manifest, f, indent=2)
    f.write("\n")
PY
sync

say "chip-id answered (imx219 bound):${BOUND:- none}"
[ -n "$BOUND" ] && [ -z "$SENSOR" ] && say "note: sensor answered but no media graph - is another enabled CSI port empty?"
say "sensor: ${SENSOR:-NOT FOUND}   lens: ${LENS:-none}   frames: $(ls frame-* raw-* 2>/dev/null | wc -l)"
say "done: $OUT"
