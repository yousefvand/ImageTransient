#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ ! -r /etc/os-release ]]; then
  echo "Cannot detect Linux distribution. Install Qt 6, CMake, Ninja and FFmpeg, then run ./scripts/build-linux.sh." >&2
  exit 1
fi

. /etc/os-release
id="${ID,,}"
id_like="${ID_LIKE:-}"

case "$id" in
  arch|manjaro|endeavouros|garuda)
    exec "$ROOT/scripts/install-arch.sh"
    ;;
  debian|ubuntu|linuxmint|pop)
    exec "$ROOT/scripts/install-debian-ubuntu.sh"
    ;;
  fedora|rhel|centos|rocky|almalinux)
    exec "$ROOT/scripts/install-fedora.sh"
    ;;
  opensuse*|suse|sles)
    exec "$ROOT/scripts/install-opensuse.sh"
    ;;
  alpine)
    exec "$ROOT/scripts/install-alpine.sh"
    ;;
esac

case "$id_like" in
  *arch*) exec "$ROOT/scripts/install-arch.sh" ;;
  *debian*|*ubuntu*) exec "$ROOT/scripts/install-debian-ubuntu.sh" ;;
  *fedora*|*rhel*) exec "$ROOT/scripts/install-fedora.sh" ;;
  *suse*) exec "$ROOT/scripts/install-opensuse.sh" ;;
  *alpine*) exec "$ROOT/scripts/install-alpine.sh" ;;
esac

echo "Unsupported distribution ID=$ID." >&2
echo "Install Qt 6, CMake, Ninja and FFmpeg, then run ./scripts/build-linux.sh." >&2
exit 1
