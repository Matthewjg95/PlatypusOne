#!/bin/sh
# install_imx219_rebind.sh — run imx219_rebind.sh after every panel recovery.
#
#   sudo tools/csi_bench/install_imx219_rebind.sh            install / update
#   sudo tools/csi_bench/install_imx219_rebind.sh --remove   uninstall
#
# Ordering: uno-q-dsi-panel-recover.service is itself After=multi-user.target,
# so a unit WantedBy=multi-user.target and After= the recovery would form the
# same ordering cycle that once dropped the kiosk at boot. This unit is pulled
# in by the recovery service instead (WantedBy=uno-q-dsi-panel-recover.service)
# and ordered after it: no cycle.
set -eu

[ "$(id -u)" -eq 0 ] || { echo "run with sudo" >&2; exit 1; }
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
UNIT=/etc/systemd/system/platypus-imx219-rebind.service
BIN=/usr/local/libexec/platypus-imx219-rebind

if [ "${1:-}" = "--remove" ]; then
    systemctl disable platypus-imx219-rebind.service 2>/dev/null || true
    rm -f "$UNIT" "$BIN"
    systemctl daemon-reload
    sync
    echo "removed"
    exit 0
fi

install -D -m 0755 "$HERE/imx219_rebind.sh" "$BIN"
cat >"$UNIT" <<EOF
[Unit]
Description=Bind IMX219 cameras whose probe failed during the panel's flaky boot window
After=uno-q-dsi-panel-recover.service
ConditionPathExists=/sys/bus/i2c/drivers

[Service]
Type=oneshot
ExecStart=$BIN

[Install]
WantedBy=uno-q-dsi-panel-recover.service
EOF
systemctl daemon-reload
systemctl enable platypus-imx219-rebind.service
sync
echo "installed: runs after uno-q-dsi-panel-recover on every boot"
echo "run now:   sudo systemctl start platypus-imx219-rebind && journalctl -u platypus-imx219-rebind -b"
