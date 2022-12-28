
#include "Scanner.h"
#include <limits>
#include <cstring>
#include <cmath>
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

    int g_sane_version_code = 0;

    int initializeSane()
    {
        if (!g_sane_version_code)
            sane_init(&g_sane_version_code, nullptr);
        return g_sane_version_code;
    }

    [[noreturn]] void error(SANE_Status status, const char *action)
    {
        throw SaneException(status, action);
    }

    void check(SANE_Status status, const char *action)
    {
        if (status != SANE_STATUS_GOOD)
            error(status, action);
    }

    QPair<double, double> getMinMax(const SANE_Option_Descriptor *option)
    {
        const auto toPair = [&](SANE_Word min, SANE_Word max) -> QPair<double, double> {
            if (option->type == SANE_TYPE_FIXED)
                return { SANE_UNFIX(min), SANE_UNFIX(max) };
            return { static_cast<double>(min), static_cast<double>(max) };
        };
        if (option && option->constraint_type == SANE_CONSTRAINT_RANGE) {
            return toPair(option->constraint.range->min, option->constraint.range->max);
        }
        else if (option && option->constraint_type == SANE_CONSTRAINT_WORD_LIST) {
            const auto wordList = option->constraint.word_list;
            auto min = std::numeric_limits<SANE_Word>::max();
            auto max = std::numeric_limits<SANE_Word>::min();
            for (auto i = 1; i <= wordList[0]; ++i) {
                if (wordList[i] < min) min = wordList[i];
                if (wordList[i] > max) max = wordList[i];
            }
            return toPair(min, max);
        }
        error(SANE_STATUS_INVAL, "reading area bounds");
    }

    template<typename F>
    void forEachValueInList(const SANE_Option_Descriptor &option, F&& function)
    {
        forEachWordInList(option, [&](auto word) {
            function(option.type == SANE_TYPE_FIXED ?
                SANE_UNFIX(word) : static_cast<double>(word));
        });
    }

    SANE_Word getNthWord(const SANE_Option_Descriptor &option, int index)
    {
        const auto list = option.constraint.word_list;
        const auto count = list[0];
        return list[std::clamp(index + 1, 1, count)];
    }

    QString getNthString(const SANE_Option_Descriptor &option, int index)
    {
        const auto list = option.constraint.string_list;
        for (auto it = list; *it; ++it, --index)
            if (index <= 0)
                return *it;
        return { };
    }

    int indexOfWord(const SANE_Option_Descriptor &option, SANE_Word word)
    {
        const auto list = option.constraint.word_list;
        const auto count = list[0];
        for (auto i = 1; i <= count; ++i)
            if (list[i] == word)
                return i - 1;
        return 0;
    }

    int indexOfString(const SANE_Option_Descriptor &option, const QString &string)
    {
        const auto list = option.constraint.string_list;
        auto i = 0;
        for (auto it = list; *it; ++it, ++i)
            if (*it == string)
                return i;
        return 0;
    }
} // namespace

SaneException::SaneException(SANE_Status status, const char *message)
    : std::runtime_error(message)
    , mStatus(status)
{
}

const char *SaneException::status_msg() const
{
    return sane_strstatus(mStatus);
}

void shutdownSane()
{
    if (g_sane_version_code) {
        sane_exit();
        g_sane_version_code = 0;
    }
}

QList<DeviceInfo> enumerateDevices()
{
    shutdownSane();

    if (!initializeSane())
        return { };

    auto device_list = std::add_pointer_t<std::add_pointer_t<SANE_Device>>{ };
    check(sane_get_devices(const_cast<const SANE_Device***>(&device_list), true),
          "enumerating devices");

    auto devices = QList<DeviceInfo>();
    for (auto i = 0; device_list[i]; ++i) {
        auto &device = *device_list[i];
        if (device.type == QStringLiteral("virtual device"))
            continue;
        devices += DeviceInfo{
            device.name,
            device.vendor,
            device.model,
            device.type,
        };
    }
    return devices;
}

Scanner::~Scanner()
{
    close();
}

