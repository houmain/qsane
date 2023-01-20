#include "CropRect.h"
#include <cmath>
#include <QPen>
#include <QBrush>
#include <QCursor>
#include <QPainter>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsScene>
#include <QStyleOptionGraphicsItem>

CropRect::CropRect(QGraphicsItem *parent)
    : QGraphicsObject(parent)
{
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);
    setFlag(QGraphicsItem::ItemIsFocusable);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setZValue(1);
}

void CropRect::setBounds(const QRectF &bounds)
{
    if (isTransforming())
        return;

    prepareGeometryChange();
    mBounds = bounds;
    update();
}

QRectF CropRect::boundingRect() const
{
    const auto h = handleSize();
    return mBounds.adjusted(-h, -h, h, h);
}

void CropRect::setMaximumBounds(const QRectF &bounds)
{
    mMaximumBounds = bounds;
}

void CropRect::startRect(const QPointF &position)
{
    prepareGeometryChange();
    grabMouse();
    mDirY = mDirX = 5; // start on mouseMoveEvent
    mMouseOffset = position;
}

void CropRect::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    QGraphicsObject::hoverMoveEvent(event);

    const auto x = event->pos().x();
    const auto y = event->pos().y();
    const auto rect = bounds();
    const auto h = handleSize();
    const auto handles = rect.adjusted(h, h, -h, -h);

    mDirX = 1;
    mDirY = 1;
    mMouseOffset = { rect.left() - x, rect.top() -y };
    if (x > handles.right()) {
        mDirX = 2;
        mMouseOffset.setX(rect.right() - x);
    }
    else if (x < handles.left()) {
        mDirX = 0;
        mMouseOffset.setX(rect.left() - x);
    }

    if (y > handles.bottom()) {
        mDirY = 2;
        mMouseOffset.setY(rect.bottom() - y);
    }
    else if (y < handles.top()) {
        mDirY = 0;
        mMouseOffset.setY(rect.top() - y);
    }

    switch (mDirY * 3 + mDirX) {
        case 0:
        case 8: setCursor(Qt::SizeFDiagCursor); break;
        case 2:
        case 6: setCursor(Qt::SizeBDiagCursor); break;
        case 1:
        case 7: setCursor(Qt::SizeVerCursor); break;
        case 3:
        case 5: setCursor(Qt::SizeHorCursor); break;
        default: setCursor(Qt::ArrowCursor); break;
    }
}

void CropRect::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    static auto s_max_z_value = zValue();
    setZValue(++s_max_z_value);

    QGraphicsObject::mousePressEvent(event);
    grabMouse();
}

void CropRect::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsObject::mouseMoveEvent(event);

    auto rect = bounds();

    if (mDirX == 5) {
        // startRect
        mDirY = mDirX = 2;
        rect = QRectF(mMouseOffset, QSizeF(0, 0));
        mMouseOffset = { };
    }

    const auto mousePos = event->pos() + mMouseOffset;
    switch (mDirY * 3 + mDirX) {
        case 0: rect.setTopLeft(mousePos); break;
        case 1: rect.setTop(mousePos.y()); break;
        case 2: rect.setTopRight(mousePos); break;
        case 3: rect.setLeft(mousePos.x()); break;
        case 4: rect.moveTopLeft(mousePos); break;
        case 5: rect.setRight(mousePos.x()); break;
        case 6: rect.setBottomLeft(mousePos); break;
        case 7: rect.setBottom(mousePos.y()); break;
        case 8: rect.setBottomRight(mousePos); break;
    }

    if (rect.left() > rect.right()) {
        const auto l = rect.left();
        const auto r = rect.right();
        rect.setRight(l);
        rect.setLeft(r);
        mDirX = 2 - mDirX;
        mMouseOffset.setX(0);
    }
    if (rect.top() > rect.bottom()) {
        const auto t = rect.top();
        const auto b = rect.bottom();
        rect.setBottom(t);
        rect.setTop(b);
        mDirY = 2 - mDirY;
        mMouseOffset.setY(0);
    }

    const auto max = mMaximumBounds;
    if (mDirX == 1) {
        if (rect.left() < max.left())
            rect.moveLeft(max.left());
        else if (rect.right() > max.right())
            rect.moveRight(max.right());
    }
    if (mDirY == 1) {
        if (rect.top() < max.top())
            rect.moveTop(max.top());
        else if (rect.bottom() > max.bottom())
            rect.moveBottom(max.bottom());
    }
    if (rect.left() < max.left())     rect.setLeft(max.left());
    if (rect.left() > max.right())    rect.setLeft(max.right());
    if (rect.right() < max.left())    rect.setRight(max.left());
    if (rect.right() > max.right())   rect.setRight(max.right());
    if (rect.top() < max.top())       rect.setTop(max.top());
    if (rect.top() > max.bottom())    rect.setTop(max.bottom());
    if (rect.bottom() < max.top())    rect.setBottom(max.top());
    if (rect.bottom() > max.bottom()) rect.setBottom(max.bottom());

    prepareGeometryChange();
    mBounds = rect;
    update();

    Q_EMIT transforming(bounds());
}

void CropRect::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    ungrabMouse();
    QGraphicsObject::mouseReleaseEvent(event);
    Q_EMIT transformed(bounds());
}

bool CropRect::isTransforming() const
{
    return (scene() && scene()->mouseGrabberItem() == this);
}

void CropRect::paint(QPainter *painter,
    const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    auto pen = QPen();
    pen.setWidth(1);
    pen.setCosmetic(true);

    pen.setColor(QColor(Qt::white));
    painter->setPen(pen);
    painter->drawRect(mBounds);

    auto dashes = QVector<qreal>();
    dashes << 4 << 4;
    pen.setDashPattern(dashes);
    pen.setColor(QColor(Qt::black));
    painter->setPen(pen);
    painter->drawRect(mBounds);
}
