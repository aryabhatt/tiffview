// #include <concepts>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <tiff.h>
#include <tiffio.h>
#include <type_traits>

#include "../array.h"

#ifndef TIFFIO__H
#define TIFFIO__H

namespace tomocam::tiff {

    template <typename T>
    concept single32_t = std::is_same_v<T, float> || std::is_same_v<T, uint32_t>;

    template <typename single32_t>
    inline Array<single32_t> read(std::string filename) {

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

        if (format != SAMPLEFORMAT_IEEEFP && format != SAMPLEFORMAT_UINT) {
            throw std::runtime_error("unsupported data type");
        }

        if (bits != sizeof(single32_t) * 8) {
            std::cerr << "Error: TIFF has " << bits
                      << "-bit samples, but template expects "
                      << sizeof(single32_t) * 8 << "-bit samples" << std::endl;
            TIFFClose(tif_);
            throw std::runtime_error("bit depth mismatch");
        }

        // allocate memory
        uint32_t nscls = static_cast<uint32_t>(npages);
        uint32_t nrows = static_cast<uint32_t>(h);
        uint32_t ncols = static_cast<uint32_t>(w);
        Array<single32_t> data(nscls, nrows, ncols);

        single32_t *buf = (single32_t *)_TIFFmalloc(w * sizeof(single32_t));

        tsize_t line_size = TIFFScanlineSize(tif_);
        if (line_size != (w * sizeof(single32_t))) {
            std::cerr << "Error: line_size, width mismatch" << std::endl;
            exit(1);
        }

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

    template <typename single32_t>
    inline void write(std::string filename, const Array<single32_t> &data) {

        // open file
        TIFF *tif_ = TIFFOpen(filename.c_str(), "w");

        uint32_t npages = static_cast<uint32_t>(data.nslices());
        uint32_t height = static_cast<uint32_t>(data.nrows());
        uint32_t width = static_cast<uint32_t>(data.ncols());

        single32_t *buf = (single32_t *)_TIFFmalloc(sizeof(single32_t) * width);

        for (uint32_t i = 0; i < npages; i++) {
            TIFFSetField(tif_, TIFFTAG_IMAGEWIDTH, width);
            TIFFSetField(tif_, TIFFTAG_IMAGELENGTH, height);
            TIFFSetField(tif_, TIFFTAG_SAMPLESPERPIXEL, 1);
            TIFFSetField(tif_, TIFFTAG_BITSPERSAMPLE, 32);
            if (std::is_same_v<single32_t, float>) {
                TIFFSetField(tif_, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
            } else {
                TIFFSetField(tif_, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
            }
            TIFFSetField(tif_, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
            TIFFSetField(tif_, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
            TIFFSetField(tif_, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
            TIFFSetField(tif_, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
            TIFFSetField(tif_, TIFFTAG_ROWSPERSTRIP, height); // one strip

            for (uint32_t j = 0; j < height; j++) {
                for (uint32_t k = 0; k < width; k++) buf[k] = data[{i, j, k}];

                if (TIFFWriteScanline(tif_, buf, j) < 0) {
                    std::cerr << "Error wrtiting data to tif file." << std::endl;
                    std::exit(2);
                }
            }
            TIFFWriteDirectory(tif_);
        }
        _TIFFfree(buf);
        TIFFClose(tif_);
    }
} // namespace tomocam::tiff
#endif // TIFFIO__H