void Scanner::open(const QString &deviceName)
{
    check(sane_open(qUtf8Printable(deviceName), &mDeviceHandle),
        "opening device");

    mOptions.clear();
    for (auto i = 1; auto option = sane_get_option_descriptor(mDeviceHandle, i); ++i)
        mOptions.insert(option->name, { i, option });

    if (hasOption(QStringLiteral("short-resolution")))
        setOptionValue("short-resolution", false);
}

void Scanner::close() {
    if (mDeviceHandle)
        sane_close(mDeviceHandle);
    mDeviceHandle = nullptr;
}

void Scanner::controlSet(const Option &option, const void *value, SANE_Int size)
{
    check(sane_control_option(mDeviceHandle, option.index,
        SANE_ACTION_SET_VALUE, const_cast<void*>(value), nullptr),
        "setting option");
}

void Scanner::controlGet(const Option &option, void *value, SANE_Int size) const
{
    Q_ASSERT(size == option.desc->size);
    if (size == option.desc->size)
        check(sane_control_option(mDeviceHandle, option.index,
            SANE_ACTION_GET_VALUE, value, nullptr),
            "getting option");
}

bool Scanner::hasOption(const QString &name) const
{
    return mOptions.contains(name);
}

void Scanner::forEachOption(
        std::function<void(const SANE_Option_Descriptor&)> callback) const
{
    for (const auto option : mOptions)
        callback(*option.desc);
}

const SANE_Option_Descriptor *Scanner::getOption(const QString &name) const
{
    return mOptions[name].desc;
}

void Scanner::setOptionValue(const QString &name, const QVariant &value, bool isIndex)
{
    const auto &option = mOptions[name];
    if (auto desc = option.desc) {
        switch (desc->type) {
            case SANE_TYPE_BOOL:
                controlSet(option, SANE_Bool{ value.toBool() });
                break;

            case SANE_TYPE_INT:
                if (isIndex && desc->constraint_type == SANE_CONSTRAINT_WORD_LIST)
                    controlSet(option, SANE_Int{ getNthWord(*desc, value.toInt()) });
                else
                    controlSet(option, SANE_Int{ value.toInt() });
                break;

            case SANE_TYPE_FIXED:
                if (isIndex && desc->constraint_type == SANE_CONSTRAINT_WORD_LIST)
                    controlSet(option, SANE_Fixed{ getNthWord(*desc, value.toInt()) });
                else
                    controlSet(option, SANE_Fixed{ SANE_FIX(value.toDouble()) });
                break;

            case SANE_TYPE_STRING: {
                auto buffer = QByteArray();
                if (isIndex && desc->constraint_type == SANE_CONSTRAINT_STRING_LIST)
                    buffer = getNthString(*desc, value.toInt()).toUtf8();
                else
                    buffer = value.toString().toUtf8();
                controlSet(option, buffer.data(), buffer.size());
                break;
            }
        }
    }
}

QVariant Scanner::getOptionValue(const QString &name, bool getIndex) const
{
    const auto &option = mOptions[name];
    if (auto desc = option.desc) {
        switch (desc->type) {
            case SANE_TYPE_BOOL:
                return static_cast<bool>(controlGet<SANE_Bool>(option));

            case SANE_TYPE_INT: {
                const auto value = controlGet<SANE_Int>(option);
                if (getIndex && desc->constraint_type == SANE_CONSTRAINT_WORD_LIST)
                    return indexOfWord(*desc, value);
                return value;
            }

            case SANE_TYPE_FIXED: {
                const auto value = controlGet<SANE_Fixed>(option);
                if (getIndex && desc->constraint_type == SANE_CONSTRAINT_WORD_LIST)
                    return indexOfWord(*desc, value);
                return SANE_UNFIX(value);
            }

            case SANE_TYPE_STRING: {
                auto buffer = QByteArray(desc->size, Qt::Uninitialized);
                controlGet(option, buffer.data(), desc->size);
                auto string = QString(buffer);
                if (getIndex && desc->constraint_type == SANE_CONSTRAINT_STRING_LIST)
                    return indexOfString(*desc, string);
                return string;
            }
        }
    }
    return { };
}

