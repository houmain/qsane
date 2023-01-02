#include "qtsanescanner.h"
#include <QDebug>
#include <QMutexLocker>
#include <sane/sane.h>

int QtSaneScanner::sSaneVersionCode;

struct QtSaneScanner::OptionDescriptor : SANE_Option_Descriptor { };

namespace
{
    template<typename... T>
    void error(SANE_Status status, T&&... action)
    {
        auto stream = qWarning();
        (stream << ... << std::forward<T>(action));
        stream << " failed: " << sane_strstatus(status) << '\n';
    }

    template<typename... T>
    void check(SANE_Status status, T&&... action)
    {
        if (status != SANE_STATUS_GOOD)
            error(status, std::forward<T>(action)...);
    }

    template<typename F>
    void forEachWordInList(const SANE_Option_Descriptor &desc, F &&function)
    {
        const auto list = desc.constraint.word_list;
        for (auto i = 1; i <= list[0]; ++i)
            function(list[i]);
    }

    template<typename F>
    void forEachStringInList(const SANE_Option_Descriptor &desc, F &&function)
    {
        const auto list = desc.constraint.string_list;
        for (auto it = list; *it; ++it)
            function(*it);
    }

    template<typename F>
    void forEachValueInList(const SANE_Option_Descriptor &desc, F &&function)
    {
        forEachWordInList(desc, [&](auto word) {
            function(desc.type == SANE_TYPE_FIXED ?
                QVariant(SANE_UNFIX(word)) : QVariant(word));
        });
    }

    double toValue(SANE_Value_Type type, const SANE_Word &word)
    {
        return (type == SANE_TYPE_FIXED ?
            SANE_UNFIX(word) : static_cast<double>(word));
    }
} // namespace

