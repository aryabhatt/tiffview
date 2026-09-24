#!/usr/bin/env bash

# Build static libxkbcommon + libxkbcommon-x11 (Ubuntu ships only .so).
# Requires: meson ninja bison (sudo apt install meson bison).
set -euo pipefail

VER=1.6.0
PREFIX=${PREFIX:-$HOME/opt/static-deps}
WORK=${WORK:-$HOME/src/xkbcommon-static}

mkdir -p "$WORK" && cd "$WORK"
[ -f libxkbcommon-$VER.tar.xz ] ||
    curl -fLO https://xkbcommon.org/download/libxkbcommon-$VER.tar.xz
tar xf libxkbcommon-$VER.tar.xz
cd libxkbcommon-$VER

meson setup build --prefix="$PREFIX" --libdir=lib --buildtype=release \
    -Ddefault_library=static -Denable-wayland=false -Denable-docs=false \
    -Denable-tools=false -Denable-x11=true
meson compile -C build
meson install -C build
