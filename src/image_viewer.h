
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
    void setViewAxis(int axis);

  protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

  private:
    QGraphicsScene *scene;
    tomocam::Array<uint8_t> imageStack;
    int axis;       // 0: z (n0), 1: y (n1), 2: x (n2) is the scroll direction
    int indices[3]; // remembered position along each axis
    void zoomIn();
    void zoomOut();
    void resetIndices();

    int axisLength() const;
    QImage extractPlane(int axis, int idx) const;
};

#endif // IMG_VIEWER__H