auto QtSaneScanner::initialize() -> QList<DeviceInfo>
{
    shutdown();

    check(sane_init(&sSaneVersionCode, nullptr), "initializing SANE");
    if (!sSaneVersionCode)
        return { };

    auto deviceList = std::add_pointer_t<std::add_pointer_t<SANE_Device>>{ };
    check(sane_get_devices(const_cast<const SANE_Device***>(&deviceList), true),
          "enumerating devices");

    auto devices = QList<DeviceInfo>();
    for (auto i = 0; deviceList[i]; ++i) {
        auto &device = *deviceList[i];

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

void QtSaneScanner::shutdown()
{
    if (sSaneVersionCode) {
        sane_exit();
        sSaneVersionCode = 0;
    }
}

QtSaneScanner::Option::Option(QtSaneScanner *scanner,
        int optionIndex, const OptionDescriptor &descriptor)
    : mScanner(scanner)
    , mIndex(optionIndex)
    , mName(QString::fromUtf8(descriptor.name))
    , mTitle(QString::fromUtf8(descriptor.title))
    , mDescription(QString::fromUtf8(descriptor.desc))
{
}

void QtSaneScanner::Option::update(const OptionDescriptor &desc)
{
    mFlags = desc.cap;
    mUnit = static_cast<Unit>(desc.unit);
    mAllowedValues.clear();

    switch (desc.type) {
        case SANE_TYPE_BOOL:
            mType = (desc.size == sizeof(SANE_Bool) ?
                Type::Bool : Type::BoolList);
            break;

        case SANE_TYPE_INT:
            mType = (desc.size == sizeof(SANE_Int) ?
                Type::Int : Type::IntList);
            break;

        case SANE_TYPE_FIXED:
            mType = (desc.size == sizeof(SANE_Fixed) ?
                Type::Value : Type::ValueList);
            break;

        case SANE_TYPE_STRING:
            mType = Type::String;
            break;

        case SANE_TYPE_BUTTON:
            mType = Type::Button;
            break;

        case SANE_TYPE_GROUP:
            mType = Type::Group;
            break;
    }

    switch (desc.constraint_type) {
        case SANE_CONSTRAINT_RANGE:
            mAllowedRange = {
                toValue(desc.type, desc.constraint.range->min),
                toValue(desc.type, desc.constraint.range->max),
                toValue(desc.type, desc.constraint.range->quant)
            };
            break;

        case SANE_CONSTRAINT_WORD_LIST:
            forEachValueInList(desc,
                [&](auto&& value) { mAllowedValues << value; });
            break;

        case SANE_CONSTRAINT_STRING_LIST:
            forEachStringInList(desc,
                [&](auto&& value) { mAllowedValues << value; });
            break;
    }

    if (isActive())
        mValue = mScanner->getOptionValue(mIndex);
}

void QtSaneScanner::Option::setValue(const QVariant &value)
{
    if (mValue != value) {
        mValue = value;
        mScanner->handleOptionValueChanged(mIndex);
    }
}

QtSaneScanner::QtSaneScanner(const QString &deviceName)
{
    check(sane_open(qUtf8Printable(deviceName), &mDeviceHandle),
        "opening device ", deviceName);
    if (!mDeviceHandle)
        return;

    for (auto i = 1; auto desc = static_cast<const OptionDescriptor*>(
            sane_get_option_descriptor(mDeviceHandle, i)); ++i) {
        mOptionDescriptors.append(desc);
        mOptions.append(Option(this, i - 1, *desc));
        mOptions.back().update(*desc);
    }
    for (auto &option : mOptions)
        mOptionMap[option.name()] = &option;
}

QtSaneScanner::~QtSaneScanner()
{
    if (mDeviceHandle)
        sane_close(mDeviceHandle);
}

QImage QtSaneScanner::startScan()
{
    auto lock = QMutexLocker(&mMutex);
    if (!mDeviceHandle || mScanning)
        return { };

    auto result = sane_start(mDeviceHandle);
    if (result != SANE_STATUS_GOOD) {
        error(result, "starting scan");
        return { };
    }
    mScanning = true;

    auto parameters = SANE_Parameters{ };
    result = sane_get_parameters(mDeviceHandle, &parameters);
    if (result != SANE_STATUS_GOOD) {
        error(result, "getting scan parameters");
        return { };
    }

    mBytesPerLine = parameters.bytes_per_line;
    auto format = QImage::Format{ };
    format = QImage::Format_RGB888;
    const auto size = QSize(parameters.pixels_per_line, parameters.lines);
    return QImage(size, format);
}

QByteArray QtSaneScanner::readScanLine()
{
    auto lock = QMutexLocker(&mMutex);
    if (!mDeviceHandle || !mScanning)
        return { };

    auto length = SANE_Int{ };
    auto rowData = QByteArray(mBytesPerLine, Qt::Uninitialized);
    const auto result = sane_read(mDeviceHandle,
        reinterpret_cast<SANE_Byte*>(rowData.data()), rowData.size(), &length);
    if (result == SANE_STATUS_EOF || result == SANE_STATUS_CANCELLED)
        return { };

    check(result, "reading line");
    return rowData;
}

void QtSaneScanner::cancelScan()
{
    auto lock = QMutexLocker(&mMutex);
    if (mDeviceHandle && mScanning) {
        mScanning = false;
        sane_cancel(mDeviceHandle);
        if (applyUnappliedOptionValues()) {
            lock.unlock();
            Q_EMIT optionsChanged();
        }
    }
}

auto QtSaneScanner::findOption(const QString &name) const -> const Option*
{
    auto it = mOptionMap.find(name);
    if (it != mOptionMap.end())
        return it.value();
    return nullptr;
}

auto QtSaneScanner::findOption(const QString &name) -> Option*
{
    return const_cast<Option*>(
        static_cast<const QtSaneScanner*>(this)->findOption(name));
}

void QtSaneScanner::handleOptionValueChanged(int index)
{
    auto lock = QMutexLocker(&mMutex);
    auto &option = mOptions[index];
    if (!mDeviceHandle || mScanning) {
        option.setHasUnappliedValue();
        return;
    }

    auto reloadOptions = false;
    setOptionValue(index, &reloadOptions);
    if (reloadOptions) {
        updateAllOptions();
        lock.unlock();
        Q_EMIT optionsChanged();
    }
    else {
        lock.unlock();
        Q_EMIT optionChanged(option);
    }
}

bool QtSaneScanner::applyUnappliedOptionValues()
{
    auto valueApplied = false;
    auto reloadOptions = false;
    for (auto i = 0; i < mOptions.size(); ++i)
        if (mOptions[i].hasUnappliedValue()) {
            setOptionValue(i, &reloadOptions);
            mOptions[i].clearHasUnappliedValue();
            valueApplied = true;
        }

    if (reloadOptions)
        updateAllOptions();

    return valueApplied;
}

void QtSaneScanner::updateAllOptions()
{
    for (auto i = 0; i < mOptions.size(); ++i)
        mOptions[i].update(*mOptionDescriptors[i]);
}

void QtSaneScanner::setOptionValue(int index, bool *reloadOptions)
{
    const auto &desc = *mOptionDescriptors[index];
    if (!SANE_OPTION_IS_ACTIVE(desc.cap) ||
        !SANE_OPTION_IS_SETTABLE(desc.cap))
        return;

    auto info = SANE_Int{ };
    const auto setData = [&](const void *value) {
        check(sane_control_option(mDeviceHandle, index + 1,
            SANE_ACTION_SET_VALUE,
            const_cast<void*>(value), &info),
            "setting option", desc.name);
    };
    const auto setValue = [&](const auto &value) {
        setData(&value);
    };

    auto &option = mOptions[index];                
    const auto &value = option.value();
    switch (desc.type) {
        case SANE_TYPE_BOOL:
            if (desc.size == sizeof(SANE_Bool))
                setValue(SANE_Bool{ value.toBool() });
            break;

        case SANE_TYPE_INT:
            if (desc.size == sizeof(SANE_Int))
                setValue(SANE_Int{ value.toInt() });
            break;

        case SANE_TYPE_FIXED:
            if (desc.size == sizeof(SANE_Fixed))
                setValue(SANE_Fixed{ SANE_FIX(value.toDouble()) });
            break;

        case SANE_TYPE_STRING: {
            auto buffer = QByteArray();
            buffer = value.toString().toUtf8();
            setData(buffer.data());
            break;
        }
    }

    if (info & SANE_INFO_RELOAD_OPTIONS) {
        // reload all options when they are affected
        *reloadOptions = true;
    }
    else if (info & SANE_INFO_INEXACT) {
        // reload only this option when value could not be applied exactly
        option.update(desc);
    }
}

QVariant QtSaneScanner::getOptionValue(int index) const
{
    const auto &desc = *mOptionDescriptors[index];

    const auto getData = [&](void *value) {
        check(sane_control_option(mDeviceHandle, index + 1,
            SANE_ACTION_GET_VALUE, value, nullptr),
            "getting option", desc.name);
    };
    const auto getBuffer = [&]() {
        auto buffer = QByteArray(desc.size, Qt::Uninitialized);
        getData(buffer.data());
        return buffer;
    };

    switch (desc.type) {
        case SANE_TYPE_BOOL: {
            if (desc.size == sizeof(SANE_Bool)) {
                auto value = SANE_Bool{ };
                getData(&value);
                return static_cast<bool>(value);
            }
            return { };
        }

        case SANE_TYPE_INT: {
            if (desc.size == sizeof(SANE_Int)) {
                auto value = SANE_Int{ };
                getData(&value);
                return value;
            }
            else {
                const auto buffer = getBuffer();
                auto list = QVariantList();
                for (auto i = 0; i < desc.size; i += sizeof(SANE_Int))
                    list << *reinterpret_cast<const SANE_Int*>(buffer.data() + i);
                return list;
            }
        }

        case SANE_TYPE_FIXED: {
            if (desc.size == sizeof(SANE_Fixed)) {
                auto value = SANE_Fixed{ };
                getData(&value);
                return SANE_UNFIX(value);
            }
            else {
                const auto buffer = getBuffer();
                auto list = QVariantList();
                for (auto i = 0; i < desc.size; i += sizeof(SANE_Fixed))
                    list << SANE_UNFIX(*reinterpret_cast<const SANE_Fixed*>(buffer.data() + i));
                return list;
            }
        }

        case SANE_TYPE_STRING:
            return QString::fromUtf8(getBuffer());

        default:
            return { };
    }
}
