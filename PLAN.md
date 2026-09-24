# Plan: static `tiffview` binary (no Qt needed on host)

## Context

`tiffview` (Qt6 Widgets + libtiff; see `CMakeLists.txt`, sources in `src/`) currently links the distro's *shared* Qt 6.4.2, so users need Qt installed. Ubuntu ships no static Qt, so a self-contained binary requires building Qt from source with `-static` and linking against it.

Decisions:

- **Packaging:** static Qt build (single executable).
- **Portability:** same-or-newer glibc. Build on Ubuntu 24.04 (glibc 2.39); the binary runs on distros with glibc >= 2.39. No container.

Result: one `tiffview` executable with Qt (Core/Gui/Widgets + xcb platform plugin), libtiff and its codecs baked in. Runtime dependency: glibc only (libstdc++/libgcc are linked statically).

## Manual steps (run these yourself)

`sudo` needs a password, so the package install and the Qt build are run by hand.

### 1. Install apt packages

```sh
sudo apt update
sudo apt install \
    libx11-xcb-dev libxkbcommon-x11-dev libxrender-dev libdrm-dev libfontconfig-dev \
    libxcb-cursor-dev libxcb-icccm4-dev libxcb-image0-dev libxcb-keysyms1-dev \
    libxcb-render-util0-dev libxcb-shape0-dev libxcb-sync-dev libxcb-xfixes0-dev \
    libxcb-xinerama0-dev libxcb-randr0-dev libxcb-shm0-dev libxcb-xkb-dev libxcb-util-dev
```

Already installed on this machine (nothing to do): `libxcb1-dev libxkbcommon-dev libx11-dev libxext-dev libfreetype-dev libpng-dev libjpeg-dev zlib1g-dev libzstd-dev libexpat1-dev libtiff-dev libwebp-dev liblerc-dev libjbig-dev libdeflate-dev liblzma-dev ninja-build cmake g++ perl python3`.

Note: `-dev` packages ship the `.a` archives needed for static linking. Check with `ls /usr/lib/x86_64-linux-gnu/libfontconfig.a /usr/lib/x86_64-linux-gnu/libX11-xcb.a /usr/lib/x86_64-linux-gnu/libxkbcommon.a`.

### 2. Build static Qt (one-time, ~20-40 min on 32 cores)

Only `qtbase` is needed. It installs to `$HOME/opt/qt6-static`, outside the repo.

```sh
./scripts/build-qt-static.sh
```

What the script does (equivalent manual commands):

```sh
QT_VER=6.8.3
PREFIX=$HOME/opt/qt6-static
WORK=$HOME/src/qt-static && mkdir -p $WORK && cd $WORK
curl -LO https://download.qt.io/official_releases/qt/6.8/$QT_VER/submodules/qtbase-everywhere-src-$QT_VER.tar.xz
tar xf qtbase-everywhere-src-$QT_VER.tar.xz
mkdir -p build && cd build
../qtbase-everywhere-src-$QT_VER/configure \
    -static -release -prefix $PREFIX \
    -nomake examples -nomake tests \
    -no-opengl -no-dbus -no-icu -no-openssl \
    -no-feature-sql -no-feature-network -no-feature-testlib \
    -qt-zlib -qt-libpng -qt-libjpeg -qt-harfbuzz -qt-pcre -qt-doubleconversion \
    -fontconfig -xcb -xkbcommon \
    -- -GNinja
cmake --build . --parallel
cmake --install .
```

If a `.a` is missing at link time (e.g. an xcb util library), re-run configure with `-qt-xcb` to use Qt's bundled xcb libs.

Sanity check: `$HOME/opt/qt6-static/bin/qmake -query QT_VERSION` (or `ls $HOME/opt/qt6-static/lib/libQt6Widgets.a`).

## Code changes (done by Claude after the manual steps)

1. **`scripts/build-qt-static.sh`** (new): the commands above, so the Qt build is reproducible.
2. **`CMakeLists.txt`:** add `option(TIFFVIEW_STATIC "Static-Qt build" OFF)`. When ON:
   - Qt resolves to the static prefix through `CMAKE_PREFIX_PATH`. Qt6's CMake auto-links the xcb platform plugin and its libs for static builds (verify no manual `Q_IMPORT_PLUGIN` is needed).
   - Link libtiff statically with its codec deps via `pkg_check_modules(TIFF_STATIC REQUIRED IMPORTED_TARGET libtiff-4)` and `pkg-config --static` flags (`-ltiff -lwebp -lLerc -ljbig -lzstd -llzma -ljpeg -ldeflate -lz ...`), for the `tiffview` target only. The `tests` target keeps `TIFF::TIFF`. Fallback if a codec `.a` is missing: build a minimal libtiff (zlib only) into the same prefix.
   - Add `-static-libstdc++ -static-libgcc` and strip in Release (`-s`).
3. **`CMakePresets.json`:** add a `static` configure preset (Ninja, Release, `binaryDir` `build/static`, `CMAKE_PREFIX_PATH=$env{HOME}/opt/qt6-static`, `TIFFVIEW_STATIC=ON`) and a matching build preset.
4. **`README.md`:** "Static build" section: run the script once, then `cmake --preset static && cmake --build --preset static`. Note glibc >= 2.39, artifact at `build/static/tiffview`, and that Qt is LGPLv3 (static linking carries relinking obligations if redistributed). No release/upload unless asked.

## Verification

- `ldd build/static/tiffview` shows only `libc`, `libm`, `ld-linux` (no `libQt6*`, `libtiff`, `libxcb`, `libstdc++`).
- `LD_DEBUG=libs ./build/static/tiffview lego2.tiff` shows no Qt or tiff library loads.
- Headless smoke test: `QT_QPA_PLATFORM=offscreen ./build/static/tiffview lego2.tiff` loads the file and starts the event loop.
- Real display, clean env: `env -i DISPLAY=$DISPLAY HOME=$HOME ./build/static/tiffview lego2.tiff`. The window opens, arrows and wheel page through slices, and "Page n/N" text renders (confirms the fontconfig/freetype path).
- The existing `debug` and `release` presets still configure and build unchanged (option defaults to OFF).

## Risks

- Static xcb/fontconfig `.a` availability (mitigated by the package list; fall back to `-qt-xcb`).
- Binary is tied to glibc 2.39. Older distros would need a build in an older-glibc container (out of scope).
- Qt LGPL obligations when distributing.
