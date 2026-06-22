#!/usr/bin/env bash
set -euo pipefail
sudo apt update
sudo apt install -y build-essential cmake ninja-build qt6-base-dev ffmpeg
exec "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/_install-common.sh"
