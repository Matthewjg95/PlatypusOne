#!/bin/sh
# Compact-display bench acceptance: stages D0..D7 with PASS/FAIL evidence.
#
#   sudo ./bench_accept.sh [--stage D3] [--boot-kind cold|warm] [--evidence DIR] [--non-interactive]
#
# Procedure, pass criteria and what each stage proves:
#   docs/hardware/COMPACT_DISPLAY_BENCH_ACCEPTANCE.md
#
#   D0 connect     operator: power off, cable orientation, 5 V/3 A supply, photo
#   D1 identify    carrier I2C: GT911 product ID "911", nothing at 0x45
#   D2 boot        our overlay is the live DT, panel-simple bound, DSI/touch dmesg
#   D3 enumerate   DRM connector DSI connected with the panel's mode
#   D4 render      display_probe pattern; operator confirms colours + frame (photo)
#   D5 touch       display_probe --input; operator taps 4 corners + centre
#   D6 repeat      boot log across reboots: >=5 cold + >=5 warm all passing D1-D3, D5 device
#   D7 camera      capture frames while the pattern is up, then re-check D3/D5
#
# Every stage writes raw output to the evidence directory; results.json holds
# the verdicts. PENDING means "needs the operator" - it is not a pass.
# Requires: display_probe on PATH (or DISPLAY_PROBE=...), i2c-tools, v4l-utils.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
. "$HERE/lib/common.sh"

STAGE=all
BOOT_KIND=""
INTERACTIVE=1
EV=""
while [ $# -gt 0 ]; do
    case "$1" in
        --stage) shift; STAGE=${1:?} ;;
        --boot-kind) shift; BOOT_KIND=${1:?} ;;
        --evidence) shift; EV=${1:?} ;;
        --non-interactive) INTERACTIVE=0 ;;
        -h|--help) sed -n '2,24p' "$0"; exit 0 ;;
        *) die "unknown argument $1" ;;
    esac
    shift
done
need_root

[ -f "$STATE_DIR/installed.env" ] || die "no $STATE_DIR/installed.env - run install_panel.sh first"
# shellcheck source=/dev/null
. "$STATE_DIR/installed.env"
# shellcheck source=panels/waveshare-3in5-dsi-h.panel
. "$PANEL_FILE"
PROBE=${DISPLAY_PROBE:-$(command -v display_probe || true)}
BOOT_ID=$(cat /proc/sys/kernel/random/boot_id)
EV=${EV:-$STATE_DIR/evidence/$(date -u +%Y%m%dT%H%M%SZ)-${BOOT_ID%%-*}}
mkdir -p "$EV"
RESULTS="$EV/results.tsv"
: > "$RESULTS"

record() {  # stage verdict note
    printf '%s\t%s\t%s\n' "$1" "$2" "$3" >> "$RESULTS"
    printf '  [%s] %-7s %s\n' "$1" "$2" "$3"
}

ask() {  # question -> y | n | pending
    if [ "$INTERACTIVE" = 0 ]; then echo pending; return; fi
    printf '\n  ?? %s [y/n] ' "$1" > /dev/tty
    read -r a < /dev/tty
    case "$a" in y|Y) echo y ;; *) echo n ;; esac
}

verdict() {  # y|n|pending -> PASS|FAIL|PENDING
    case "$1" in y) echo PASS ;; n) echo FAIL ;; *) echo PENDING ;; esac
}

want() { [ "$STAGE" = all ] || [ "$STAGE" = "$1" ]; }

dmesg_errors() {
    dmesg 2>/dev/null | grep -iE 'dsi.*(error|fail|timeout)|panel.*(fail|error)|goodix.*(error|fail)|cci.*(timeout|error)'
}

DM_STOPPED=""
take_drm_master() {
    for dm in lightdm gdm3 sddm; do
        if systemctl is-active --quiet "$dm" 2>/dev/null; then
            systemctl stop "$dm" && DM_STOPPED="$DM_STOPPED $dm"
        fi
    done
}
# shellcheck disable=SC2329  # invoked by the EXIT trap
restore_dm() { for dm in $DM_STOPPED; do systemctl start "$dm"; done; }
trap restore_dm EXIT

