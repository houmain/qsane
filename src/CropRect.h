#pragma once

#include <QGraphicsObject>

class CropRect : public QGraphicsObject
{
    Q_OBJECT
public:
    explicit CropRect(QGraphicsItem *parent = 0);
    bool isTransforming() const;
    void startRect(const QPointF &position);
    void setBounds(const QRectF &bounds);
    const QRectF &bounds() const { return mBounds; }
    void setMaximumBounds(const QRectF &bounds);
    QRectF boundingRect() const override;
    void setHandleSize(qreal handleSize) { mHandleSize = handleSize; }
    qreal handleSize() const { return mHandleSize; }

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
    QRectF mBounds{ };
    int mDirX{ };
    int mDirY{ };
    QPointF mMouseOffset{ };
    qreal mHandleSize{ 1 };
};
