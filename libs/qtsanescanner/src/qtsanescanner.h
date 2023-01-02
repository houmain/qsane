#pragma once

#include <QString>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QVariant>
#include <QImage>

class QtSaneScanner : public QObject
{
    Q_OBJECT
public:
    struct OptionDescriptor;

    struct DeviceInfo
    {
        QString name;
        QString vendor;
        QString model;
        QString type;
    };

    enum Flags : int {
        SoftSelect = (1 << 0),
        HardSelect = (1 << 1),
        SoftDetect = (1 << 2),
        Emulated = (1 << 3),
        Automatic = (1 << 4),
        Inactive = (1 << 5),
        Advanced = (1 << 6),
        HasUnappliedValue = (1 << 10),
    };

    enum class Type
    {
        Bool,
        Int,
        Value,
        String,
        Button,
        Group,
        BoolList,
        IntList,
        ValueList,
    };

    enum class Unit
    {
        None,
        Pixel,
        Bit,
        Millimeter,
        DPI,
        Percent,
        Microsecond,
    };

    struct Range
    {
        double min;
        double max;
        double quantization;
    };

    class Option
    {
    public:
        Option(QtSaneScanner *scanner, int optionIndex,
            const OptionDescriptor &descriptor);

        const QString &name() const { return mName; }
        const QString &title() const { return mTitle; }
        const QString &description() const { return mDescription; }
        bool isActive() const { return (mFlags & Inactive) == 0; }
        bool isSettable() const { return (mFlags & SoftSelect) != 0; }
        bool isAdvanced() const { return (mFlags & Advanced) != 0; }
        Unit unit() const { return mUnit; }
        Type type() const { return mType; }
        const QList<QVariant> &allowedValues() const { return mAllowedValues; }
        const Range &allowedRange() const { return mAllowedRange; }
        const QVariant &value() const { return mValue; }
        void setValue(const QVariant &value);

    private:
        friend class QtSaneScanner;
        void update(const OptionDescriptor &descriptor);
        void setHasUnappliedValue() { mFlags |= HasUnappliedValue; }
        void clearHasUnappliedValue() { mFlags &= ~HasUnappliedValue; }
        bool hasUnappliedValue() { return (mFlags & HasUnappliedValue) != 0; }

        QtSaneScanner *mScanner{ };
        int mIndex{ };
        QString mName;
        QString mTitle;
        QString mDescription;
        unsigned int mFlags;
        Type mType{ };
        Unit mUnit{ };
        QList<QVariant> mAllowedValues;
        Range mAllowedRange;
        QVariant mValue;
    };

    static QList<DeviceInfo> initialize();
    static void shutdown();

    explicit QtSaneScanner(const QString &deviceName);
    ~QtSaneScanner();
    bool isOpened() const { return (mDeviceHandle != nullptr); }
    const QList<Option> &options() const { return mOptions; }
    Option* findOption(const QString &name);
    const Option* findOption(const QString &name) const;
    QImage startScan();
    QByteArray readScanLine();
    void cancelScan();

Q_SIGNALS:
    void optionChanged(const QtSaneScanner::Option &option);
    void optionsChanged();

private:
    void handleOptionValueChanged(int index);
    bool applyUnappliedOptionValues();
    void updateAllOptions();
    void setOptionValue(int index, bool *reloadOptions);
    QVariant getOptionValue(int index) const;

    static int sSaneVersionCode;
    void* mDeviceHandle{ };
    QList<Option> mOptions;
    QList<const OptionDescriptor*> mOptionDescriptors;
    QMap<QString, Option*> mOptionMap;
    QMutex mMutex;
    bool mScanning{ };
    int mBytesPerLine{ };
};
