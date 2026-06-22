#!/usr/bin/env bash
set -euo pipefail
sudo dnf install -y gcc-c++ cmake ninja-build qt6-qtbase-devel
if ! sudo dnf install -y ffmpeg; then
  echo "WARNING: FFmpeg was not installed. Enable RPM Fusion or another multimedia repository, then install ffmpeg." >&2
fi
exec "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/_install-common.sh"
