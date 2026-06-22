#!/usr/bin/env bash
set -euo pipefail
sudo pacman -S --needed base-devel cmake ninja qt6-base ffmpeg
exec "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/_install-common.sh"
