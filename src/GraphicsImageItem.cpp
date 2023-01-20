#include "GraphicsImageItem.h"
#include <QPainter>
#include <cstring>

GraphicsImageItem::GraphicsImageItem(QGraphicsItem *parent)
    : QGraphicsItem(parent)
{
    setFlags(QGraphicsItem::ItemSendsGeometryChanges);
}

void GraphicsImageItem::setImage(const QImage &image)
{
    prepareGeometryChange();

    mImage = image;
    mNextScanLine = 0;

    const auto dpm = QPointF(mImage.dotsPerMeterX(), mImage.dotsPerMeterY());
    auto transform = QTransform().scale(1000 / dpm.x(), 1000 / dpm.y());
    setTransform(transform);
}

void GraphicsImageItem::clear()
{
    mImage = { };
    update();
}

QRectF GraphicsImageItem::boundingRect() const
{
    return QRect(QPoint(), mImage.size());
}

void GraphicsImageItem::setNextScanLine(const QByteArray &scanline)
{
    const auto y = mNextScanLine++;
    if (y < mImage.height()) {
        std::memcpy(mImage.scanLine(y), scanline.data(), scanline.size());
        update(0, y, mImage.width(), 1);
    }
}

void GraphicsImageItem::paint(QPainter *painter,
    const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    if (mImage.isNull())
        return;

    if (mNextScanLine)
        painter->drawImage(0, 0, mImage, 0, 0, mImage.width(), mNextScanLine);

    auto pen = QPen();
    pen.setWidth(1);
    pen.setCosmetic(true);
    pen.setColor(QColor::fromRgbF(0, 0, 0, 0.2));
    painter->setPen(pen);
    painter->drawRect(boundingRect());
}
