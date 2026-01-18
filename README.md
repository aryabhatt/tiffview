# tiffview

A lightweight TIFF image viewer built with Rust and egui.

## Features

- View TIFF images in a graphical window
- Support for multi-page TIFF files
- Interactive scaling and navigation
- Cross-platform compatibility

## Installation

```bash
cargo build --release
```

The binary will be available at `target/release/tiffview`.

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
