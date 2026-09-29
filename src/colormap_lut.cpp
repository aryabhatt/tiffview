#include "colormap_lut.h"

#include <cmath>

namespace tomocam::colormap {

    namespace {

        constexpr int kAnchorCount = 17;
        constexpr int kSegments = kAnchorCount - 1;

        // Real matplotlib viridis/inferno RGB samples at t = i/16, i=0..16.
        constexpr std::array<Rgb, kAnchorCount> kViridisAnchors = {{
            {68, 1, 84},
            {72, 24, 106},
            {71, 45, 123},
            {66, 64, 134},
            {59, 82, 139},
            {51, 99, 141},
            {44, 114, 142},
            {38, 130, 142},
            {33, 145, 140},
            {31, 160, 136},
            {40, 174, 128},
            {63, 188, 115},
            {94, 201, 98},
            {132, 212, 75},
            {173, 220, 48},
            {216, 226, 25},
            {253, 231, 37},
        }};

        constexpr std::array<Rgb, kAnchorCount> kInfernoAnchors = {{
            {0, 0, 4},
            {11, 7, 36},
            {33, 12, 74},
            {61, 9, 101},
            {87, 16, 110},
            {113, 25, 110},
            {138, 34, 106},
            {163, 44, 97},
            {188, 55, 84},
            {210, 70, 68},
            {228, 90, 49},
            {241, 115, 29},
            {249, 142, 9},
            {252, 172, 17},
            {249, 203, 53},
            {242, 234, 105},
            {252, 255, 164},
        }};

        std::array<Rgb, 256>
        interpolate(const std::array<Rgb, kAnchorCount> &anchors) {
            std::array<Rgb, 256> lut;
            for (int x = 0; x < 256; x++) {
                double pos = double(x) * kSegments / 255.0;
                int seg = static_cast<int>(pos);
                if (seg >= kSegments) seg = kSegments - 1;
                double t = pos - seg;

                const Rgb &a = anchors[seg];
                const Rgb &b = anchors[seg + 1];
                for (int c = 0; c < 3; c++)
                    lut[x][c] = static_cast<uint8_t>(
                        std::lround(a[c] + (double(b[c]) - double(a[c])) * t));
            }
            return lut;
        }

    } // namespace

    Colormap next(Colormap cm) {
        switch (cm) {
            case Colormap::Grayscale: return Colormap::Viridis;
            case Colormap::Viridis: return Colormap::Inferno;
            case Colormap::Inferno: return Colormap::Grayscale;
        }
        return Colormap::Grayscale;
    }

    const char *name(Colormap cm) {
        switch (cm) {
            case Colormap::Grayscale: return "Grayscale";
            case Colormap::Viridis: return "Viridis";
            case Colormap::Inferno: return "Inferno";
        }
        return "Grayscale";
    }

    std::array<Rgb, 256> buildLut(Colormap cm) {
        if (cm == Colormap::Viridis) return interpolate(kViridisAnchors);
        if (cm == Colormap::Inferno) return interpolate(kInfernoAnchors);

        std::array<Rgb, 256> lut;
        for (int i = 0; i < 256; i++) lut[i] = {uint8_t(i), uint8_t(i), uint8_t(i)};
        return lut;
    }

} // namespace tomocam::colormap