{
    echo "panel=$PANEL_ID goodix=$GOODIX_ADDR installed=$INSTALLED_UTC"
    echo "boot_id=$BOOT_ID boot_kind=${BOOT_KIND:-unspecified}"
    uname -a; tr -d '\0' < /proc/device-tree/model; echo
    git -C "$HERE" rev-parse HEAD 2>/dev/null
} > "$EV/context.txt"
dmesg > "$EV/dmesg.txt" 2>&1
step "Evidence: $EV"

# ---------------------------------------------------------------- D0 ------
if want D0; then
    step "D0 connect (operator)"
    say "  Expected: power removed while cabling; 22-pin end in the carrier DISPLAY"
    say "  connector as photographed for the 5\" (DSI_BRINGUP.md); 15-pin end in the"
    say "  panel; 5 V/3 A supply. Save a photo as $EV/D0-assembly.jpg"
    record D0 "$(verdict "$(ask 'Cabled with power off, supply is 5 V/3 A, photo saved?')")" "operator"
fi

# ---------------------------------------------------------------- D1 ------
D1=FAIL
if want D1 || want D6; then
    step "D1 identify"
    BUS=$(find_cci_bus || true)
    if [ -z "$BUS" ]; then
        record D1 FAIL "carrier I2C bus not found"
    else
        ID=$(gt911_product_id "$BUS" "$GOODIX_ADDR" || true)
        OTHER=0x5d; [ "$GOODIX_ADDR" = 0x5d ] && OTHER=0x14
        ID2=$(gt911_product_id "$BUS" "$OTHER" || true)
        CTRL=$(i2ctransfer -y -f "$BUS" w1@0x45 0x80 r1 2>/dev/null || true)
        {
            echo "bus=$BUS"
            echo "gt911@$GOODIX_ADDR=${ID:-none}"
            echo "gt911@$OTHER=${ID2:-none}"
            echo "0x45=${CTRL:-none}"
        } > "$EV/D1-i2c.txt"
        case "$ID" in
            "0x39 0x31 0x31"*)
                if [ -z "$CTRL" ]; then D1=PASS; record D1 PASS "GT911 at $GOODIX_ADDR, no controller at 0x45"
                else record D1 FAIL "something answers at 0x45 - an ATTINY panel (the 5\"?) is connected"; fi ;;
            *)  record D1 FAIL "no GT911 product ID at $GOODIX_ADDR (other address: ${ID2:-none})" ;;
        esac
    fi
fi

