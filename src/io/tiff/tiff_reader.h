#ifndef TOMOCAM_TIFF_READER__H
#define TOMOCAM_TIFF_READER__H

#include <cstdint>
#include <string>
#include <vector>

#include <tiffio.h>

namespace tomocam::tiff {

    struct TiffMeta {
        uint32_t n_pages = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        uint16_t bits_per_sample = 0;
        uint16_t sample_format = 0; // SAMPLEFORMAT_*
    };

    // Bytes of the normalized (uint8, 1 byte/pixel) volume this file would
    // produce if fully loaded -- used to decide whether the whole stack
    // fits within a configured cache budget.
    inline uint64_t normalizedVolumeBytes(const TiffMeta &m) {
        return uint64_t(m.n_pages) * m.width * m.height;
    }

    // Fast metadata-only probe: opens the file, reads width/height/bits/
    // format from directory 0 first, then counts pages by walking
    // TIFFReadDirectory (IFD-header traversal only, no scanline decode).
    // Throws std::runtime_error if the file cannot be opened.
    TiffMeta probe(const std::string &filename);

    // Opens one TIFF handle and keeps it open for the lifetime of the
    // object, for repeated random-access single-slice reads. NOT
    // thread-safe: intended to be owned and used exclusively by a single
    // dedicated worker thread.
    class TiffSliceReader {
      public:
        explicit TiffSliceReader(std::string filename); // throws std::runtime_error
        ~TiffSliceReader();

        TiffSliceReader(const TiffSliceReader &) = delete;
        TiffSliceReader &operator=(const TiffSliceReader &) = delete;

        const TiffMeta &meta() const { return meta_; }

        // Reads page `index`, decodes it to its native sample type, and
        // produces a per-slice-normalized uint8 buffer of width*height
        // bytes (a raw copy, no rescale, when the native type is already
        // uint8 -- matches tomocam::tiff::read()'s 8-bit passthrough).
        // Throws std::runtime_error on I/O failure or out-of-range index.
        std::vector<uint8_t> readSliceNormalized(uint32_t index);

      private:
        template <typename T>
        std::vector<uint8_t> readAndNormalize(uint32_t index);

        TIFF *tif_;
        std::string filename_;
        TiffMeta meta_;
    };

} // namespace tomocam::tiff
#endif // TOMOCAM_TIFF_READER__H
