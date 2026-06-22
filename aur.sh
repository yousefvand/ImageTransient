#!/usr/bin/env bash
set -euo pipefail

PUSH_TO_AUR=1
if [[ "${1:-}" == "--no-push" ]]; then
  PUSH_TO_AUR=0
  shift
fi

if [[ "$#" -ne 0 ]]; then
  echo "Usage: ./aur.sh [--no-push]" >&2
  exit 1
fi

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "ERROR: missing required command: $1" >&2
    exit 1
  }
}

read_cmake_version() {
  awk '
    /project\(/ { in_project=1 }
    in_project && /VERSION[[:space:]]+[0-9]+\.[0-9]+\.[0-9]+/ {
      for (i = 1; i <= NF; i++) {
        if ($i == "VERSION") { print $(i + 1); exit }
      }
    }
  ' "$PROJECT_DIR/CMakeLists.txt"
}

need_cmd git
need_cmd curl
need_cmd makepkg
need_cmd awk
need_cmd sha256sum

PKGNAME="${AUR_PKGNAME:-imagetransient}"
PKGVER="${PKGVER:-$(read_cmake_version)}"
PKGREL="${PKGREL:-1}"
GITHUB_REPO="${GITHUB_REPO:-yousefvand/ImageTransient}"
TAG="${TAG:-v${PKGVER}}"
SOURCE_URL="https://github.com/${GITHUB_REPO}/archive/refs/tags/${TAG}.tar.gz"
AUR_REMOTE="${AUR_REMOTE:-ssh://aur@aur.archlinux.org/${PKGNAME}.git}"
AUR_WORKDIR="${AUR_WORKDIR:-$PROJECT_DIR/build/aur/${PKGNAME}}"
PKGDESC="Compact Qt 6 app for creating MP4 transition videos from two still images"
LICENSE="GPL-3.0-or-later"

if [[ -z "$PKGVER" ]]; then
  echo "ERROR: could not read project version from CMakeLists.txt" >&2
  exit 1
fi

TMPDIR_CHECK="$(mktemp -d)"
trap 'rm -rf "$TMPDIR_CHECK"' EXIT

echo "==> Checking source tarball"
echo "Source: $SOURCE_URL"
curl -fL --retry 3 --retry-delay 2 -o "$TMPDIR_CHECK/source.tar.gz" "$SOURCE_URL"
SHA256SUM="$(sha256sum "$TMPDIR_CHECK/source.tar.gz" | awk '{print $1}')"
echo "SHA256: $SHA256SUM"

mkdir -p "$(dirname "$AUR_WORKDIR")"

if [[ ! -d "$AUR_WORKDIR/.git" ]]; then
  echo "==> Creating local AUR checkout: $AUR_WORKDIR"
  if git clone "$AUR_REMOTE" "$AUR_WORKDIR"; then
    :
  else
    echo "==> AUR clone failed; creating a new local AUR repo for first push"
    rm -rf "$AUR_WORKDIR"
    mkdir -p "$AUR_WORKDIR"
    git -C "$AUR_WORKDIR" init
    git -C "$AUR_WORKDIR" checkout -B master
    git -C "$AUR_WORKDIR" remote add origin "$AUR_REMOTE"
  fi
fi

cd "$AUR_WORKDIR"

git checkout -B master >/dev/null

find . -mindepth 1 -maxdepth 1 ! -name .git -exec rm -rf {} +

cat > PKGBUILD <<PKGBUILD_EOF
# Maintainer: Remisa Phillips <remisa.yousefvand@gmail.com>

pkgname=${PKGNAME}
pkgver=${PKGVER}
pkgrel=${PKGREL}
pkgdesc='${PKGDESC}'
arch=('x86_64')
url='https://github.com/${GITHUB_REPO}'
license=('${LICENSE}')
depends=('qt6-base' 'ffmpeg')
makedepends=('cmake' 'ninja' 'gcc')
source=("\${pkgname}-\${pkgver}.tar.gz::${SOURCE_URL}")
sha256sums=('${SHA256SUM}')

_find_srcdir() {
  find "\${srcdir}" -mindepth 1 -maxdepth 1 -type d -name 'ImageTransient-*' -print -quit
}

build() {
  cd "\$(_find_srcdir)"
  cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_SKIP_RPATH=ON
  cmake --build build --parallel "\$(nproc)"
}

package() {
  cd "\$(_find_srcdir)"
  DESTDIR="\${pkgdir}" cmake --install build
  install -Dm644 LICENSE "\${pkgdir}/usr/share/licenses/\${pkgname}/LICENSE"
}
PKGBUILD_EOF

makepkg --printsrcinfo > .SRCINFO

# Stage all changes so old accidental source files can be removed from AUR.
git add -A

# Safety guard: added or modified files must be only PKGBUILD and .SRCINFO.
bad_added_or_modified="$(git diff --cached --name-only --diff-filter=AMT | grep -Ev '^(PKGBUILD|\.SRCINFO)$' || true)"
if [[ -n "$bad_added_or_modified" ]]; then
  echo "ERROR: refusing to add/modify non-packaging files in AUR:" >&2
  echo "$bad_added_or_modified" >&2
  exit 1
fi

if git diff --cached --quiet; then
  echo "==> No AUR changes to commit."
  exit 0
fi

echo "==> Staged AUR changes"
git diff --cached --stat

git commit -m "Update ${PKGNAME} to ${PKGVER}-${PKGREL}"

if [[ "$PUSH_TO_AUR" -eq 1 ]]; then
  echo "==> Pushing to AUR: $AUR_REMOTE"
  git push -u origin master
  echo "==> Done. AUR package submitted/updated: ${PKGNAME}"
else
  echo "==> --no-push selected. Nothing was pushed."
  echo "AUR working directory: $AUR_WORKDIR"
fi
