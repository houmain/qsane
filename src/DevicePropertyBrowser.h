#pragma once

#include <sane/sane.h>
#include "qtpropertybrowser/src/qttreepropertybrowser.h"

class QtVariantPropertyManager;
class QtVariantProperty;
class QtProperty;
class Scanner;

class DevicePropertyBrowser : public QtTreePropertyBrowser
{
    Q_OBJECT
public:
    explicit DevicePropertyBrowser(QWidget *parent = nullptr);

    void setScanner(Scanner *scanner);
    void setShowAdvanced(bool showAdvanced);
    void refreshProperties();

Q_SIGNALS:
    void valueChanged(QtProperty *property,
        const QVariant &value);

private Q_SLOTS:
    void handleValueChanged(QtProperty *property,
        const QVariant &value);

private:
    void addOption(const SANE_Option_Descriptor &option);
    void refreshProperty(QtVariantProperty &property,
        const SANE_Option_Descriptor &option);
    void updatePropertyVisibility();

    QtVariantPropertyManager* mPropertyManager;
    QList<QtProperty *> mProperties;
    Scanner *mScanner{ };
    bool mShowAdvanced{ };
};
