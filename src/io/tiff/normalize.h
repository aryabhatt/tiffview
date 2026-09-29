#ifndef TOMOCAM_TIFF_NORMALIZE__H
#define TOMOCAM_TIFF_NORMALIZE__H

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>

namespace tomocam::tiff {

    template <typename T>
    concept Normalizable = std::unsigned_integral<T> || std::floating_point<T>;

    // Rescales n contiguous samples from src into dst using only this
    // slice's own min/max. If every value is equal, dst is filled with 0.
    // Used by both the eager whole-volume reader and the lazy per-slice
    // reader so visual output is identical regardless of which path loads
    // a given slice.
    template <Normalizable T>
    inline void normalizeSliceToU8(const T *src, uint8_t *dst, size_t n) {
        if (n == 0) return;

        T min_val = src[0];
        T max_val = src[0];
        for (size_t k = 1; k < n; k++) {
            if (src[k] < min_val) min_val = src[k];
            if (src[k] > max_val) max_val = src[k];
        }

        T range = max_val - min_val;
        if (range == 0) {
            std::fill(dst, dst + n, uint8_t{0});
            return;
        }

        for (size_t k = 0; k < n; k++)
            dst[k] = static_cast<uint8_t>(
                255.0 * (double(src[k]) - double(min_val)) / double(range));
    }

} // namespace tomocam::tiff
#endif // TOMOCAM_TIFF_NORMALIZE__H
