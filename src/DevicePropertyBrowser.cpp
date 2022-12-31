#include "DevicePropertyBrowser.h"
#include "qtpropertybrowser/src/qtvariantproperty.h"
#include "Scanner.h"

namespace
{
    int getVariantType(const SANE_Option_Descriptor &option)
    {
        switch (option.type) {
            case SANE_TYPE_BOOL:
                return QVariant::Bool;

            case SANE_TYPE_INT:
                if (option.constraint_type == SANE_CONSTRAINT_WORD_LIST)
                    return QtVariantPropertyManager::enumTypeId();
                if (option.size != sizeof(SANE_Int))
                    return 0;
                return QVariant::Int;

            case SANE_TYPE_FIXED:
                if (option.constraint_type == SANE_CONSTRAINT_WORD_LIST)
                    return QtVariantPropertyManager::enumTypeId();
                if (option.size != sizeof(SANE_Int))
                    return 0;
                return QVariant::Double;

            case SANE_TYPE_STRING:
                if (option.constraint_type == SANE_CONSTRAINT_STRING_LIST)
                    return QtVariantPropertyManager::enumTypeId();
                return QVariant::String;

            default:
                return 0;
        }
    }

    QString getUnitString(SANE_Unit unit)
    {
        switch (unit) {
            case SANE_UNIT_PIXEL: return QStringLiteral("px");
            case SANE_UNIT_BIT: return QStringLiteral("bits");
            case SANE_UNIT_MM: return QStringLiteral("mm");
            case SANE_UNIT_DPI: return QStringLiteral("dpi");
            case SANE_UNIT_PERCENT: return QStringLiteral("%");
            case SANE_UNIT_MICROSECOND: return QStringLiteral("μs");
            default: return { };
        }
    }

    bool filterDeviceOption(const QString &name, const QString &desc)
    {
        if (desc.contains(QStringLiteral("DEPRECATED"), Qt::CaseInsensitive))
            return false;

        if (name == "preview" ||
            name == "preview-speed" ||
            name == "speed" ||
            name == "short-resolution")
            return false;

        return true;
    }
} // namespace

DevicePropertyBrowser::DevicePropertyBrowser(QWidget *parent)
    : QtTreePropertyBrowser(parent)
    , mPropertyManager(new QtVariantPropertyManager(this))
{
    auto propertyFactory = new QtVariantEditorFactory(this);
    setFactoryForManager(mPropertyManager, propertyFactory);
    setIndentation(0);
}

void DevicePropertyBrowser::setScanner(Scanner *scanner)
{
    mScanner = scanner;

    clear();
    mProperties.clear();

    if (mScanner)
        mScanner->forEachOption([&](const SANE_Option_Descriptor &option) {
            addOption(option);
        });

    refreshProperties();
}

void DevicePropertyBrowser::setShowAdvanced(bool showAdvanced)
{
    if (mShowAdvanced != showAdvanced) {
        mShowAdvanced = showAdvanced;
        refreshProperties();
    }
}

void DevicePropertyBrowser::addOption(const SANE_Option_Descriptor &option)
{
    const auto variantType = getVariantType(option);
    if (!variantType)
        return;

    const auto name = QString(option.name);
    auto desc = QString(option.desc);
    if (!filterDeviceOption(name, desc))
        return;

    auto title = QString(option.title);
    if (option.unit)
        title += " [" + getUnitString(option.unit) + "]";
#if !defined(NDEBUG)
    desc += " [" + name + "]";
#endif

    auto property = mPropertyManager->addProperty(variantType, title);
    property->setWhatsThis(name);
    property->setToolTip(desc);
    mProperties.append(property);
}

void DevicePropertyBrowser::refreshProperty(QtVariantProperty &property,
    const SANE_Option_Descriptor &option)
{
    if (option.constraint_type == SANE_CONSTRAINT_STRING_LIST) {
        auto enumNames = QStringList();
        forEachStringInList(option, [&](auto string) { enumNames << string; });
        property.setAttribute(QLatin1String("enumNames"), enumNames);
    }
    else if (option.constraint_type == SANE_CONSTRAINT_WORD_LIST) {
        auto enumNames = QStringList();
        forEachWordInList(option, [&](auto word) { enumNames << QString::number(word); });
        property.setAttribute(QLatin1String("enumNames"), enumNames);
    }
    else if (option.constraint_type == SANE_CONSTRAINT_RANGE) {
        property.setAttribute(QLatin1String("minimum"), option.constraint.range->min);
        property.setAttribute(QLatin1String("maximum"), option.constraint.range->max);
        if (option.constraint.range->quant)
            property.setAttribute(QLatin1String("singleStep"), option.constraint.range->quant);
    }

    if (option.type == SANE_TYPE_FIXED) {
        property.setAttribute(QLatin1String("singleStep"), 0.001);
        property.setAttribute(QLatin1String("decimals"), 4);
    }
    property.setEnabled(option.cap & SANE_CAP_SOFT_SELECT);
    property.setValue(mScanner->getOptionValue(option.name, true));
}

void DevicePropertyBrowser::refreshProperties()
{
    disconnect(mPropertyManager, &QtVariantPropertyManager::valueChanged,
        this, &DevicePropertyBrowser::handleValueChanged);

    auto activeProperties = QList<QtProperty *>();
    for (auto property : qAsConst(mProperties))
        if (auto option = mScanner->getOption(property->whatsThis())) {
            const auto advanced = (option->cap & SANE_CAP_ADVANCED) != 0;
            const auto inactive = (option->cap & SANE_CAP_INACTIVE) != 0;
            if (!inactive && (!advanced || mShowAdvanced)) {
                activeProperties.append(property);
                refreshProperty(static_cast<QtVariantProperty&>(*property), *option);
            }
        }

    if (activeProperties != properties()) {
        clear();
        for (auto property : qAsConst(activeProperties))
            addProperty(property);
    }

    connect(mPropertyManager, &QtVariantPropertyManager::valueChanged,
        this, &DevicePropertyBrowser::handleValueChanged);
}

void DevicePropertyBrowser::handleValueChanged(QtProperty *property, const QVariant &value)
{
    mScanner->setOptionValue(property->whatsThis(), value, true);
    refreshProperties();

    Q_EMIT valueChanged(property, value);
}
