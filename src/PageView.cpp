#include "PageView.h"
#include <cmath>
#include <QWheelEvent>
#include <QScrollBar>
#include <QGraphicsPathItem>
#include <QImageReader>
#include <QBitmap>

PageView::PageView(QWidget *parent) : QGraphicsView(parent)
{
    setTransformationAnchor(AnchorUnderMouse);
    setRenderHints(QPainter::SmoothPixmapTransform);
    setScene(new QGraphicsScene());
    mOutside = new QGraphicsPathItem();
    scene()->addItem(mOutside);
    setBounds({ });
}

void PageView::setBounds(QRectF bounds)
{
    auto pen = QPen();
    pen.setWidth(1);
    pen.setCosmetic(true);
    pen.setColor(QColor::fromRgbF(0.5, 0.5, 0.5));

    mOutside->setPen(pen);
    mOutside->setBrush(QBrush(QColor::fromRgbF(0.6, 0.6, 0.6, 0.7)));

    const auto max = 65536;
    auto outside = QPainterPath();
    outside.addRect(-max, -max, 2 * max, 2 * max);
    auto inside = QPainterPath();
    inside.addRect(bounds);
    outside = outside.subtracted(inside);
    mOutside->setPath(outside);

    const auto margin = 5;
    bounds.adjust(-margin, -margin, margin, margin);
    setSceneRect(bounds);

    setZoom(mZoom);
}

void PageView::wheelEvent(QWheelEvent *event)
{
    if (!event->modifiers()) {
        auto delta = (event->angleDelta().y() > 0 ? 1 : -1);
        setZoom(std::max(min, std::min(mZoom + delta, max)));
        return;
    }
    QGraphicsView::wheelEvent(event);
}

void PageView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        mPan = true;
        mPanStartX = event->x();
        mPanStartY = event->y();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    QGraphicsView::mousePressEvent(event);

    if (event->button() == Qt::LeftButton)
        if (scene()->selectedItems().isEmpty())
            Q_EMIT mousePressed(mapToScene(event->pos()));
}

void PageView::mouseMoveEvent(QMouseEvent *event)
{
    if (mPan) {
        horizontalScrollBar()->setValue(
            horizontalScrollBar()->value() - (event->x() - mPanStartX));
        verticalScrollBar()->setValue(
            verticalScrollBar()->value() - (event->y() - mPanStartY));
        mPanStartX = event->x();
        mPanStartY = event->y();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void PageView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        mPan = false;
        setCursor(Qt::ArrowCursor);
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void PageView::setZoom(int zoom) {
    mZoom = zoom;
    auto scale = std::pow(1.25, zoom);
    updateTransform(scale);

    Q_EMIT zoomChanged(scale);
}

void PageView::updateTransform(double scale)
{
    auto transform = QTransform().scale(scale, scale);
    setTransform(transform);
}
