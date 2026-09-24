#include <QGraphicsPixmapItem>
#include <QMouseEvent>
#include <algorithm>
#include <cstdint>
#include <qevent.h>
#include <qgraphicsview.h>
#include <qnamespace.h>
#include <string>
#include <unistd.h>

#include "image_viewer.h"
#include "io/tiff/tiffio.h"

// wrap idx into [0, n) for any signed idx; n must be > 0
static int wrapIndex(int idx, int n) { return ((idx % n) + n) % n; }

ImageViewer::ImageViewer(const tomocam::Array<uint8_t> &images, QWidget *parent)
    : QGraphicsView(parent), imageStack(images), axis(0) {
    resetIndices();
    scene = new QGraphicsScene(this);
    setScene(scene);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setFocusPolicy(Qt::StrongFocus);
    setBackgroundBrush(QBrush(Qt::black));
    updateImage();
}

static const char *axisNames[3] = {"Z", "Y", "X"};

int ImageViewer::axisLength() const {
    tomocam::dims_t d = imageStack.dims();
    return static_cast<int>(axis == 0 ? d.n0 : axis == 1 ? d.n1 : d.n2);
}

void ImageViewer::resetIndices() {
    tomocam::dims_t d = imageStack.dims();
    indices[0] = 0;
    indices[1] = static_cast<int>(d.n1 / 2);
    indices[2] = static_cast<int>(d.n2 / 2);
}

void ImageViewer::updateImage() {
    scene->clear();
    QImage img = extractPlane(axis, indices[axis]);
    scene->addPixmap(QPixmap::fromImage(img));
    scene->setSceneRect(img.rect());

    setWindowTitle(QString("%1 %2/%3")
                       .arg(axisNames[axis])
                       .arg(indices[axis] + 1)
                       .arg(axisLength()));
}

void ImageViewer::setViewAxis(int newAxis) {
    if (newAxis < 0 || newAxis > 2 || newAxis == axis) return;
    axis = newAxis;
    updateImage();
    resetTransform();
    fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
}

// The 2D plane at position idx along the given axis. Dimensions are
// (n0, n1, n2) = (slices, rows, cols); the plane keeps the remaining two
// axes in their original order.
QImage ImageViewer::extractPlane(int ax, int idx) const {
    tomocam::dims_t d = imageStack.dims();
    const uint8_t *data = imageStack.begin();
    size_t i = static_cast<size_t>(idx);

    if (ax == 0) { // rows: n1, cols: n2, contiguous
        QImage img(d.n2, d.n1, QImage::Format_Grayscale8);
        const uint8_t *src = data + i * d.n1 * d.n2;
        for (uint32_t r = 0; r < d.n1; ++r)
            std::copy_n(src + size_t(r) * d.n2, d.n2, img.scanLine(r));
        return img;
    }
    if (ax == 1) { // rows: n0, cols: n2
        QImage img(d.n2, d.n0, QImage::Format_Grayscale8);
        for (uint32_t r = 0; r < d.n0; ++r) {
            const uint8_t *src = data + imageStack.flatIdx(r, i, 0);
            std::copy_n(src, d.n2, img.scanLine(r));
        }
        return img;
    }
    // ax == 2, rows: n0, cols: n1
    QImage img(d.n1, d.n0, QImage::Format_Grayscale8);
    for (uint32_t r = 0; r < d.n0; ++r) {
        uchar *line = img.scanLine(r);
        for (uint32_t c = 0; c < d.n1; ++c)
            line[c] = data[imageStack.flatIdx(r, c, i)];
    }
    return img;
}

void ImageViewer::wheelEvent(QWheelEvent *event) {
    int nImgs = axisLength();
    if (nImgs <= 0) return;

    int step = 1;
    if (event->modifiers() & Qt::ControlModifier) { step = 5; }

    int &idx = indices[axis];
    if (event->angleDelta().y() > 0) {
        idx = wrapIndex(idx + step, nImgs);
    } else {
        idx = wrapIndex(idx - step, nImgs);
    }
    updateImage();
}

void ImageViewer::mousePressEvent(QMouseEvent *event) {
    QGraphicsView::mousePressEvent(event);
}

void ImageViewer::updateImageStack(const tomocam::Array<uint8_t> &arr) {
    imageStack = arr;
    axis = 0;
    resetIndices();
    updateImage();
}

void ImageViewer::zoomIn() { scale(1.2, 1.2); }
void ImageViewer::zoomOut() { scale(1 / 1.2, 1 / 1.2); }

void ImageViewer::keyPressEvent(QKeyEvent *event) {

    if (imageStack.size() <= 0) {
        QGraphicsView::keyReleaseEvent(event);
        return;
    }

    if (event->modifiers() & Qt::ControlModifier) {
        switch (event->key()) {
            case Qt::Key_1: setViewAxis(0); return;
            case Qt::Key_2: setViewAxis(1); return;
            case Qt::Key_3: setViewAxis(2); return;
            case Qt::Key_T: setViewAxis((axis + 1) % 3); return;
            default: break;
        }
    }

    int nImgs = axisLength();
    int &idx = indices[axis];
    int oldIndex = idx;
    switch (event->key()) {
        case Qt::Key_Up: idx = wrapIndex(idx + 1, nImgs); break;
        case Qt::Key_Down: idx = wrapIndex(idx - 1, nImgs); break;
        case Qt::Key_PageUp: idx = wrapIndex(idx + 5, nImgs); break;
        case Qt::Key_PageDown: idx = wrapIndex(idx - 5, nImgs); break;
        case Qt::Key_Home: idx = 0; break;
        case Qt::Key_End: idx = nImgs - 1; break;
        case Qt::Key_Z: zoomIn(); break;
        case Qt::Key_X: zoomOut(); break;
        case Qt::Key_R: fitInView(scene->sceneRect(), Qt::KeepAspectRatio); break;
        default: QGraphicsView::keyPressEvent(event); return;
    }

    if (idx != oldIndex) { updateImage(); }
}
