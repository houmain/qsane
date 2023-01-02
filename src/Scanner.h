#pragma once

#include "qtsanescanner/src/qtsanescanner.h"

class Scanner : public QtSaneScanner
{
    Q_OBJECT

public:
    explicit Scanner(const QString &deviceName);

    QPointF getResolution() const;
    void setResolution(const QPointF &resolution);
    void setResolution(double res) { setResolution(QPointF(res, res)); }
    QList<double> getUniformResolutions() const;
    QRectF getBounds() const;
    void setBounds(const QRectF &bounds);
    QRectF getMaximumBounds() const;
    QImage startScan(bool preview);
    void cancelScan();

Q_SIGNALS:
    void optionValuesChanged();

private:
    void setOptionValue(const QString &name, const QVariant &value) {
        if (auto option = findOption(name))
            return option->setValue(value);
    }
    QVariant getOptionValue(const QString &name) const {
        if (auto option = findOption(name))
            return option->value();
        return { };
    }
};
