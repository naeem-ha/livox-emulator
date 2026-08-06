#!/usr/bin/env bash
# Cross-compile a fully static aarch64 binary for the QEMU guest / Raspberry Pi.
set -euo pipefail
cd "$(dirname "$0")/.."

cmake -B build-aarch64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake \
  -DEMULATOR_STATIC=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-aarch64 -j"$(nproc)" --target livox_emulator
file build-aarch64/livox_emulator
