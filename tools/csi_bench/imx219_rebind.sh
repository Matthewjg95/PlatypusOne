#!/bin/sh
# imx219_rebind.sh — bind IMX219 sensors whose boot-time probe failed.
#
# On the UNO Q + Media Carrier with the Waveshare panel, the imx219 probe runs
# a few seconds into boot, while CCI bus 0 (PCA9555 camera-power expander,
# panel ATTINY, touch) is still in the flaky window uno-q-dsi-panel-recover
# exists for. The expander write that powers the camera times out and the
# driver never retries (2026-10-04, docs/hardware/CSI_CAMERA_BENCH.md). Run
# after the panel recovery, this binds every unbound IMX219 node, retrying
# while the bus settles. Root only; installed by install_imx219_rebind.sh.
set -u

DRIVER=/sys/bus/i2c/drivers/imx219
TRIES=${IMX219_REBIND_TRIES:-10}
DELAY=${IMX219_REBIND_DELAY:-3}

log() { printf 'imx219-rebind: %s\n' "$*"; }

[ -d "$DRIVER" ] || modprobe imx219 2>/dev/null
[ -d "$DRIVER" ] || { log "imx219 driver not loaded; nothing to do"; exit 0; }

unbound() {
    for d in /sys/bus/i2c/devices/*-0010; do
        [ -e "$d" ] || continue
        grep -q 'sony,imx219' "$d/modalias" 2>/dev/null || continue
        [ -e "$d/driver" ] || basename "$d"
    done
}

n=0
while :; do
    pending=$(unbound)
    [ -z "$pending" ] && { log "all IMX219 sensors bound"; exit 0; }
    n=$((n + 1))
    for dev in $pending; do
        if echo "$dev" >"$DRIVER/bind" 2>/dev/null; then
            log "$dev bound (attempt $n)"
        else
            log "$dev not bound yet (attempt $n)"
        fi
    done
    [ "$n" -ge "$TRIES" ] && break
    sleep "$DELAY"
done
log "still unbound after $n attempts: $(unbound | tr '\n' ' ')"
# An enabled port with no camera fitted never binds: not a failure.
exit 0
