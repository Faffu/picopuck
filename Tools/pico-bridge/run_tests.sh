#!/usr/bin/env bash
# Host tests for the decoder and the Switch Pro conversions.
set -euo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC="$REPO/Firmware/RP2040/src"
OUT="${TMPDIR:-/tmp}/ogxm_bridge_tests"

g++ -std=c++20 -Wall -Wextra -Wno-unused-parameter -I"$SRC" -o "$OUT" \
    "$REPO/Tools/pico-bridge/tests/test_bridge.cpp" \
    "$SRC/SteamController2/SteamController2.cpp" \
    "$SRC/SteamController2/ReportCapture.cpp" \
    "$SRC/USBDevice/DeviceDriver/SwitchPro/SwitchProCodec.cpp"

"$OUT"
