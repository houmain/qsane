#pragma once

#include <QGraphicsItem>
#include <QImage>

class GraphicsImageItem : public QGraphicsItem
{
public:
    explicit GraphicsImageItem(QGraphicsItem *parent = nullptr);

    void setImage(const QImage &image);
    void clear();
    const QImage &image() const { return mImage; }
    QRectF boundingRect() const override;
    void setNextScanLine(const QByteArray &scanline);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
        QWidget *widget) override;

private:
    QImage mImage;
    int mNextScanLine{ };
};
