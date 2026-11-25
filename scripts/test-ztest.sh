#!/usr/bin/env bash
#
# test-ztest.sh - build and run the HWLINK ztest suite on native_sim
#
# Author:  Sandesh Ghimire <sandesh@soccentric.com>
# Usage:   scripts/test-ztest.sh [clean]
#
# native_sim is a Zephyr board that runs the image as a host process, so the
# same test runs on a developer machine and in GitHub Actions without a board
# or an emulator.  The build uses the freestanding CMake flow of this repo; no
# west workspace is required.
#

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
DEV="$ROOT/scripts/docker-dev.sh"
BUILD="$ROOT/build/tests-hwlink"
cd "$ROOT"

if [[ "${1:-}" == "clean" ]]; then
    rm -rf "$BUILD"
fi

"$DEV" run env CCACHE_DIR=/work/build/tests-hwlink/.ccache \
    cmake -S /work/tests/hwlink -B /work/build/tests-hwlink -G Ninja \
    -DBOARD=native_sim \
    -DUSER_CACHE_DIR=/work/build/tests-hwlink/.zephyr-cache
"$DEV" run cmake --build /work/build/tests-hwlink
"$DEV" run /work/build/tests-hwlink/zephyr/zephyr.exe
