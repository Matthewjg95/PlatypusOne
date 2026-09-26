#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
BUILD_TOF_TEST=$(mktemp -d)
trap 'rm -rf "$BUILD_TOF_TEST"' EXIT
ULD=firmware/tof_bringup/src/uld
for src in "$ULD"/*.c; do
  gcc -std=c11 -I"$ULD" -c "$src" -o "$BUILD_TOF_TEST/$(basename "$src" .c).o"
done
g++ -std=c++17 -Wall -Wextra -Werror -Itests/tof_bringup tests/tof_bringup/transport.cpp "$BUILD_TOF_TEST"/*.o -o "$BUILD_TOF_TEST/test"
"$BUILD_TOF_TEST/test"
echo 'PASS: host ULD compile; transport chunking, register order and failure cleanup'
