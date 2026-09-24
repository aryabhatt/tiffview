
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImage>
#include <QWheelEvent>
#include <qevent.h>

#include "io/array.h"

#ifndef IMG_VIEWER__H
#define IMG_VIEWER__H

class ImageViewer : public QGraphicsView {
    Q_OBJECT

  public:
    ImageViewer(const tomocam::Array<uint8_t> &, QWidget *parent = nullptr);
    void updateImage();
    void updateImageStack(const tomocam::Array<uint8_t> &);

  protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

  private:
    QGraphicsScene *scene;
    tomocam::Array<uint8_t> imageStack;
    int currentIndex;
    void zoomIn();
    void zoomOut();

    QImage ArrayToQImage(const tomocam::Slice<uint8_t> &);
};

#endif // IMG_VIEWER__H
