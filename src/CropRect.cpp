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
    setPos(bounds.topLeft());
    mBoundingRect.setSize(bounds.size());
    update();
}

QRectF CropRect::bounds() const
{
    return { pos() + mBoundingRect.topLeft(), mBoundingRect.size() };
}

void CropRect::setMaximumBounds(const QRectF &bounds)
{
    mMaximumBounds = bounds;
}

void CropRect::startResize()
{
    prepareGeometryChange();
    grabMouse();
    mDirY = mDirX = 2;
    mMouseOffset = { };
    mBoundingRect.setSize({ 0.001, 0.001 });
    update();
}

void CropRect::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    QGraphicsObject::hoverMoveEvent(event);

    const auto x = event->pos().x();
    const auto y = event->pos().y();
    const auto rect = boundingRect();
    const auto handleSizeX = 0.1 * rect.width();
    const auto handleSizeY = 0.1 * rect.height();
    auto handles = rect;
    handles.adjust(handleSizeX, handleSizeY, -handleSizeX, -handleSizeY);

    mDirX = 1;
    mDirY = 1;
    mMouseOffset = { -x, -y };
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

    const auto mousePos = event->pos() + mMouseOffset;
    auto rect = mBoundingRect;
    switch (mDirY * 3 + mDirX) {
        case 0: rect.setTopLeft(mousePos); break;
        case 1: rect.setTop(mousePos.y()); break;
        case 2: rect.setTopRight(mousePos); break;
        case 3: rect.setLeft(mousePos.x()); break;
        case 4: rect.translate(mousePos - rect.topLeft()); break;
        case 5: rect.setRight(mousePos.x()); break;
        case 6: rect.setBottomLeft(mousePos); break;
        case 7: rect.setBottom(mousePos.y()); break;
        case 8: rect.setBottomRight(mousePos); break;
    }
    if (!rect.width() || !rect.height())
        return;

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

    auto maximumBounds = mMaximumBounds;
    maximumBounds.moveTopLeft(-pos());

    if (mDirX == 1) {
        if (rect.left() < maximumBounds.left())
            rect.moveLeft(maximumBounds.left());
        else if (rect.right() > maximumBounds.right())
            rect.moveRight(maximumBounds.right());
    }
    if (mDirY == 1) {
        if (rect.top() < maximumBounds.top())
            rect.moveTop(maximumBounds.top());
        else if (rect.bottom() > maximumBounds.bottom())
            rect.moveBottom(maximumBounds.bottom());
    }

    rect = rect.intersected(maximumBounds);

    prepareGeometryChange();
    mBoundingRect = rect;
    update();

    Q_EMIT transforming(bounds());
}

void CropRect::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    ungrabMouse();
    QGraphicsObject::mouseReleaseEvent(event);

    moveBy(mBoundingRect.left(), mBoundingRect.top());
    mBoundingRect.moveTo(0, 0);

    Q_EMIT transformed(bounds());
}

bool CropRect::isTransforming() const
{
    return (scene() && scene()->mouseGrabberItem() == this);
}

QRectF CropRect::boundingRect() const
{
    return mBoundingRect;
}

void CropRect::paint(QPainter *painter,
    const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    auto pen = QPen();
    pen.setWidth(1);
    pen.setCosmetic(true);

    pen.setColor(QColor(Qt::white));
    painter->setPen(pen);
    painter->drawRect(boundingRect());

    auto dashes = QVector<qreal>();
    dashes << 4 << 4;
    pen.setDashPattern(dashes);
    pen.setColor(QColor(Qt::black));
    painter->setPen(pen);
    painter->drawRect(boundingRect());
}
