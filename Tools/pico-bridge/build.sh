#!/usr/bin/env bash
# Reproducible Pico 2 W build for the Steam Controller 2 bridge fork.
#   ./Tools/pico-bridge/build.sh [build-dir] [extra cmake args...]
#   ./Tools/pico-bridge/build.sh build/capture -DOGXM_FORCE_CAPTURE=ON
# Needs: arm-none-eabi-gcc, cmake, ninja, git, python3.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${1:-$REPO/build/pico2w}"
SDK_VERSION="2.1.0"
SDK_PATH="${PICO_SDK_PATH:-/tmp/pico-sdk-$SDK_VERSION}"

# Upstream cmake matches git's English error text; a localized git breaks the
# "patch already applied" check.
export LC_ALL=C

if [ ! -d "$SDK_PATH" ]; then
    git clone --branch "$SDK_VERSION" --depth 1 --recurse-submodules \
        https://github.com/raspberrypi/pico-sdk.git "$SDK_PATH"
fi
export PICO_SDK_PATH="$SDK_PATH"

# pioasm (host tool) misses <cstdint> under GCC 15+.
for f in "$SDK_PATH/tools/pioasm/pio_types.h" "$SDK_PATH/tools/pioasm/output_format.h"; do
    grep -q '#include <cstdint>' "$f" || sed -i '0,/^#include /s//#include <cstdint>\n#include /' "$f"
done

cmake -S "$REPO/Firmware/RP2040" -B "$BUILD_DIR" -G Ninja \
    -DPICO_SDK_PATH="$SDK_PATH" \
    -DPICOTOOL_FORCE_FETCH_FROM_GIT=1 \
    -DPICOTOOL_FETCH_FROM_GIT_PATH="${PICOTOOL_DIR:-$SDK_PATH/../picotool-fetch}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DOGXM_BOARD=PI_PICO2W \
    -DMAX_GAMEPADS=1 \
    "${@:2}"
cmake --build "$BUILD_DIR" -j"$(nproc)"

sha256sum "$BUILD_DIR"/*.uf2
