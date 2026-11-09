# shellcheck shell=sh disable=SC2034  # constants are used by the sourcing scripts
# Shared helpers for tools/compact_display/*.sh. POSIX sh, runs on the UNO Q.
#
# Paths match dcuartielles/uno_q_dsi_displays lib/common.sh at the pinned
# commit below, because the proven 5" install (docs/hardware/DSI_BRINGUP.md)
# was made with that repository and these scripts sit on top of it.

UPSTREAM_URL=https://github.com/dcuartielles/uno_q_dsi_displays.git
UPSTREAM_COMMIT=d633636527a213e884dae05468ffd355d12974f1

DTB_DIR=/boot/efi/dtb/qcom
BASE_DTB="$DTB_DIR/qrb2210-arduino-imola-base.dtb"
CARRIER_DTBO="$DTB_DIR/qrb2210-arduino-imola-carrier-media.dtbo"
# The only display slot a described panel can use (arduino-linux-config
# hardcodes its option names), shared with the proven 5" overlay.
SLOT_DTBO="$DTB_DIR/qrb2210-arduino-imola-carrier-media-panel-5in_touch_a-dsi.dtbo"
CARRIER_DISPLAY_OPTION="5-dsi-touch-a"

STATE_DIR=/var/lib/platypus-compact-display
PROVEN_5IN_PANEL=panels/waveshare-800x480.panel   # relative to the upstream checkout

say()  { printf '%s\n' "$*"; }
step() { printf '\n==> %s\n' "$*"; }
ok()   { printf '  ok  %s\n' "$*"; }
warn() { printf '  !!  %s\n' "$*"; }
die()  { printf ' ERROR %s\n' "$*" >&2; exit 1; }

have_cmd() { command -v "$1" >/dev/null 2>&1; }

need_root() { [ "$(id -u)" -eq 0 ] || die "run with sudo"; }

is_uno_q() {
    tr -d '\0' < /proc/device-tree/model 2>/dev/null | grep -qiE 'unoq|uno q|imola'
}

# The carrier's I2C (DISPLAY pins 20/21) hangs off the Qualcomm CCI controller;
# its bus number is not stable across boots. Same search as upstream
# scripts/detect-panel.sh: adapter name first, then the carrier's own GPIO
# expander at 0x26.
find_cci_bus() {
    for d in /sys/class/i2c-adapter/i2c-*; do
        [ -e "$d" ] || continue
        case "$(cat "$d/name" 2>/dev/null)" in
            *cci*|*CCI*) printf '%s' "${d##*/i2c-}"; return 0 ;;
        esac
    done
    for n in 0 1 2 3 4 5; do
        [ -e "/dev/i2c-$n" ] || continue
        if i2ctransfer -y -f "$n" r1@0x26 >/dev/null 2>&1; then
            printf '%s' "$n"; return 0
        fi
    done
    return 1
}

# GT911 product-ID read: register 0x8140, 4 bytes, ASCII "911\0".
# Prints the bytes, or nothing if the address does not answer.
gt911_product_id() {
    i2ctransfer -y -f "$1" "w2@$2" 0x81 0x40 r4 2>/dev/null
}

upstream_commit() { git -C "$1" rev-parse HEAD 2>/dev/null; }
