#!/bin/sh
# Install a controller-less compact DSI panel on UNO Q + UNO Media Carrier.
#
#   sudo ./install_panel.sh <uno_q_dsi_displays checkout> [panel] [--touch-addr auto|0x14|0x5d]
#   ./install_panel.sh --dry-run [panel] [out-dir]      # generate + compile only, no board needed
#
# Default panel: panels/waveshare-3in5-dsi-h.panel (the leading candidate).
#
# NOT YET RUN ON HARDWARE. This script prepares a bench trial; it is not
# evidence that the panel works. Acceptance is bench_accept.sh.
#
# What it does, in order (each step stops the script on failure):
#   1. pre-flight: UNO Q, carrier overlays, tools, upstream at the pinned commit
#   2. backs up everything it will change into $STATE_DIR/backup-<UTC time>/
#      (the proven 5" slot overlay, carrier config, module inventory)
#   3. picks the GT911 address: reads the product ID at 0x14 and 0x5d on the
#      carrier I2C bus, falls back to the .panel default if neither answers
#   4. generates our overlay (gen_overlay.py), compiles it, and test-composes it
#      against the board's own base DTB + carrier overlay with fdtoverlay
#   5. builds panel-simple with this panel's descriptor using the upstream
#      scripts that built the proven 5" (20-build-drivers.sh, 25-install-dkms.sh)
#   6. installs the upstream Goodix CCI read-size fix (17-install-goodix-fix.sh)
#   7. installs the overlay in the 5" slot and enables the carrier display
#
# It does NOT reboot. Undo with restore_5in.sh, which reinstalls the proven
# 5" configuration with the same upstream installer that made it.
set -e
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
. "$HERE/lib/common.sh"

DRY_RUN=0
TOUCH_SEL=auto
POSITIONAL=""
while [ $# -gt 0 ]; do
    case "$1" in
        --dry-run) DRY_RUN=1 ;;
        --touch-addr) shift; TOUCH_SEL=${1:?--touch-addr needs auto, 0x14 or 0x5d} ;;
        -h|--help) sed -n '2,27p' "$0"; exit 0 ;;
        *) POSITIONAL="$POSITIONAL $1" ;;
    esac
    shift
done
# shellcheck disable=SC2086
set -- $POSITIONAL

case "$TOUCH_SEL" in auto|0x14|0x5d) ;; *) die "--touch-addr must be auto, 0x14 or 0x5d" ;; esac

if [ "$DRY_RUN" = 1 ]; then
    PANEL=${1:-$HERE/panels/waveshare-3in5-dsi-h.panel}
    OUT=${2:-$(mktemp -d)}
    mkdir -p "$OUT"
    step "Dry run: generate + compile $(basename "$PANEL")"
    python3 "$HERE/gen_overlay.py" "$PANEL" "$OUT/overlay.dts"
    have_cmd dtc || die "dtc not installed (apt install device-tree-compiler)"
    dtc -@ -q -I dts -O dtb -o "$OUT/overlay.dtbo" "$OUT/overlay.dts"
    ok "$OUT/overlay.dtbo"
    exit 0
fi

UPSTREAM=${1:?usage: sudo $0 <uno_q_dsi_displays checkout> [panel] [--touch-addr auto|0x14|0x5d]}
PANEL=${2:-$HERE/panels/waveshare-3in5-dsi-h.panel}
UPSTREAM=$(CDPATH='' cd -- "$UPSTREAM" && pwd)
PANEL=$(CDPATH='' cd -- "$(dirname -- "$PANEL")" && pwd)/$(basename -- "$PANEL")

need_root

# ------------------------------------------------------------- 1 preflight --
step "Pre-flight"
is_uno_q || die "this does not look like an UNO Q: $(tr -d '\0' < /proc/device-tree/model 2>/dev/null)"
[ -f "$BASE_DTB" ] && [ -f "$CARRIER_DTBO" ] || die "Media Carrier overlays missing - this image predates the carrier (DSI_BRINGUP.md blocker 2)"
[ -f "$SLOT_DTBO" ] || die "5-inch slot overlay missing: $SLOT_DTBO"
for c in python3 dtc fdtoverlay i2ctransfer arduino-linux-config git; do
    have_cmd "$c" || die "$c not found"
done
[ -f "$UPSTREAM/lib/common.sh" ] && [ -f "$UPSTREAM/$PROVEN_5IN_PANEL" ] \
    || die "$UPSTREAM is not a uno_q_dsi_displays checkout (git clone $UPSTREAM_URL)"
GOT=$(upstream_commit "$UPSTREAM")
if [ "$GOT" != "$UPSTREAM_COMMIT" ]; then
    [ "${ALLOW_UPSTREAM_DRIFT:-0}" = 1 ] \
        || die "upstream is at ${GOT:-?}, these scripts were written against $UPSTREAM_COMMIT
    (git -C $UPSTREAM checkout $UPSTREAM_COMMIT, or ALLOW_UPSTREAM_DRIFT=1 to proceed)"
    warn "upstream drift accepted: $GOT"
fi
[ -d "/lib/modules/$(uname -r)/build" ] || die "no kernel headers for $(uname -r)"
ok "UNO Q, carrier overlays, tools, upstream $GOT"

