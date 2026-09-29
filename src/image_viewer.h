
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImage>
#include <QLabel>
#include <QWheelEvent>
#include <qevent.h>

#include <cstdint>
#include <memory>

#include "colormap_lut.h"
#include "io/array.h"
#include "slice_cache.h"

#ifndef IMG_VIEWER__H
#define IMG_VIEWER__H

class ImageViewer : public QGraphicsView {
    Q_OBJECT

  public:
    enum class ViewerMode { FullyLoaded, LazyZOnly };

    ImageViewer(const tomocam::Array<uint8_t> &, QWidget *parent = nullptr);
    ImageViewer(std::unique_ptr<SliceCache> cache, uint64_t fullVolumeBytes,
                size_t cacheBudgetBytes, QWidget *parent = nullptr);
    void updateImage();
    void updateImageStack(const tomocam::Array<uint8_t> &);
    void setViewAxis(int axis);

  protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

  private slots:
    void onSliceReady(uint32_t index);

  private:
    ViewerMode mode_ = ViewerMode::FullyLoaded;
    QGraphicsScene *scene;
    tomocam::Array<uint8_t> imageStack;      // FullyLoaded only
    std::unique_ptr<SliceCache> sliceCache_; // LazyZOnly only
    uint64_t fullVolumeBytes_ = 0;           // LazyZOnly only
    size_t cacheBudgetBytes_ = 0;            // LazyZOnly only
    QLabel *hintLabel_ = nullptr;            // transient "Y/X disabled" overlay
    int axis;       // 0: z (n0), 1: y (n1), 2: x (n2) is the scroll direction
    int indices[3]; // remembered position along each axis
    tomocam::colormap::Colormap colormap_ = tomocam::colormap::Colormap::Grayscale;
    void zoomIn();
    void zoomOut();
    void resetIndices();
    void showTransientMessage(const QString &text);

    int axisLength() const;
    QImage extractPlane(int axis, int idx) const;
    QImage makeIndexedImage(int width, int height) const;
};

#endif // IMG_VIEWER__H
