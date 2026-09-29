#include "colormap.h"

QVector<QRgb> colorTableFor(tomocam::colormap::Colormap cm) {
    auto lut = tomocam::colormap::buildLut(cm);
    QVector<QRgb> table(256);
    for (int i = 0; i < 256; i++) table[i] = qRgb(lut[i][0], lut[i][1], lut[i][2]);
    return table;
}