# --------------------------------------------------------------- 2 backup --
TS=$(date -u +%Y%m%dT%H%M%SZ)
BACKUP="$STATE_DIR/backup-$TS"
step "Backing up to $BACKUP"
mkdir -p "$BACKUP"
cp -a "$SLOT_DTBO" "$BACKUP/"
sha256sum "$SLOT_DTBO" > "$BACKUP/slot.sha256"
arduino-linux-config carrier show > "$BACKUP/carrier-show.txt" 2>&1 || true
uname -a > "$BACKUP/uname.txt"
find "/lib/modules/$(uname -r)" \( -name 'panel-simple.ko*' -o -name 'goodix_ts.ko*' \
    -o -name 'edt-ft5x06.ko*' -o -name 'rpi-panel-attiny-regulator.ko*' \) \
    -exec sha256sum {} + > "$BACKUP/modules.sha256" 2>/dev/null || true
dkms status > "$BACKUP/dkms-status.txt" 2>&1 || true
# The module binaries the proven 5" is running, for restore_5in.sh --offline.
MODDIR="/lib/modules/$(uname -r)"
for m in "$MODDIR"/updates/dkms/panel-simple.ko* "$MODDIR"/kernel/drivers/gpu/drm/panel/panel-simple.ko*; do
    [ -f "$m" ] || continue
    mkdir -p "$BACKUP/modules$(dirname "$m")"
    cp -a "$m" "$BACKUP/modules$m"
done
[ -f /var/lib/uno-q-dsi-panel/installed.txt ] && cp /var/lib/uno-q-dsi-panel/installed.txt "$BACKUP/upstream-installed.txt"
[ -e "/lib/modules/$(uname -r)/updates/goodix_ts.ko" ] && echo present > "$BACKUP/goodix-fix-was-present"
ln -sfn "$BACKUP" "$STATE_DIR/last-backup"
ok "slot overlay $(cut -c1-12 "$BACKUP/slot.sha256") saved"

# ---------------------------------------------------------- 3 touch addr --
step "GT911 address"
# shellcheck source=panels/waveshare-3in5-dsi-h.panel
. "$PANEL"
ADDR=$GOODIX_ADDR
if [ "$TOUCH_SEL" = auto ]; then
    BUS=$(find_cci_bus || true)
    if [ -n "$BUS" ]; then
        for a in 0x14 0x5d; do
            ID=$(gt911_product_id "$BUS" "$a" || true)
            say "  bus $BUS $a: ${ID:-no answer}"
            case "$ID" in "0x39 0x31 0x31"*) ADDR=$a ;; esac
        done
    else
        warn "carrier I2C bus not found (carrier disabled?) - keeping $ADDR from the .panel"
    fi
else
    ADDR=$TOUCH_SEL
fi
ok "touchscreen at $ADDR"

WORK="$STATE_DIR/build-$TS"
mkdir -p "$WORK"
EFFECTIVE="$WORK/$(basename "$PANEL")"
{ cat "$PANEL"; printf '\n# install_panel.sh %s\nGOODIX_ADDR="%s"\n' "$TS" "$ADDR"; } > "$EFFECTIVE"

# ------------------------------------------------------ 4 overlay + compose --
step "Generating and test-composing the overlay"
python3 "$HERE/gen_overlay.py" "$EFFECTIVE" "$WORK/overlay.dts"
dtc -@ -q -I dts -O dtb -o "$WORK/overlay.dtbo" "$WORK/overlay.dts"
fdtoverlay -i "$BASE_DTB" -o "$WORK/test.dtb" "$CARRIER_DTBO" "$WORK/overlay.dtbo" \
    || die "overlay does not apply to this board's base DTB - nothing has been changed yet"
ok "composes against $(basename "$BASE_DTB") + $(basename "$CARRIER_DTBO")"

# ---------------------------------------------------- 5/6 drivers (upstream) --
step "Building panel-simple with the $PANEL_ID descriptor (upstream scripts)"
sh "$UPSTREAM/scripts/20-build-drivers.sh" "$EFFECTIVE"
sh "$UPSTREAM/scripts/25-install-dkms.sh" "$EFFECTIVE"
step "Goodix CCI read-size fix (upstream)"
sh "$UPSTREAM/scripts/17-install-goodix-fix.sh" "$EFFECTIVE"

# ----------------------------------------------------- 7 overlay + enable --
step "Installing the overlay in the 5-inch slot"
cp "$WORK/overlay.dtbo" "$SLOT_DTBO"
arduino-linux-config carrier enable media-carrier "display=$CARRIER_DISPLAY_OPTION"

cat > "$STATE_DIR/installed.env" <<EOF
PANEL_ID="$PANEL_ID"
PANEL_FILE="$EFFECTIVE"
GOODIX_ADDR="$ADDR"
HACTIVE="$HACTIVE"
VACTIVE="$VACTIVE"
BACKUP="$BACKUP"
UPSTREAM="$UPSTREAM"
INSTALLED_UTC="$TS"
EOF
ok "state: $STATE_DIR/installed.env"

step "Done - nothing is verified yet"
say "  sudo reboot"
say "  sudo $HERE/bench_accept.sh             # stages D3..D8"
say "Undo:  sudo $HERE/restore_5in.sh && sudo reboot"
