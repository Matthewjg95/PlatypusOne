#!/bin/sh
# imx219_overlay_fix.sh — apply arduino/linux-qcom PR #5 to an installed board.
#
#   tools/csi_bench/imx219_overlay_fix.sh build      (normal user)
#   sudo tools/csi_bench/imx219_overlay_fix.sh install
#   sudo tools/csi_bench/imx219_overlay_fix.sh rollback
#
# Upstream: https://github.com/arduino/linux-qcom/pull/5 (merged 2026-09-28)
# removes `reset-gpios` (PCA9555 pin 0 / pin 2) from the four Media Carrier
# IMX219 overlays. Third-party IMX219 modules load the CCI bus while XCLR is
# low; the PCA9555 on that bus then stops answering, XCLR is never released
# and the sensor never acknowledges ("failed to read chip id"). Boards on the
# 2026-05-08 kernel package still carry the old overlays.
#
# build    decompiles the installed overlays, removes exactly the sensor's
#          reset-gpios and its __local_fixups__ entry, recompiles, and
#          test-composes them with the base, carrier and current panel
#          overlays (no root, nothing installed).
# install  keeps Arduino's originals as *.dtbo.arduino-orig (once), installs
#          the rebuilt overlays, re-runs `arduino-linux-config carrier enable`
#          with the current camera/display options, syncs. Then POWER OFF FULLY
#          (unplug ~10 s): the expander keeps its state across a warm reboot.
# rollback restores the originals and re-composes.
set -eu

Q=/boot/efi/dtb/qcom
W=${IMX219_FIX_DIR:-$HOME/csi_bench/overlay-fix}
[ "$(id -u)" -eq 0 ] && [ -n "${SUDO_USER:-}" ] && W=${IMX219_FIX_DIR:-$(getent passwd "$SUDO_USER" | cut -d: -f6)/csi_bench/overlay-fix}
VARIANTS="csi0-2lanes csi0-4lanes csi1-2lanes csi1-4lanes"
name() { printf 'qrb2210-arduino-imola-carrier-media-camera-imx219-%s.dtbo' "$1"; }

current_options() { # "camera0=... camera1=... display=..." as configured for next boot
    arduino-linux-config --format json carrier show media-carrier >"$W/carrier.json"
    python3 - "$W/carrier.json" <<'PY'
import json, sys
nxt = json.load(open(sys.argv[1]))["carriers"][0]["next"]
print(" ".join(d["device"] + "=" + d["option"] for d in nxt))
PY
}

recompose() {
    opts=$(current_options)
    echo "re-composing: arduino-linux-config carrier enable media-carrier $opts"
    # shellcheck disable=SC2086
    arduino-linux-config carrier enable media-carrier $opts
    sync
}

case "${1:-}" in
    build)
        mkdir -p "$W"
        cd "$W"
        for v in $VARIANTS; do
            f=$(name "$v")
            src="$Q/$f"
            [ -f "$src.arduino-orig" ] && src="$src.arduino-orig"
            cp "$src" "orig-$f"
            dtc -q -I dtb -O dts -o "$v.dts" "orig-$f"
            grep -v -E '^[[:space:]]*reset-gpios = <(0x01 0x0[02] 0x00|0x00)>;$' "$v.dts" >"$v.fixed.dts"
            [ "$(grep -c reset-gpios "$v.fixed.dts" || true)" = 0 ] || { echo "reset-gpios still present in $v" >&2; exit 1; }
            dtc -q -I dts -O dtb -o "$f" "$v.fixed.dts"
        done
        fdtoverlay -i "$Q/qrb2210-arduino-imola-base.dtb" -o test.dtb \
            "$Q/qrb2210-arduino-imola-carrier-media.dtbo" \
            "$Q/qrb2210-arduino-imola-carrier-media-panel-5in_touch_a-dsi.dtbo" \
            "$(name csi0-2lanes)" "$(name csi1-2lanes)"
        for b in i2c-bus@0 i2c-bus@1; do
            p=/soc@0/cci@5c1b000/$b/sensor@10
            [ "$(fdtget test.dtb $p compatible)" = sony,imx219 ] || { echo "no sensor at $p" >&2; exit 1; }
            if fdtget test.dtb $p reset-gpios >/dev/null 2>&1; then echo "reset-gpios survived at $p" >&2; exit 1; fi
        done
        sha256sum qrb2210-*.dtbo >SHA256SUMS
        echo "built and test-composed in $W"
        ;;
    install)
        [ "$(id -u)" -eq 0 ] || { echo "install needs sudo" >&2; exit 1; }
        for v in $VARIANTS; do
            [ -f "$W/$(name "$v")" ] || { echo "run '$0 build' first" >&2; exit 1; }
        done
        (cd "$W" && sha256sum -c --quiet SHA256SUMS)
        for v in $VARIANTS; do
            f=$(name "$v")
            [ -f "$Q/$f.arduino-orig" ] || cp -a "$Q/$f" "$Q/$f.arduino-orig"
            cp "$W/$f" "$Q/$f"
        done
        recompose
        echo
        echo "installed. Now power OFF fully (unplug ~10 s), power on, and run csi_bench.sh."
        ;;
    rollback)
        [ "$(id -u)" -eq 0 ] || { echo "rollback needs sudo" >&2; exit 1; }
        for v in $VARIANTS; do
            f=$(name "$v")
            [ -f "$Q/$f.arduino-orig" ] && cp -a "$Q/$f.arduino-orig" "$Q/$f"
        done
        recompose
        echo "originals restored; power-cycle the board"
        ;;
    *)
        echo "usage: $0 build | install | rollback" >&2
        exit 2
        ;;
esac
