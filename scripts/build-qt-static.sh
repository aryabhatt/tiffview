#!/usr/bin/env bash
# Build a static qtbase (Core/Gui/Widgets + xcb platform plugin) into
# $HOME/opt/qt6-static. See PLAN.md for the apt packages this needs.
set -euo pipefail

QT_VER=${QT_VER:-6.8.3}
PREFIX=${PREFIX:-$HOME/opt/qt6-static}
WORK=${WORK:-$HOME/src/qt-static}
SRC=qtbase-everywhere-src-$QT_VER

mkdir -p "$WORK" && cd "$WORK"
[ -f $SRC.tar.xz ] ||
    curl -fLO https://download.qt.io/official_releases/qt/${QT_VER%.*}/$QT_VER/submodules/$SRC.tar.xz
[ -d $SRC ] || tar xf $SRC.tar.xz

mkdir -p build && cd build
../$SRC/configure \
    -static -release -prefix "$PREFIX" \
    -nomake examples -nomake tests \
    -no-opengl -no-dbus -no-icu -no-openssl \
    -no-feature-sql -no-feature-network -no-feature-testlib \
    -qt-zlib -qt-libpng -qt-libjpeg -qt-harfbuzz -qt-pcre -qt-doubleconversion \
    -fontconfig -xcb -xkbcommon \
    -- -GNinja
cmake --build . --parallel
cmake --install .
