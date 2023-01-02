#include "Scanner.h"
#include <QSet>

namespace
{
    namespace WellKnownOption
    {
        const auto preview = QStringLiteral("preview");
        const auto resolution = QStringLiteral("resolution");
        const auto x_resolution = QStringLiteral("x-resolution");
        const auto y_resolution = QStringLiteral("y-resolution");
        const auto top_left_x = QStringLiteral("tl-x");
        const auto top_left_y = QStringLiteral("tl-y");
        const auto bottom_right_x = QStringLiteral("br-x");
        const auto bottom_right_y = QStringLiteral("br-y");
    }

    QPair<double, double> getMinMax(const QtSaneScanner::Option &option)
    {
        if (!option.allowedValues().isEmpty())
            return {
                option.allowedValues().first().toDouble(),
                option.allowedValues().last().toDouble()
            };

        return { option.allowedRange().min, option.allowedRange().max };
    }

    QSet<double> toValueSet(const QList<QVariant> &values)
    {
        auto set = QSet<double>();
        for (const auto &value : values)
            set << value.toDouble();
        return set;
    }

    QList<double> intersectLists(const QList<QVariant> &a, const QList<QVariant> &b)
    {
        const auto intersected = toValueSet(a).intersect(toValueSet(b));
        auto list = QList<double>(intersected.begin(), intersected.end());
        std::sort(list.begin(), list.end());
        return list;
    }
} // namespace

Scanner::Scanner(const QString &deviceName)
    : QtSaneScanner(deviceName)
{
    connect(this, &QtSaneScanner::optionsChanged,
        this, &Scanner::optionValuesChanged);
    connect(this, &QtSaneScanner::optionChanged,
        this, &Scanner::optionValuesChanged);
}

QImage Scanner::startScan(bool preview)
{
    disconnect(this, &QtSaneScanner::optionsChanged,
        this, &Scanner::optionValuesChanged);
    disconnect(this, &QtSaneScanner::optionChanged,
        this, &Scanner::optionValuesChanged);

    const auto savedResolution = getResolution();
    const auto savedBounds = getBounds();
    if (preview) {
        setOptionValue(WellKnownOption::preview, preview);
        const auto resolutions = getUniformResolutions();
        if (!resolutions.isEmpty())
            setResolution(resolutions.first());
        setBounds(getMaximumBounds());
    }

    const auto dpi = getResolution();
    const auto dpiToDpm = 39.37;
    auto image = QtSaneScanner::startScan();
    image.setDotsPerMeterX(static_cast<int>(dpi.x() * dpiToDpm));
    image.setDotsPerMeterY(static_cast<int>(dpi.y() * dpiToDpm));

    if (preview) {
        setOptionValue(WellKnownOption::preview, false);
        setResolution(savedResolution);
        setBounds(savedBounds);
    }
    return image;
}

void Scanner::cancelScan()
{
    connect(this, &QtSaneScanner::optionsChanged,
        this, &Scanner::optionValuesChanged);
    connect(this, &QtSaneScanner::optionChanged,
        this, &Scanner::optionValuesChanged);

    QtSaneScanner::cancelScan();
}

void Scanner::setResolution(const QPointF &res)
{
    setOptionValue(WellKnownOption::resolution, std::min(res.x(), res.y()));
    if (auto x_resolution = findOption(WellKnownOption::x_resolution))
        x_resolution->setValue(res.x());
    if (auto y_resolution = findOption(WellKnownOption::y_resolution))
        y_resolution->setValue(res.y());
}

QPointF Scanner::getResolution() const
{
    auto x_res = getOptionValue(WellKnownOption::resolution).toDouble();
    auto y_res = x_res;
    if (auto x_resolution = findOption(WellKnownOption::x_resolution))
        x_res = x_resolution->value().toDouble();
    if (auto y_resolution = findOption(WellKnownOption::y_resolution))
        y_res = y_resolution->value().toDouble();
    return { x_res, y_res };
}

QList<double> Scanner::getUniformResolutions() const
{
    auto list = QList<double>();
    const auto x_res = findOption(WellKnownOption::x_resolution);
    const auto y_res = findOption(WellKnownOption::y_resolution);
    if (x_res && y_res && !x_res->allowedValues().isEmpty()) {
        list = intersectLists(x_res->allowedValues(), y_res->allowedValues());
    }
    else if (auto res = findOption(WellKnownOption::resolution)) {
        if (!res->allowedValues().isEmpty()) {
            for (const auto &value : res->allowedValues())
                list << value.toDouble();
        }
        else {
            // TODO: improve list
            auto range = res->allowedRange();
            const auto step = (range.max - range.min) / 10;
            for (auto value = range.min; value < range.max; value += step)
                list << value;
            list << range.max;
        }
    }
    return list;
}

void Scanner::setBounds(const QRectF &bounds)
{
    setOptionValue(WellKnownOption::top_left_x, bounds.topLeft().x());
    setOptionValue(WellKnownOption::top_left_y, bounds.topLeft().y());
    setOptionValue(WellKnownOption::bottom_right_x, bounds.bottomRight().x());
    setOptionValue(WellKnownOption::bottom_right_y, bounds.bottomRight().y());
}

QRectF Scanner::getBounds() const
{
    const auto topLeft = QPointF(
        getOptionValue(WellKnownOption::top_left_x).toDouble(),
        getOptionValue(WellKnownOption::top_left_y).toDouble());
    const auto bottomRight = QPointF(
        getOptionValue(WellKnownOption::bottom_right_x).toDouble(),
        getOptionValue(WellKnownOption::bottom_right_y).toDouble());
    return QRectF(topLeft, bottomRight);
}

QRectF Scanner::getMaximumBounds() const
{
    const auto br_x = findOption(WellKnownOption::bottom_right_x);
    const auto br_y = findOption(WellKnownOption::bottom_right_y);
    if (!br_x || !br_y)
        return { };

    const auto minMaxX = getMinMax(*br_x);
    const auto minMaxY = getMinMax(*br_y);
    const auto rect = QRectF(minMaxX.first, minMaxY.first,
                             minMaxX.second, minMaxY.second);

    if (br_x->unit() == Unit::Pixel) {
        const auto dpi = getResolution();
        const auto pixelsToMM = QPointF(25.4 / dpi.x(), 25.4 / dpi.y());
        return QRectF{
            rect.left() * pixelsToMM.x(),
            rect.top() * pixelsToMM.y(),
            rect.right() * pixelsToMM.x(),
            rect.bottom() * pixelsToMM.y(),
        };
    }
    return rect;
}

