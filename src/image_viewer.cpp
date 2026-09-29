#include <QGraphicsPixmapItem>
#include <QMouseEvent>
#include <QTimer>
#include <algorithm>
#include <cstdint>
#include <qevent.h>
#include <qgraphicsview.h>
#include <qnamespace.h>
#include <string>
#include <unistd.h>
#include <utility>

#include "colormap.h"
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

ImageViewer::ImageViewer(std::unique_ptr<SliceCache> cache, uint64_t fullVolumeBytes,
                         size_t cacheBudgetBytes, QWidget *parent)
    : QGraphicsView(parent), mode_(ViewerMode::LazyZOnly),
      sliceCache_(std::move(cache)), fullVolumeBytes_(fullVolumeBytes),
      cacheBudgetBytes_(cacheBudgetBytes), axis(0) {
    connect(sliceCache_.get(), &SliceCache::sliceReady, this,
            &ImageViewer::onSliceReady, Qt::QueuedConnection);
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
    if (mode_ == ViewerMode::LazyZOnly)
        return static_cast<int>(sliceCache_->pageCount());
    tomocam::dims_t d = imageStack.dims();
    return static_cast<int>(axis == 0 ? d.n0 : axis == 1 ? d.n1 : d.n2);
}

void ImageViewer::resetIndices() {
    if (mode_ == ViewerMode::LazyZOnly) {
        indices[0] = 0;
        indices[1] = 0;
        indices[2] = 0;
        return;
    }
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

    QString title = QString("%1 %2/%3")
                        .arg(axisNames[axis])
                        .arg(indices[axis] + 1)
                        .arg(axisLength());
    if (colormap_ != tomocam::colormap::Colormap::Grayscale)
        title += QString(" [%1]").arg(tomocam::colormap::name(colormap_));
    setWindowTitle(title);
}

void ImageViewer::setViewAxis(int newAxis) {
    if (newAxis < 0 || newAxis > 2 || newAxis == axis) return;

    if (mode_ == ViewerMode::LazyZOnly && newAxis != 0) {
        showTransientMessage(
            QString("Y/X views are disabled for this file: the normalized volume is "
                    "%1 MB, which exceeds the %2 MB cache budget needed for a full "
                    "load. Increase --cache-mb (or cache_mb in "
                    "~/.config/tiffview/config.toml) to enable Y/X viewing.")
                .arg(fullVolumeBytes_ / 1e6, 0, 'f', 0)
                .arg(cacheBudgetBytes_ / 1e6, 0, 'f', 0));
        return;
    }

    axis = newAxis;
    updateImage();
    resetTransform();
    fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
}

void ImageViewer::showTransientMessage(const QString &text) {
    if (!hintLabel_) {
        hintLabel_ = new QLabel(this);
        hintLabel_->setStyleSheet(
            "background-color: rgba(0, 0, 0, 180); color: white; "
            "padding: 6px; border-radius: 4px;");
        hintLabel_->setWordWrap(true);
        hintLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    hintLabel_->setFixedWidth(std::min(420, width() - 16));
    hintLabel_->setText(text);
    hintLabel_->adjustSize();
    hintLabel_->move(8, 8);
    hintLabel_->show();
    hintLabel_->raise();
    QTimer::singleShot(4000, hintLabel_, &QWidget::hide);
}

void ImageViewer::onSliceReady(uint32_t index) {
    if (mode_ == ViewerMode::LazyZOnly && index == static_cast<uint32_t>(indices[0]))
        updateImage();
}

QImage ImageViewer::makeIndexedImage(int width, int height) const {
    QImage img(width, height, QImage::Format_Indexed8);
    img.setColorTable(colorTableFor(colormap_));
    return img;
}

// The 2D plane at position idx along the given axis. Dimensions are
// (n0, n1, n2) = (slices, rows, cols); the plane keeps the remaining two
// axes in their original order.
QImage ImageViewer::extractPlane(int ax, int idx) const {
    if (mode_ == ViewerMode::LazyZOnly) {
        sliceCache_->setCurrentIndex(static_cast<uint32_t>(idx));
        auto bytes = sliceCache_->getSlice(static_cast<uint32_t>(idx));
        QImage img = makeIndexedImage(sliceCache_->width(), sliceCache_->height());
        if (!bytes) {
            img.fill(0);
            return img;
        }
        const uint8_t *src = bytes->data();
        for (uint32_t r = 0; r < sliceCache_->height(); ++r)
            std::copy_n(src + size_t(r) * sliceCache_->width(), sliceCache_->width(),
                        img.scanLine(r));
        return img;
    }

    tomocam::dims_t d = imageStack.dims();
    const uint8_t *data = imageStack.begin();
    size_t i = static_cast<size_t>(idx);

    if (ax == 0) { // rows: n1, cols: n2, contiguous
        QImage img = makeIndexedImage(d.n2, d.n1);
        const uint8_t *src = data + i * d.n1 * d.n2;
        for (uint32_t r = 0; r < d.n1; ++r)
            std::copy_n(src + size_t(r) * d.n2, d.n2, img.scanLine(r));
        return img;
    }
    if (ax == 1) { // rows: n0, cols: n2
        QImage img = makeIndexedImage(d.n2, d.n0);
        for (uint32_t r = 0; r < d.n0; ++r) {
            const uint8_t *src = data + imageStack.flatIdx(r, i, 0);
            std::copy_n(src, d.n2, img.scanLine(r));
        }
        return img;
    }
    // ax == 2, rows: n0, cols: n1
    QImage img = makeIndexedImage(d.n1, d.n0);
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

    if (axisLength() <= 0) {
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
        case Qt::Key_C:
            colormap_ = tomocam::colormap::next(colormap_);
            updateImage();
            return;
        case Qt::Key_Q: close(); return;
        default: QGraphicsView::keyPressEvent(event); return;
    }

    if (idx != oldIndex) { updateImage(); }
}
