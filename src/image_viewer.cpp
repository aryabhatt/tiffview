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
    : QGraphicsView(parent), imageStack(images), currentIndex(0) {
    scene = new QGraphicsScene(this);
    setScene(scene);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setFocusPolicy(Qt::StrongFocus);
    setBackgroundBrush(QBrush(Qt::black));
    updateImage();
}

void ImageViewer::updateImage() {
    scene->clear();
    QImage img = ArrayToQImage(imageStack.slice(currentIndex));
    scene->addPixmap(QPixmap::fromImage(img));
    scene->setSceneRect(img.rect());

    setWindowTitle(
        QString("Page %1/%2").arg(currentIndex + 1).arg(imageStack.nslices()));
}

QImage ImageViewer::ArrayToQImage(const tomocam::Slice<uint8_t> &arr) {
    int width = arr.ncols;
    int height = arr.nrows;
    QImage img(width, height, QImage::Format_Grayscale8);
    for (int y = 0; y < height; ++y) {
        uchar *line = img.scanLine(y);
        for (int x = 0; x < width; ++x) {
            line[x] = static_cast<uchar>(arr[size_t(y) * width + x]);
        }
    }
    return img;
}

void ImageViewer::wheelEvent(QWheelEvent *event) {
    int nImgs = imageStack.nslices();
    if (nImgs <= 0) return;

    int step = 1;
    if (event->modifiers() & Qt::ControlModifier) { step = 5; }

    if (event->angleDelta().y() > 0) {
        currentIndex = wrapIndex(currentIndex + step, nImgs);
    } else {
        currentIndex = wrapIndex(currentIndex - step, nImgs);
    }
    updateImage();
}

void ImageViewer::mousePressEvent(QMouseEvent *event) {
    QGraphicsView::mousePressEvent(event);
}

void ImageViewer::updateImageStack(const tomocam::Array<uint8_t> &arr) {
    imageStack = arr;
    currentIndex = 0;
    updateImage();
}

void ImageViewer::zoomIn() { scale(1.2, 1.2); }
void ImageViewer::zoomOut() { scale(1 / 1.2, 1 / 1.2); }

void ImageViewer::keyPressEvent(QKeyEvent *event) {

    if (imageStack.size() <= 0) {
        QGraphicsView::keyReleaseEvent(event);
        return;
    }

    int nImgs = imageStack.nslices();
    int oldIndex = currentIndex;
    switch (event->key()) {
        case Qt::Key_Up: currentIndex = wrapIndex(currentIndex + 1, nImgs); break;
        case Qt::Key_Down: currentIndex = wrapIndex(currentIndex - 1, nImgs); break;
        case Qt::Key_PageUp:
            currentIndex = wrapIndex(currentIndex + 5, nImgs);
            break;
        case Qt::Key_PageDown:
            currentIndex = wrapIndex(currentIndex - 5, nImgs);
            break;
        case Qt::Key_Home: currentIndex = 0; break;
        case Qt::Key_End: currentIndex = nImgs - 1; break;
        case Qt::Key_Z: zoomIn(); break;
        case Qt::Key_X: zoomOut(); break;
        case Qt::Key_R: fitInView(scene->sceneRect(), Qt::KeepAspectRatio); break;
        default: QGraphicsView::keyPressEvent(event); return;
    }

    if (currentIndex != oldIndex) { updateImage(); }
}