QImage Scanner::start(bool preview)
{
    setPreviewMode(preview);

    check(sane_start(mDeviceHandle), "starting scan");
    check(sane_get_parameters(mDeviceHandle, &mParameters), "getting scan parameters");

    auto format = QImage::Format{ };
    format = QImage::Format_RGB888;

    const auto size = QSize(mParameters.pixels_per_line, mParameters.lines);
    auto image = QImage(size, format);

    const auto dpi = getResolution();
    const auto dpiToDpm = 39.37;
    image.setDotsPerMeterX(static_cast<int>(dpi.x() * dpiToDpm));
    image.setDotsPerMeterY(static_cast<int>(dpi.y() * dpiToDpm));

    return image;
}

QByteArray Scanner::readScanLine()
{
    auto length = SANE_Int{ };
    auto rowData = QByteArray(mParameters.bytes_per_line, Qt::Uninitialized);
    const auto result = sane_read(mDeviceHandle,
        reinterpret_cast<SANE_Byte*>(rowData.data()), rowData.size(), &length);

    if (result == SANE_STATUS_EOF || result == SANE_STATUS_CANCELLED)
        return { };

    check(result, "reading line");
    return rowData;
}

void Scanner::cancel()
{
    sane_cancel(mDeviceHandle);
    restoreScanMode();
}

void Scanner::setPreviewMode(bool preview)
{
    setOptionValue(WellKnownOption::preview, preview);
    mSavedResolution = getResolution();
    mSavedBounds = getBounds();
    if (preview) {
        const auto resolutions = getUniformResolutions();
        setResolution(resolutions.first());
        setBounds(getMaximumBounds());
    }
}

void Scanner::restoreScanMode()
{
    setOptionValue(WellKnownOption::preview, false);
    setResolution(mSavedResolution);
    setBounds(mSavedBounds);
}

void Scanner::setResolution(const QPointF &resolution)
{
    setOptionValue(WellKnownOption::resolution,
        std::min(resolution.x(), resolution.y()));
    if (hasOption(WellKnownOption::x_resolution))
        setOptionValue(WellKnownOption::x_resolution, resolution.x());
    if (hasOption(WellKnownOption::y_resolution))
        setOptionValue(WellKnownOption::y_resolution, resolution.y());
}

QPointF Scanner::getResolution() const
{
    auto xResolution = getOptionValue(WellKnownOption::resolution).toDouble();
    auto yResolution = xResolution;
    if (hasOption(WellKnownOption::x_resolution))
        xResolution = getOptionValue(WellKnownOption::x_resolution).toDouble();
    if (hasOption(WellKnownOption::y_resolution))
        yResolution = getOptionValue(WellKnownOption::y_resolution).toDouble();
    return { xResolution, yResolution };
}

QList<double> Scanner::getUniformResolutions() const
{
    const auto getValues = [](const auto &desc) {
        auto values = QSet<double>();
        forEachValueInList(desc, [&](auto value) { values.insert(value); });
        return values;
    };

    auto resolutions = getValues(*getOption(WellKnownOption::resolution));
    auto xResolutions = resolutions;
    auto yResolutions = resolutions;
    if (hasOption(WellKnownOption::x_resolution))
        xResolutions = getValues(*getOption(WellKnownOption::x_resolution));
    if (hasOption(WellKnownOption::y_resolution))
        yResolutions = getValues(*getOption(WellKnownOption::y_resolution));
    resolutions = xResolutions.intersect(yResolutions);
    if (resolutions.empty())
        resolutions.insert(*xResolutions.begin());

    auto list = QList<double>(resolutions.begin(), resolutions.end());
    std::sort(list.begin(), list.end());
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
    const auto minMaxX = getMinMax(getOption(WellKnownOption::bottom_right_x));
    const auto minMaxY = getMinMax(getOption(WellKnownOption::bottom_right_y));
    const auto rect = QRectF(minMaxX.first, minMaxY.first,
                            minMaxX.second, minMaxY.second);

    if (getOption(WellKnownOption::bottom_right_x)->unit == SANE_UNIT_PIXEL) {
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

