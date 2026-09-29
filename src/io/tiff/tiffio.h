#ifndef TIFFIO__H
#define TIFFIO__H

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <format>
#include <iostream>
#include <stdexcept>
#include <tiff.h>
#include <tiffio.h>
#include <type_traits>

#include "../array.h"
#include "normalize.h"

namespace tomocam::tiff {

    // contrain T to be unsigned integer or floating point type
    template <typename T>
    concept U = std::unsigned_integral<T> || std::floating_point<T>;

    // Normalizes each slice independently using its own min/max, rather
    // than a single min/max over the whole volume. This matches the
    // per-slice normalization used by the lazy single-slice reader, and
    // avoids a full-volume pass before any pixels can be produced.
    template <typename U>
    auto normalize_to_u8(const Array<U> &data) -> Array<uint8_t> {
        Array<uint8_t> result(data.dims());
        if (data.size() == 0) return result;

        size_t sliceSize = size_t(data.dims().n1) * data.dims().n2;
        for (uint32_t i = 0; i < data.dims().n0; i++)
            normalizeSliceToU8(&data.slice(i)[0], &result.slice(i)[0], sliceSize);

        return result;
    }

    template <typename U>
    auto read_data(std::string filename) -> Array<U> {

        // open the TIFF file
        TIFF *tif_ = TIFFOpen(filename.c_str(), "r");

        // count number of projections
        uint32_t npages = 0;
        do { npages++; } while (TIFFReadDirectory(tif_));

        // get image size
        uint16_t bits, format;
        uint32_t w, h;
        TIFFGetField(tif_, TIFFTAG_IMAGEWIDTH, &w);
        TIFFGetField(tif_, TIFFTAG_IMAGELENGTH, &h);
        TIFFGetField(tif_, TIFFTAG_BITSPERSAMPLE, &bits);
        TIFFGetField(tif_, TIFFTAG_SAMPLEFORMAT, &format);

        // allocate memory
        uint32_t nscls = static_cast<uint32_t>(npages);
        uint32_t nrows = static_cast<uint32_t>(h);
        uint32_t ncols = static_cast<uint32_t>(w);
        Array<U> data(dims_t{nscls, nrows, ncols});

        tsize_t line_size = TIFFScanlineSize(tif_);
        if (line_size != (w * sizeof(U))) {
            std::cerr << "Error: line_size, width mismatch" << std::endl;
            exit(1);
        }
        U *buf = static_cast<U *>(_TIFFmalloc(line_size));
        for (uint32_t i = 0; i < nscls; i++) {
            TIFFSetDirectory(tif_, static_cast<tdir_t>(i));
            for (uint32_t j = 0; j < h; j++) {
                if (TIFFReadScanline(tif_, buf, static_cast<uint32_t>(j)) < 0) {
                    std::cerr << "Error: failed to read scanline: " << i
                              << std::endl;
                    exit(1);
                }
                for (uint32_t k = 0; k < ncols; k++) data[{i, j, k}] = buf[k];
            }
        }
        _TIFFfree(buf);
        TIFFClose(tif_);
        return data;
    }

    inline auto read(std::string filename) -> Array<uint8_t> {
        // read the header to determine data type
        TIFF *tif_ = TIFFOpen(filename.c_str(), "r");
        uint16_t format, bits;
        TIFFGetField(tif_, TIFFTAG_BITSPERSAMPLE, &bits);
        TIFFGetField(tif_, TIFFTAG_SAMPLEFORMAT, &format);
        TIFFClose(tif_);

        if (bits == 8) {
            return read_data<uint8_t>(filename);
        } else if (bits == 16) {
            return normalize_to_u8(read_data<uint16_t>(filename));
        } else if (bits == 32) {
            if (format == SAMPLEFORMAT_IEEEFP) {
                return normalize_to_u8(read_data<float>(filename));
            } else
                return normalize_to_u8(read_data<uint32_t>(filename));
        } else if (bits == 64) {
            return normalize_to_u8(read_data<double>(filename));
        } else {
            throw std::runtime_error(std::format(
                "Unsupported TIFF data format: bits={}, format={}", bits, format));
        }
        return Array<uint8_t>(); // should never reach here
    }

} // namespace tomocam::tiff
#endif // TIFFIO__H
