#!/bin/sh
# capture_raw.sh OUT — one raw IMX219 frame (SRGGB10 MIPI-packed, pRAA) at
# fixed exposure/gain through camss, bypassing libcamera's uncalibrated ISP.
#
# NOT YET RUN ON HARDWARE as a script. It packages the manual raw path recorded
# on the UNO Q on 2026-10-04 (PR #42, docs/hardware/CSI_CAMERA_BENCH.md, run
# B0393/20261004T232839Z-raw): 1640x1232, 2056 bytes/line, black level ~64.
#
# Environment (defaults = that bench run, camera on CAMERA1):
#   CC_WIDTH=1640 CC_HEIGHT=1232
#   CC_EXPOSURE=1700 CC_GAIN=100      sensor control units (lines, gain code)
#   CC_CSIPHY=msm_csiphy1 CC_CSID=msm_csid0 CC_VFE=msm_vfe0_rdi0
#   CC_VIDEO=/dev/video0 CC_MEDIA=/dev/media0
#   CC_BLACK=64 CC_WHITE=1023 CC_BAYER=RGGB
#
# Prints the read-back state as one JSON line (consumed by `camera_characterize
# capture`). Exits non-zero, writing nothing, if the sensor is not found.
set -eu
OUT=${1:?usage: capture_raw.sh OUT}
W=${CC_WIDTH:-1640}
H=${CC_HEIGHT:-1232}
MEDIA=${CC_MEDIA:-/dev/media0}
VIDEO=${CC_VIDEO:-/dev/video0}
[ -e "$OUT" ] && { echo "refusing to overwrite $OUT" >&2; exit 1; }

SENSOR_ENTITY=$(media-ctl -d "$MEDIA" -p | sed -n 's/.*entity [0-9]*: \(imx219 [0-9]*-0010\).*/\1/p' | head -1)
[ -n "$SENSOR_ENTITY" ] || { echo "no imx219 entity in $MEDIA" >&2; exit 2; }
SUBDEV=""
LENS=""
for sd in /sys/class/video4linux/v4l-subdev*; do
    case "$(cat "$sd/name" 2>/dev/null)" in
        imx219*) SUBDEV=/dev/$(basename "$sd") ;;
        *dw97* | *dw98* | *ak73* | *vcm* | *lens* | *focus*) LENS=/dev/$(basename "$sd") ;;
    esac
done
[ -n "$SUBDEV" ] || { echo "no imx219 subdev" >&2; exit 2; }

FMT="SRGGB10_1X10/${W}x${H}"
media-ctl -d "$MEDIA" -V "\"$SENSOR_ENTITY\":0[fmt:$FMT]"
media-ctl -d "$MEDIA" -V "\"${CC_CSIPHY:-msm_csiphy1}\":0[fmt:$FMT]"
media-ctl -d "$MEDIA" -V "\"${CC_CSID:-msm_csid0}\":0[fmt:$FMT]"
media-ctl -d "$MEDIA" -V "\"${CC_VFE:-msm_vfe0_rdi0}\":0[fmt:$FMT]"
v4l2-ctl -d "$SUBDEV" -c exposure="${CC_EXPOSURE:-1700}" -c analogue_gain="${CC_GAIN:-100}"

TMP="$OUT.partial"
v4l2-ctl -d "$VIDEO" --set-fmt-video=width="$W",height="$H",pixelformat=pRAA \
    --stream-mmap=4 --stream-count=5 --stream-skip=4 --stream-to="$TMP" >/dev/null
STRIDE=$(( (W * 5 + 3) / 4 ))
FRAME=$(( STRIDE * H ))
# Keep only the LAST frame (earlier ones may carry the previous exposure).
SIZE=$(wc -c < "$TMP")
[ "$SIZE" -ge "$FRAME" ] || { rm -f "$TMP"; echo "short stream: $SIZE < $FRAME bytes" >&2; exit 3; }
tail -c "$FRAME" "$TMP" > "$OUT"
rm -f "$TMP"

EXP=$(v4l2-ctl -d "$SUBDEV" -C exposure | sed 's/.*: //')
GAIN=$(v4l2-ctl -d "$SUBDEV" -C analogue_gain | sed 's/.*: //')
FOCUS='"not_exposed"'
FMODE='"not_exposed"'
if [ -n "$LENS" ]; then
    FOCUS=$(v4l2-ctl -d "$LENS" -C focus_absolute 2>/dev/null | sed 's/.*: //')
    FOCUS=${FOCUS:-null}
    FMODE='"motorized_position"'
fi
printf '{"format":"raw10p","width":%s,"height":%s,"stride":%s,"bayer_pattern":"%s","black_level":%s,"white_level":%s,"exposure_lines":%s,"analogue_gain":%s,"focus_position":%s,"focus_readback_mode":%s,"sensor_entity":"%s"}\n' \
    "$W" "$H" "$STRIDE" "${CC_BAYER:-RGGB}" "${CC_BLACK:-64}" "${CC_WHITE:-1023}" \
    "${EXP:-null}" "${GAIN:-null}" "$FOCUS" "$FMODE" "$SENSOR_ENTITY"
