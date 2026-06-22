#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build/release}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
PREFIX="${PREFIX:-/usr}"
GENERATOR="${GENERATOR:-Ninja}"

run_sudo() {
  if [[ ${EUID:-$(id -u)} -eq 0 ]]; then
    "$@"
  else
    sudo "$@"
  fi
}

cmake -S "$ROOT" -B "$BUILD_DIR" -G "$GENERATOR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_INSTALL_PREFIX="$PREFIX"

cmake --build "$BUILD_DIR" --parallel "$(nproc 2>/dev/null || echo 2)"
run_sudo cmake --install "$BUILD_DIR"

if command -v update-desktop-database >/dev/null 2>&1; then
  run_sudo update-desktop-database "${PREFIX}/share/applications" || true
fi

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
  run_sudo gtk-update-icon-cache -q -t -f "${PREFIX}/share/icons/hicolor" || true
fi

cat <<MSG

Installed Image Transient.
Launch it from your application menu or run:
  imagetransient
MSG
