#!/bin/sh
# Install the PlatypusOne kiosk unit and the `platypus-mode` switch on the UNO Q.
#
#   sudo ./tools/kiosk/install.sh
#
# Installs, does not switch: the board stays in whatever mode it is in until
# you run `sudo platypus-mode kiosk`. Safe to re-run (re-reads the repo path).
# The kiosk binary is built separately (cmake --build build-bench).
set -eu

[ "$(id -u)" -eq 0 ] || { echo "run with sudo" >&2; exit 1; }

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO=$(CDPATH= cd -- "$HERE/../.." && pwd)
# The user who owns the checkout runs the kiosk — never root.
KIOSK_USER=${SUDO_USER:-$(stat -c %U "$REPO")}

BIN="$REPO/build-bench/tools/scout_kiosk/scout_kiosk"
[ -x "$BIN" ] || echo "note: $BIN not built yet - build before switching to kiosk mode" >&2

sed -e "s|@REPO@|$REPO|g" -e "s|@USER@|$KIOSK_USER|g" \
    "$HERE/platypus-kiosk.service.in" > /etc/systemd/system/platypus-kiosk.service
install -m 0755 "$HERE/platypus-mode" /usr/local/bin/platypus-mode
systemctl daemon-reload
# Flush before reporting success: a bench board that loses power seconds
# after an install otherwise keeps zero-length files (seen 2026-10-04: an
# empty platypus-mode and a corrupted checkout after an unplug).
sync

echo "installed platypus-kiosk.service (runs as $KIOSK_USER from $REPO)"
echo "installed /usr/local/bin/platypus-mode"
echo
echo "switch:  sudo platypus-mode kiosk [--once]  |  sudo platypus-mode desktop"
