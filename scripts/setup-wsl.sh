#!/usr/bin/env bash
# Build helper for the Chess Clock firmware under WSL / Ubuntu.
#
# Unlike pomodoro_timer (which vendors the SDKs under third_party/), the chess clock
# REUSES the SDKs already vendored in pomodoro_timer via two env vars (SPEC §11). This
# script just sets them and configures + builds. Safe to re-run.
#
# Usage:   bash scripts/setup-wsl.sh
# Then:    the build/ dir holds chess_clock.uf2
#
# Prereqs (install once):
#   sudo apt update
#   sudo apt install -y cmake python3 build-essential gcc-arm-none-eabi \
#        libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib git ninja-build
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
echo "Project root: $ROOT"

# Shared SDKs vendored in the sibling pomodoro_timer repo (override by exporting these
# before running, e.g. if pomodoro_timer lives elsewhere).
: "${PICO_SDK_PATH:=$ROOT/../pomodoro_timer/third_party/pico-sdk}"
: "${PICOSYSTEM_DIR:=$ROOT/../pomodoro_timer/third_party/picosystem}"
export PICO_SDK_PATH PICOSYSTEM_DIR
echo "PICO_SDK_PATH=$PICO_SDK_PATH"
echo "PICOSYSTEM_DIR=$PICOSYSTEM_DIR"

if [ ! -f "$PICO_SDK_PATH/pico_sdk_init.cmake" ]; then
  echo "ERROR: pico-sdk not found at $PICO_SDK_PATH" >&2
  echo "Vendor it in pomodoro_timer (bash ../pomodoro_timer/scripts/setup-wsl.sh) or" >&2
  echo "export PICO_SDK_PATH / PICOSYSTEM_DIR to your SDK locations." >&2
  exit 1
fi

mkdir -p build && cd build
cmake .. -G Ninja
ninja

echo
echo "Built: $ROOT/build/chess_clock.uf2"
echo "Flash: hold X while powering on -> RPI-RP2 drive, then copy chess_clock.uf2 onto it."
