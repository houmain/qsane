#pragma once

#include <QGraphicsView>

class PageView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit PageView(QWidget *parent = 0);
    void setBounds(QRectF bounds);

Q_SIGNALS:
    void mousePressed(const QPointF &position);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void setZoom(int zoom);
    void updateTransform(double scale);

    const int min = 0;
    const int max = 20;

    QGraphicsPathItem *mOutside{ };
    bool mPan{ };
    int mZoom{ 2 };
    int mPanStartX{ };
    int mPanStartY{ };
};
