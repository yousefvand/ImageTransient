#!/usr/bin/env bash
set -euo pipefail
sudo apk add build-base cmake ninja qt6-qtbase-dev ffmpeg
exec "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/_install-common.sh"
