#pragma once

#include <QMap>
#include <QImage>
#include <QVariant>
#include <stdexcept>
#include <sane/sane.h>
#include <vector>
#include <functional>

class SaneException : public std::runtime_error
{
public:
    SaneException(SANE_Status status, const char *message);

    SANE_Status status() const { return mStatus; }
    const char *status_msg() const;

private:
    SANE_Status mStatus;
};

struct DeviceInfo {
    QString name;
    QString vendor;
    QString model;
    QString type;
};

class Scanner {
public:
    Scanner() = default;
    Scanner(const Scanner&) = delete;
    Scanner &operator=(const Scanner&) = delete;
    ~Scanner();

    void open(const QString &deviceName);
    void close();
    bool hasOption(const QString &name) const;
    void forEachOption(std::function<void(const SANE_Option_Descriptor&)> callback) const;
    const SANE_Option_Descriptor *getOption(const QString &name) const;
    void setOptionValue(const QString &name, const QVariant &value, bool isIndex = false);
    QVariant getOptionValue(const QString &name, bool getIndex = false) const;
    QImage start(bool preview);
    QByteArray readScanLine();
    void cancel();

    QPointF getResolution() const;
    void setResolution(const QPointF &resolution);
    void setResolution(double res) { setResolution(QPointF(res, res)); }
    QList<double> getUniformResolutions() const;
    QRectF getBounds() const;
    void setBounds(const QRectF &bounds);
    QRectF getMaximumBounds() const;

private:
    void setPreviewMode(bool preview);
    void restoreScanMode();

    struct Option {
        SANE_Int index;
        const SANE_Option_Descriptor *desc;
    };

    void controlSet(const Option &option, const void *value, SANE_Int size);
    void controlGet(const Option &option, void *value, SANE_Int size) const;

    template<typename T>
    void controlSet(const Option &option, const T &value) {
        controlSet(option, &value, sizeof(value));
    }

    template<typename T>
    T controlGet(const Option &option) const {
        auto value = T{ };
        controlGet(option, &value, sizeof(value));
        return value;
    }

    DeviceInfo mDeviceInfo;
    SANE_Handle mDeviceHandle{ };
    QMap<QString, Option> mOptions;
    SANE_Parameters mParameters{ };

    QPointF mSavedResolution{ };
    QRectF mSavedBounds{ };
};

QList<DeviceInfo> enumerateDevices();
void shutdownSane();

template<typename F>
void forEachWordInList(const SANE_Option_Descriptor &option, F&& function)
{
    auto list = option.constraint.word_list;
    for (auto i = 1; i <= list[0]; ++i)
        function(list[i]);
}

template<typename F>
void forEachStringInList(const SANE_Option_Descriptor &option, F&& function)
{
    const auto list = option.constraint.string_list;
    for (auto it = list; *it; ++it)
        function(*it);
}
