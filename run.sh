#!/usr/bin/env bash
# Build and run cTetris.
# Usage: ./run.sh [--debug]

set -e

BUILD_DIR="build"
BUILD_TYPE="Release"

if [[ "$1" == "--debug" ]]; then
    BUILD_DIR="build-debug"
    BUILD_TYPE="Debug"
fi

JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)"

cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" -j"$JOBS"

exec "./$BUILD_DIR/cTetris"
