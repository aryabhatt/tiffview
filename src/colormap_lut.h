#ifndef TOMOCAM_COLORMAP_LUT__H
#define TOMOCAM_COLORMAP_LUT__H

#include <array>
#include <cstdint>

namespace tomocam::colormap {

    enum class Colormap { Grayscale, Viridis, Inferno };

    // Returns the colormap that follows `cm` in the fixed cycle order
    // Grayscale -> Viridis -> Inferno -> Grayscale.
    Colormap next(Colormap cm);

    // Human-readable name, e.g. for a window-title suffix.
    const char *name(Colormap cm);

    using Rgb = std::array<uint8_t, 3>;

    // Builds a 256-entry RGB lookup table for the given colormap. Viridis
    // and Inferno are built by linearly interpolating 17 anchor points
    // (taken from matplotlib's real colormap data at t = i/16, i=0..16)
    // -- indistinguishable from the full 256-sample colormap at 8-bit
    // resolution. Grayscale is the identity ramp {i,i,i}. Kept Qt-free so
    // it's directly unit-testable.
    std::array<Rgb, 256> buildLut(Colormap cm);

} // namespace tomocam::colormap
#endif // TOMOCAM_COLORMAP_LUT__H
