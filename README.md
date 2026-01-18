# tiffview

A lightweight multi-page TIFF image viewer CLI tool.

## Features

- View TIFF images in a graphical window
- Support for multi-page TIFF files
- Interactive scaling and navigation
- Cross-platform compatibility

## Installation

```bash
cargo build --release
cargo install --path .
```
Alternatively, install directly from GitHub.
```bash
cargo install --git https://github.com/aryabhatt/tiffview.git
```
The executable will be available in `~/.cargo/bin`. 


## Usage

```bash
tiffview <tiff-file>
```

Example:
```bash
tiffview test.tif
```

## Dependencies

- [eframe](https://github.com/emilk/egui)/[egui](https://github.com/emilk/egui) - Immediate mode GUI framework
- [tiff](https://github.com/image-rs/image-tiff) - TIFF format decoder
- [image](https://github.com/image-rs/image) - Image processing library

## License

BSD 2-Clause License - see [LICENSE](LICENSE) file for details.
