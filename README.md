# TIFF Viewer - Multi-Page Float32 TIFF Viewer

A command-line application for viewing multi-page TIFF files with float32 data. The application automatically rescales float32 data to unsigned int (0-255) for display.

## Features

- Loads multi-page TIFF files with float32 data
- Automatic rescaling from float32 to uint8 for display
- Per-page dynamic range adjustment (min/max normalization)
- Keyboard and mouse navigation
- Re-orient the volume to view it along the Z, Y or X axis
- Cycle through Grayscale, Viridis and Inferno colormaps
- Lightweight Qt-based GUI

## Building

### With System Dependencies

```bash
cmake -B build -S .
cmake --build build
```

### With Conan (Recommended)

```bash
# Install dependencies
conan install . --output-folder=build/debug --build=missing -s build_type=Debug

# Configure and build
cmake --preset conan-debug
cmake --build --preset conan-debug
```

For release builds:

```bash
conan install . --output-folder=build/release --build=missing -s build_type=Release
cmake --preset conan-release
cmake --build --preset conan-release
```

## Usage

```bash
./build/tiffview <tiff_file>
```

### Example

```bash
./build/tiffview /path/to/your/data.tif
```

## Keyboard Controls

| Key | Action |
|-----|--------|
| Up Arrow / Down Arrow | Navigate one page forward/backward |
| Page Up / Page Down | Jump 5 pages forward/backward |
| Home | Go to first page |
| End | Go to last page |
| Mouse Wheel | Navigate pages (hold Ctrl to jump 5 pages) |
| Ctrl+1 | View along the Z axis (default): pages are the stack's slices |
| Ctrl+2 | View along the Y axis |
| Ctrl+3 | View along the X axis |
| Ctrl+T | Cycle the view axis Z → Y → X → Z |
| Z / X | Zoom in / out |
| R | Fit image to window |
| C | Cycle colormap: Grayscale → Viridis → Inferno → Grayscale |
| Q | Quit |

Navigation keys act on the current view axis and wrap around at the ends. Each
axis remembers its own position, so switching back returns to where you left
off. The window title shows the axis and position, e.g. `Y 120/512`.

## Data Handling

- **Input**: Multi-page TIFF files with 32-bit floating-point data
- **Rescaling**: Each page is independently rescaled using min/max normalization:
  - Find minimum and maximum values in the current page
  - Map [min, max] → [0, 255] for display
  - Handles constant-value images gracefully

## Requirements

- Qt6 (Core, Gui, Widgets)
- libtiff
- C++20 compiler

## Static build

Produces a single `tiffview` executable with Qt and libtiff linked in; the only
runtime dependencies are glibc (`libc`, `libm`). It runs on Linux x86-64
systems with glibc >= the one it was built with (2.39 on Ubuntu 24.04) and an
X11 server (Wayland via XWayland).

One-time setup (Ubuntu 24.04; Ubuntu ships only shared Qt, so it is built from
source into `$HOME/opt`):

```sh
sudo apt install bison meson libx11-xcb-dev libxkbcommon-x11-dev libxrender-dev \
    libdrm-dev libfontconfig-dev libxcb-cursor-dev libxcb-icccm4-dev \
    libxcb-image0-dev libxcb-keysyms1-dev libxcb-render-util0-dev \
    libxcb-shape0-dev libxcb-sync-dev libxcb-xfixes0-dev libxcb-xinerama0-dev \
    libxcb-randr0-dev libxcb-shm0-dev libxcb-xkb-dev libxcb-util-dev
./scripts/build-xkbcommon-static.sh   # Ubuntu has no libxkbcommon.a
./scripts/build-qt-static.sh          # 20-40 minutes
```

Then:

```sh
cmake --preset static
cmake --build --preset static
```

The binary is `build/static/tiffview` (stripped, ~23 MB). Check it with
`ldd build/static/tiffview`. The tests are not built in this configuration.

Qt is licensed under the LGPLv3; if you redistribute a statically linked
binary you must also let recipients relink against a different Qt.

## Implementation Details

The viewer uses the existing `tomocam::Array` and TIFF I/O infrastructure:
- Reads float32 TIFF data using `tomocam::tiff::read<float>()`
- Converts to grayscale images with per-page normalization
- Displays using Qt's QGraphicsView for efficient rendering

## Notes

- The rescaling is performed per-page, so each page is normalized independently
- This ensures optimal contrast for each individual page
- The original float32 data is preserved in memory (rescaling only affects display)
