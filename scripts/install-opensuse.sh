#!/usr/bin/env bash
set -euo pipefail
sudo zypper --non-interactive install -t pattern devel_C_C++ || true
if ! sudo zypper --non-interactive install cmake ninja qt6-base-devel; then
  sudo zypper --non-interactive install cmake ninja libqt6-qtbase-devel
fi
if ! sudo zypper --non-interactive install ffmpeg; then
  echo "WARNING: FFmpeg was not installed. Enable the required multimedia repository, then install ffmpeg." >&2
fi
exec "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/_install-common.sh"
