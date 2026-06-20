#!/usr/bin/env bash
set -euo pipefail

sudo pacman -S --needed base-devel cmake ninja qt6-base ffmpeg

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build

echo
echo "Built: ./build/imagetransient"
echo "Run with: ./build/imagetransient"
