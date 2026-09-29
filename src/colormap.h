#ifndef TIFFVIEW_COLORMAP__H
#define TIFFVIEW_COLORMAP__H

#include <QRgb>
#include <QVector>

#include "colormap_lut.h"

// Qt-facing adapter over colormap_lut's Qt-free core: converts a 256-entry
// RGB lookup table into the QVector<QRgb> that QImage::setColorTable()
// expects.
QVector<QRgb> colorTableFor(tomocam::colormap::Colormap cm);

#endif // TIFFVIEW_COLORMAP__H
