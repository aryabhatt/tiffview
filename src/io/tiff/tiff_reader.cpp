#include "tiff_reader.h"
#include "normalize.h"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <tiff.h>
#include <type_traits>
#include <utility>

namespace tomocam::tiff {

    namespace {

        TiffMeta readMetaFromDirectory0(TIFF *tif) {
            TiffMeta m;
            uint32_t w = 0, h = 0;
            uint16_t bits = 0, format = 0;
            TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
            TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
            TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bits);
            TIFFGetField(tif, TIFFTAG_SAMPLEFORMAT, &format);
            m.width = w;
            m.height = h;
            m.bits_per_sample = bits;
            m.sample_format = format;
            return m;
        }

        uint32_t countPages(TIFF *tif) {
            uint32_t n = 1;
            while (TIFFReadDirectory(tif)) n++;
            return n;
        }

    } // namespace

    TiffMeta probe(const std::string &filename) {
        TIFF *tif = TIFFOpen(filename.c_str(), "r");
        if (!tif)
            throw std::runtime_error(
                std::format("Failed to open TIFF file: {}", filename));

        // Read dimension/format fields from directory 0 BEFORE walking the
        // directory chain to count pages, so they never come from whatever
        // directory the counting loop happens to land on last.
        TiffMeta m = readMetaFromDirectory0(tif);
        m.n_pages = countPages(tif);
        TIFFClose(tif);
        return m;
    }

    TiffSliceReader::TiffSliceReader(std::string filename)
        : tif_(nullptr), filename_(std::move(filename)) {
        tif_ = TIFFOpen(filename_.c_str(), "r");
        if (!tif_)
            throw std::runtime_error(
                std::format("Failed to open TIFF file: {}", filename_));

        meta_ = readMetaFromDirectory0(tif_);
        meta_.n_pages = countPages(tif_);
        TIFFSetDirectory(tif_, 0);
    }

    TiffSliceReader::~TiffSliceReader() {
        if (tif_) TIFFClose(tif_);
    }

    template <typename T>
    std::vector<uint8_t> TiffSliceReader::readAndNormalize(uint32_t index) {
        uint32_t w = meta_.width;
        uint32_t h = meta_.height;

        if (!TIFFSetDirectory(tif_, static_cast<tdir_t>(index)))
            throw std::runtime_error(
                std::format("Failed to seek to TIFF directory {}", index));

        tsize_t line_size = TIFFScanlineSize(tif_);
        if (line_size != static_cast<tsize_t>(w * sizeof(T)))
            throw std::runtime_error(
                std::format("TIFF scanline size mismatch reading slice {}", index));

        std::vector<T> raw(size_t(w) * h);
        T *buf = static_cast<T *>(_TIFFmalloc(line_size));
        if (!buf)
            throw std::runtime_error("Failed to allocate TIFF scanline buffer");

        for (uint32_t j = 0; j < h; j++) {
            if (TIFFReadScanline(tif_, buf, j) < 0) {
                _TIFFfree(buf);
                throw std::runtime_error(
                    std::format("Failed to read scanline {} of slice {}", j, index));
            }
            std::copy_n(buf, w, raw.data() + size_t(j) * w);
        }
        _TIFFfree(buf);

        std::vector<uint8_t> out(size_t(w) * h);
        if constexpr (std::is_same_v<T, uint8_t>) {
            std::copy(raw.begin(), raw.end(), out.begin());
        } else {
            normalizeSliceToU8<T>(raw.data(), out.data(), raw.size());
        }
        return out;
    }

    std::vector<uint8_t> TiffSliceReader::readSliceNormalized(uint32_t index) {
        if (index >= meta_.n_pages)
            throw std::runtime_error(std::format(
                "Slice index {} out of range (n_pages={})", index, meta_.n_pages));

        uint16_t bits = meta_.bits_per_sample;
        uint16_t format = meta_.sample_format;

        if (bits == 8) return readAndNormalize<uint8_t>(index);
        if (bits == 16) return readAndNormalize<uint16_t>(index);
        if (bits == 32) {
            if (format == SAMPLEFORMAT_IEEEFP) return readAndNormalize<float>(index);
            return readAndNormalize<uint32_t>(index);
        }
        if (bits == 64) return readAndNormalize<double>(index);

        throw std::runtime_error(std::format(
            "Unsupported TIFF data format: bits={}, format={}", bits, format));
    }

} // namespace tomocam::tiff
