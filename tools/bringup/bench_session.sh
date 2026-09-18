#!/usr/bin/env bash
# Platypus One — first bench session driver.
#
# Runs the hardware-gated steps of docs/hardware/TEST_CHECKLISTS.md end to end
# and prints a paste-ready report. Written to be run on the machine the UNO Q
# and camera are actually attached to — a cloud agent session cannot see USB
# devices, so this is the handoff between "code is ready" and "hardware is
# proven".
#
#   ./tools/bringup/bench_session.sh              # full run
#   ./tools/bringup/bench_session.sh --no-build   # skip configure/build
#
# Nothing here writes to the repo except the build directory and ./observations.
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${REPO}/build-bench"
DO_BUILD=1
[[ "${1:-}" == "--no-build" ]] && DO_BUILD=0

pass=0; fail=0; skip=0
step() { printf '\n=== %s ===\n' "$1"; }
ok()   { printf '  PASS  %s\n' "$1"; pass=$((pass+1)); }
no()   { printf '  FAIL  %s\n' "$1"; fail=$((fail+1)); }
meh()  { printf '  SKIP  %s\n' "$1"; skip=$((skip+1)); }

printf 'Platypus One bench session — %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
printf 'repo: %s\n' "$REPO"

step "0. Host identity"
uname -a || true
if grep -qiE 'qualcomm|qrb2210|uno.?q' /proc/device-tree/model 2>/dev/null \
   || grep -qiE 'qrb2210|uno.?q' /proc/cpuinfo 2>/dev/null; then
    echo "  -> looks like we are running ON the UNO Q (native build path)"
else
    echo "  -> looks like a dev host (UNO Q should be reachable over USB/network)"
fi

step "1. MCU / serial link"
mapfile -t TTYS < <(ls /dev/ttyACM* /dev/ttyUSB* /dev/ttyRPMSG* 2>/dev/null)
if (( ${#TTYS[@]} )); then
    ok "serial nodes: ${TTYS[*]}"
else
    meh "no /dev/ttyACM*, /dev/ttyUSB* or /dev/ttyRPMSG* — board not attached, or you are on the UNO Q itself"
fi
command -v arduino-cli >/dev/null && ok "arduino-cli $(arduino-cli version 2>/dev/null | head -1)" \
                                  || meh "arduino-cli not installed (needed to flash firmware/)"

step "2. Camera enumeration"
mapfile -t VIDS < <(ls /dev/video* 2>/dev/null)
if (( ${#VIDS[@]} )); then
    ok "video nodes: ${VIDS[*]}"
    if command -v v4l2-ctl >/dev/null; then
        v4l2-ctl --list-devices 2>/dev/null | sed 's/^/    /'
        for v in "${VIDS[@]}"; do
            # A UVC webcam exposes a capture node AND a metadata node. Only the
            # capture node lists formats — that is how you tell them apart.
            fmts=$(v4l2-ctl -d "$v" --list-formats 2>/dev/null | grep -c "\[")
            printf '    %s: %s format(s)%s\n' "$v" "$fmts" \
                   "$( (( fmts == 0 )) && echo '   <- metadata node, do NOT use' )"
        done
        echo "    --- formats on the first capture node ---"
        for v in "${VIDS[@]}"; do
            if (( $(v4l2-ctl -d "$v" --list-formats 2>/dev/null | grep -c "\[") > 0 )); then
                v4l2-ctl -d "$v" --list-formats-ext 2>/dev/null | sed 's/^/    /' | head -40
                break
            fi
        done
    else
        meh "v4l2-ctl not installed (apt install v4l-utils) — falling back to the harness's own --list"
    fi
else
    no "no /dev/video* — camera not attached"
fi

step "3. Build"
if (( DO_BUILD )); then
    if cmake -S "$REPO" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release >/tmp/pb_cfg.log 2>&1 \
       && cmake --build "$BUILD" -j"$(nproc)" >/tmp/pb_build.log 2>&1; then
        ok "configure + build"
    else
        no "build failed — tail of log:"; tail -20 /tmp/pb_build.log /tmp/pb_cfg.log | sed 's/^/    /'
    fi
else
    meh "build skipped (--no-build)"
fi

CAP="${BUILD}/tools/engineering_scout_capture/engineering_scout_capture"
step "4. Host tests"
if [[ -x "${BUILD}/tests/platypus_tests" ]]; then
    if "${BUILD}/tests/platypus_tests" >/tmp/pb_tests.log 2>&1; then
        ok "$(grep -c ': OK' /tmp/pb_tests.log) suites"
    else
        no "tests failed:"; tail -15 /tmp/pb_tests.log | sed 's/^/    /'
    fi
else
    meh "test binary not built"
fi

step "5. Camera modes as the harness sees them"
if [[ -x "$CAP" ]] && (( ${#VIDS[@]} )); then
    for v in "${VIDS[@]}"; do
        printf '  %s:\n' "$v"
        "$CAP" --device "$v" --list 2>&1 | sed 's/^/    /'
    done
else
    meh "harness or camera unavailable"
fi

step "6. Live capture + measurement"
echo "  Place the calibration square and one fastener in frame, well lit,"
echo "  not touching, on a light background. Pass --reference-mm if the"
echo "  square is not 20 mm."
if [[ -x "$CAP" ]] && (( ${#VIDS[@]} )); then
    out="${REPO}/observations"
    if "$CAP" --device "${VIDS[0]}" --out "$out" 2>&1 | sed 's/^/    /'; then
        latest=$(ls -td "$out"/scan-* 2>/dev/null | head -1)
        [[ -n "$latest" ]] && ok "wrote $latest" && ls -la "$latest" | sed 's/^/    /' \
                           || no "no observation directory produced"
    else
        no "capture failed (see output above)"
    fi
else
    meh "harness or camera unavailable"
fi

printf '\n=== SUMMARY: %d pass, %d fail, %d skip ===\n' "$pass" "$fail" "$skip"
echo "Paste this whole output back to Claude, plus observations/scan-NNNN/observation.json."
