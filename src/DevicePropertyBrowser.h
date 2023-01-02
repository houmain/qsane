#pragma once

#include "qtsanescanner/src/qtsanescanner.h"
#include "qtpropertybrowser/src/qttreepropertybrowser.h"

class QtVariantPropertyManager;
class QtVariantProperty;
class QtProperty;

class DevicePropertyBrowser : public QtTreePropertyBrowser
{
    Q_OBJECT
public:
    explicit DevicePropertyBrowser(QWidget *parent = nullptr);

    void setScanner(QtSaneScanner *scanner);
    void setShowAdvanced(bool showAdvanced);

private Q_SLOTS:
    void handleValueChanged(QtProperty *property,
        const QVariant &value);
    void handleOptionsChanged();
    void handleOptionChanged(const QtSaneScanner::Option &option);

private:
    void createProperty(const QtSaneScanner::Option &option);
    void refreshProperties();
    void refreshProperty(QtProperty &property,
        const QtSaneScanner::Option &option);

    QtVariantPropertyManager* mPropertyManager;
    QMap<QString, QtProperty *> mProperties;
    QtSaneScanner *mScanner{ };
    bool mShowAdvanced{ };
};