# ---------------------------------------------------------------- D2 ------
D2=FAIL
if want D2 || want D6; then
    step "D2 boot"
    BUILT="$(dirname "$PANEL_FILE")/overlay.dtbo"
    SLOT_OK=0
    [ -f "$BUILT" ] && cmp -s "$BUILT" "$SLOT_DTBO" && SLOT_OK=1
    LIVE=$(find /proc/device-tree -name compatible -exec grep -la "$PANEL_COMPATIBLE" {} + 2>/dev/null | head -1)
    DRV=""
    for d in /sys/bus/mipi-dsi/devices/*; do
        [ -e "$d/driver" ] && DRV="$DRV $(basename "$(readlink "$d/driver")")"
    done
    dmesg_errors > "$EV/D2-dmesg-errors.txt" || true
    {
        echo "slot_matches_build=$SLOT_OK"
        echo "live_dt_node=${LIVE:-none}"
        echo "mipi_dsi_drivers=${DRV:-none}"
        echo "dmesg_error_lines=$(wc -l < "$EV/D2-dmesg-errors.txt")"
    } > "$EV/D2-boot.txt"
    if [ "$SLOT_OK" = 1 ] && [ -n "$LIVE" ] && echo "$DRV" | grep -q panel-simple; then
        D2=PASS; record D2 PASS "overlay live, panel-simple bound, $(wc -l < "$EV/D2-dmesg-errors.txt") dmesg error lines (see file)"
    else
        record D2 FAIL "slot=$SLOT_OK live=${LIVE:+yes} driver=${DRV:-none}"
    fi
fi

# ---------------------------------------------------------------- D3 ------
D3=FAIL
check_drm() {  # prints the DSI connector dir if connected with our mode
    for c in /sys/class/drm/card*-DSI-*; do
        [ -d "$c" ] || continue
        [ "$(cat "$c/status")" = connected ] || continue
        grep -qx "${HACTIVE}x${VACTIVE}" "$c/modes" && { echo "$c"; return 0; }
    done
    return 1
}
if want D3 || want D6; then
    step "D3 enumerate"
    for c in /sys/class/drm/card*-*; do
        [ -f "$c/status" ] && echo "$(basename "$c") $(cat "$c/status") $(tr '\n' ' ' < "$c/modes")"
    done > "$EV/D3-connectors.txt" 2>/dev/null
    CONN=$(check_drm || true)
    if [ -n "$CONN" ]; then
        D3=PASS; record D3 PASS "$(basename "$CONN") connected, ${HACTIVE}x${VACTIVE}"
    else
        record D3 FAIL "no connected DSI connector with ${HACTIVE}x${VACTIVE} (see D3-connectors.txt)"
    fi
fi

# ---------------------------------------------------------------- D4 ------
if want D4; then
    step "D4 render"
    if [ -z "$PROBE" ]; then
        record D4 FAIL "display_probe not found (build it, or set DISPLAY_PROBE)"
    else
        take_drm_master
        "$PROBE" --prefer dsi --pattern 25 > "$EV/D4-probe.txt" 2>&1 &
        P=$!
        sleep 3
        say "  Expected: red top-left, green top-right, blue bottom-left, white"
        say "  bottom-right, a yellow frame touching all four edges, portrait."
        say "  Photo the glass now: $EV/D4-pattern.jpg"
        A=$(ask "Colours in the right places, all four frame edges visible, no flicker/roll?")
        wait "$P"; RC=$?
        MODE_OK=0
        grep -q "DSI.* ${HACTIVE}x${VACTIVE} @" "$EV/D4-probe.txt" && MODE_OK=1
        if [ "$RC" -ne 0 ] || [ "$MODE_OK" = 0 ]; then
            record D4 FAIL "display_probe rc=$RC mode_ok=$MODE_OK (see D4-probe.txt)"
        else
            record D4 "$(verdict "$A")" "probe ok at ${HACTIVE}x${VACTIVE}; operator=$A"
        fi
    fi
fi

# ---------------------------------------------------------------- D5 ------
D5DEV=FAIL
if want D5 || want D6; then
    step "D5 touch"
    grep -B1 -A8 -i goodix /proc/bus/input/devices > "$EV/D5-input-devices.txt" 2>/dev/null \
        && D5DEV=PASS
fi
touch_test() {  # seconds outfile -> PASS|FAIL with corner coverage
    take_drm_master
    say "  Tap, in order: top-left, top-right, bottom-right, bottom-left, centre."
    "$PROBE" --prefer dsi --input "$1" > "$2" 2>&1
    awk -v W="$HACTIVE" -v H="$VACTIVE" '
        $1=="touch" && $2=="down" { n++; x=$3; y=$4
            if (x<W*.2 && y<H*.2) tl=1; if (x>W*.8 && y<H*.2) tr=1
            if (x>W*.8 && y>H*.8) br=1; if (x<W*.2 && y>H*.8) bl=1
            if (x>W*.35 && x<W*.65 && y>H*.35 && y<H*.65) c=1 }
        END { printf "downs=%d tl=%d tr=%d br=%d bl=%d centre=%d\n", n, tl, tr, br, bl, c
              exit !(tl && tr && br && bl && c) }' "$2"
}
if want D5; then
    if [ "$D5DEV" = FAIL ]; then
        record D5 FAIL "no Goodix input device"
    elif [ -z "$PROBE" ] || [ "$INTERACTIVE" = 0 ]; then
        record D5 PENDING "Goodix input device present; tap test needs the operator"
    else
        if COV=$(touch_test 30 "$EV/D5-touch.txt"); then
            record D5 PASS "$COV"
        else
            # Device present + zero events is the CCI 12-byte read fault signature.
            record D5 FAIL "$COV"
        fi
    fi
fi

# ---------------------------------------------------------------- D6 ------
if want D6; then
    step "D6 reboot repeatability"
    LOG="$STATE_DIR/boots.tsv"
    [ -f "$LOG" ] || printf 'boot_id\tkind\tutc\tD1\tD2\tD3\tD5dev\n' > "$LOG"
    if grep -q "^$BOOT_ID" "$LOG"; then
        say "  this boot is already logged"
    else
        printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$BOOT_ID" "${BOOT_KIND:-unspecified}" \
            "$(date -u +%Y%m%dT%H%M%SZ)" "$D1" "$D2" "$D3" "$D5DEV" >> "$LOG"
    fi
    cp "$LOG" "$EV/D6-boots.tsv"
    # Only boots since the last install count.
    SUMMARY=$(awk -F'\t' -v since="$INSTALLED_UTC" 'NR>1 && $3>=since {
            n++; ok=($4=="PASS"&&$5=="PASS"&&$6=="PASS"&&$7=="PASS")
            if (!ok) bad++; else if ($2=="cold") c++; else if ($2=="warm") w++ }
        END { printf "%d %d %d %d", n, bad+0, c+0, w+0 }' "$LOG")
    # shellcheck disable=SC2086  # split on purpose
    set -- $SUMMARY
    if [ "$2" -gt 0 ]; then
        record D6 FAIL "$2 of $1 logged boots failed an automatic check (D6-boots.tsv)"
    elif [ "$3" -ge 5 ] && [ "$4" -ge 5 ]; then
        record D6 PASS "$3 cold + $4 warm boots, all passing"
    else
        record D6 PENDING "$3/5 cold, $4/5 warm so far - reboot and rerun with --stage D6 --boot-kind cold|warm"
    fi
fi

# ---------------------------------------------------------------- D7 ------
if want D7; then
    step "D7 camera coexistence"
    CAM=${CAM_DEV:-}
    if [ -z "$CAM" ] && have_cmd v4l2-ctl; then
        v4l2-ctl --list-devices > "$EV/D7-v4l2-devices.txt" 2>&1
        # First capture node of a USB camera; CSI per #40 via CAM_DEV / CAM_CMD.
        CAM=$(awk '/usb|USB/{u=1;next} u&&/\/dev\/video/{print $1; exit}' "$EV/D7-v4l2-devices.txt")
    fi
    if [ -z "$PROBE" ] || { [ -z "$CAM" ] && [ -z "${CAM_CMD:-}" ]; }; then
        record D7 FAIL "need display_probe and a camera (CAM_DEV=/dev/videoN or CAM_CMD='...')"
    else
        BEFORE=$(dmesg_errors | wc -l)
        take_drm_master
        "$PROBE" --prefer dsi --pattern 30 > "$EV/D7-probe.txt" 2>&1 &
        P=$!
        sleep 3
        if [ -n "${CAM_CMD:-}" ]; then
            sh -c "$CAM_CMD" > "$EV/D7-capture.log" 2>&1; CAPRC=$?
        else
            v4l2-ctl -d "$CAM" --stream-mmap --stream-count=90 --stream-to="$EV/D7-frames.raw" \
                > "$EV/D7-capture.log" 2>&1; CAPRC=$?
            [ -s "$EV/D7-frames.raw" ] || CAPRC=1
        fi
        A=$(ask "Did the pattern stay stable (no blanking/tearing) while the camera ran?")
        wait "$P"
        AFTER=$(dmesg_errors | wc -l)
        CONN=$(check_drm || true)
        TOUCH="skipped"
        if [ "$INTERACTIVE" = 1 ]; then
            TOUCH=$(touch_test 20 "$EV/D7-touch.txt" && echo ok || echo fail)
        fi
        rm -f "$EV/D7-frames.raw"
        NOTE="capture_rc=$CAPRC new_dmesg_errors=$((AFTER - BEFORE)) dsi=${CONN:+connected} touch=$TOUCH operator=$A"
        if [ "$CAPRC" -ne 0 ] || [ "$AFTER" -gt "$BEFORE" ] || [ -z "$CONN" ] || [ "$TOUCH" = fail ]; then
            record D7 FAIL "$NOTE"
        elif [ "$TOUCH" = skipped ]; then
            record D7 PENDING "$NOTE"
        else
            record D7 "$(verdict "$A")" "$NOTE"
        fi
    fi
fi

# ------------------------------------------------------------- results ----
python3 - "$RESULTS" "$EV/results.json" "$PANEL_ID" "$BOOT_ID" <<'PY'
import json, sys
rows = [l.rstrip("\n").split("\t", 2) for l in open(sys.argv[1]) if l.strip()]
out = {"panel": sys.argv[3], "boot_id": sys.argv[4],
       "stages": {s: {"verdict": v, "note": n} for s, v, n in rows}}
out["overall"] = ("FAIL" if any(r[1] == "FAIL" for r in rows)
                  else "PENDING" if any(r[1] != "PASS" for r in rows) else "PASS")
json.dump(out, open(sys.argv[2], "w"), indent=2)
print(f"\n  overall: {out['overall']}  ->  {sys.argv[2]}")
PY
grep -q FAIL "$RESULTS" && exit 1
exit 0
