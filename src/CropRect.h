#pragma once

#include <QGraphicsObject>

class CropRect : public QGraphicsObject
{
    Q_OBJECT
public:
    explicit CropRect(QGraphicsItem *parent = 0);
    bool isTransforming() const;
    void startResize();
    void setBounds(const QRectF &bounds);
    QRectF bounds() const;
    void setMaximumBounds(const QRectF &bounds);
    QRectF boundingRect() const override;

Q_SIGNALS:
    void transforming(const QRectF &rect);
    void transformed(const QRectF &rect);

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
        QWidget *widget) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QRectF mMaximumBounds{ };
    QRectF mBoundingRect{ };
    int mDirX{ };
    int mDirY{ };
    QPointF mMouseOffset{ };
};
