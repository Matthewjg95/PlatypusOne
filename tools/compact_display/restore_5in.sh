#!/bin/sh
# Put the proven Waveshare 5" (800x480) configuration back.
#
#   sudo ./restore_5in.sh [uno_q_dsi_displays checkout]     # default, authoritative
#   sudo ./restore_5in.sh --offline                          # no network, from backup
#
# Default: re-runs the upstream installer with panels/waveshare-800x480.panel -
# the exact command that produced the working 5" (DSI_BRINGUP.md step 10) - so
# the restored state is the proven state, not a reconstruction of it. Needs
# network (it fetches kernel sources for the module build).
#
# --offline: copies the 5" slot overlay and panel-simple module binaries back
# from the backup install_panel.sh made. Quick, but DKMS still holds the
# compact panel's sources, so a later kernel upgrade would rebuild the wrong
# descriptor. Follow it with the default mode when the network is available.
#
# Both modes remove the Goodix fix if install_panel.sh added it, then check the
# slot overlay against the hash recorded before the compact install.
# It does NOT reboot.
set -e
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
. "$HERE/lib/common.sh"
need_root

# shellcheck source=/dev/null
[ -f "$STATE_DIR/installed.env" ] && . "$STATE_DIR/installed.env"
BACKUP=${BACKUP:-$STATE_DIR/last-backup}
[ -d "$BACKUP" ] || warn "no install_panel.sh backup found ($BACKUP); hash check will be skipped"
MODDIR="/lib/modules/$(uname -r)"

OFFLINE=0
[ "${1:-}" = "--offline" ] && { OFFLINE=1; shift; }

step "Goodix fix"
if [ -d "$BACKUP" ] && [ ! -f "$BACKUP/goodix-fix-was-present" ] && [ -e "$MODDIR/updates/goodix_ts.ko" ]; then
    rm -f "$MODDIR/updates/goodix_ts.ko"
    depmod -a
    ok "removed (it was added by install_panel.sh)"
else
    ok "left as found"
fi

if [ "$OFFLINE" = 1 ]; then
    [ -d "$BACKUP" ] || die "--offline needs the install_panel.sh backup"
    step "Restoring from $BACKUP"
    cp -a "$BACKUP/$(basename "$SLOT_DTBO")" "$SLOT_DTBO"
    if [ -d "$BACKUP/modules" ]; then
        (cd "$BACKUP/modules" && find . -type f) | while read -r f; do
            cp -a "$BACKUP/modules/${f#./}" "/${f#./}"
            ok "/${f#./}"
        done
        depmod -a
    fi
    arduino-linux-config carrier enable media-carrier "display=$CARRIER_DISPLAY_OPTION"
    warn "DKMS still holds the compact panel sources - rerun without --offline when online"
else
    UP=${1:-${UPSTREAM:-}}
    if [ -z "$UP" ] || [ ! -f "$UP/install.sh" ]; then
        die "usage: sudo $0 <uno_q_dsi_displays checkout>"
    fi
    step "Re-running the proven 5\" install"
    (cd "$UP" && sh ./install.sh "$PROVEN_5IN_PANEL")
fi

step "Checking the slot overlay"
if [ -f "$BACKUP/slot.sha256" ]; then
    WANT=$(cut -d' ' -f1 "$BACKUP/slot.sha256")
    HAVE=$(sha256sum "$SLOT_DTBO" | cut -d' ' -f1)
    if [ "$WANT" = "$HAVE" ]; then
        ok "identical to the pre-install 5\" overlay ($(printf '%s' "$HAVE" | cut -c1-12))"
    else
        # Expected after an online restore if upstream generation is not
        # byte-reproducible; the reboot check below is what counts.
        warn "differs from the pre-install overlay (want ${WANT%"${WANT#????????????}"}, have ${HAVE%"${HAVE#????????????}"})"
    fi
fi
mv -f "$STATE_DIR/installed.env" "$STATE_DIR/installed.env.restored-$(date -u +%Y%m%dT%H%M%SZ)" 2>/dev/null || true

step "Reboot, then confirm the 5\" the usual way"
say "  sudo reboot"
say "  display_probe            # DSI-1 connected, 800x480"
say "  display_probe --input 20 # touch the glass"
