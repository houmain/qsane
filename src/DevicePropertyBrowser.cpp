#include "DevicePropertyBrowser.h"
#include "qtpropertybrowser/src/qtvariantproperty.h"

namespace
{
    int getVariantType(const QtSaneScanner::Option &option)
    {
        if (!option.allowedValues().isEmpty())
            return QtVariantPropertyManager::enumTypeId();

        using Type = QtSaneScanner::Type;
        switch (option.type()) {
            case Type::Bool:
                return QVariant::Bool;

            case Type::Int:
                return QVariant::Int;

            case Type::Value:
                return QVariant::Double;

            case Type::String:
                return QVariant::String;

            default:
                return 0;
        }
    }

    QString getUnitString(QtSaneScanner::Unit unit)
    {
        using Unit = QtSaneScanner::Unit;
        switch (unit) {
            case Unit::Pixel: return QStringLiteral("px");
            case Unit::Bit: return QStringLiteral("bits");
            case Unit::Millimeter: return QStringLiteral("mm");
            case Unit::DPI: return QStringLiteral("dpi");
            case Unit::Percent: return QStringLiteral("%");
            case Unit::Microsecond: return QStringLiteral("μs");
            default: return { };
        }
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

void DevicePropertyBrowser::setScanner(QtSaneScanner *scanner)
{
    if (mScanner) {
        disconnect(mScanner, &QtSaneScanner::optionsChanged, this,
            &DevicePropertyBrowser::handleOptionsChanged);
        disconnect(mScanner, &QtSaneScanner::optionChanged, this,
            &DevicePropertyBrowser::handleOptionChanged);

        clear();
        mProperties.clear();
    }

    mScanner = scanner;

    if (mScanner) {
        for (const auto &option : mScanner->options())
            createProperty(option);

        connect(mScanner, &QtSaneScanner::optionsChanged, this,
            &DevicePropertyBrowser::handleOptionsChanged);
        connect(mScanner, &QtSaneScanner::optionChanged, this,
            &DevicePropertyBrowser::handleOptionChanged);

        refreshProperties();
    }
}

void DevicePropertyBrowser::setShowAdvanced(bool showAdvanced)
{
    if (mShowAdvanced != showAdvanced) {
        mShowAdvanced = showAdvanced;
        refreshProperties();
    }
}

void DevicePropertyBrowser::createProperty(const QtSaneScanner::Option &option)
{
    const auto variantType = getVariantType(option);
    if (!variantType)
        return;

    auto description = option.description();
    auto title = QString(option.title());
    if (option.unit() != QtSaneScanner::Unit::None)
        title += " [" + getUnitString(option.unit()) + "]";
#if !defined(NDEBUG)
    description += " [" + option.name() + "]";
#endif

    auto property = mPropertyManager->addProperty(variantType, title);
    property->setWhatsThis(option.name());
    property->setToolTip(description);
    mProperties[option.name()] = property;
}

void DevicePropertyBrowser::refreshProperty(QtProperty &property_,
    const QtSaneScanner::Option &option)
{
    auto &property = static_cast<QtVariantProperty&>(property_);
    const auto &range = option.allowedRange();
    if (!option.allowedValues().isEmpty()) {
        auto enumNames = QStringList();
        for (const auto &value : option.allowedValues())
            enumNames << value.toString();
        property.setAttribute(QLatin1String("enumNames"), enumNames);
    }
    else if (range.min != range.max) {
        property.setAttribute(QLatin1String("minimum"), range.min);
        property.setAttribute(QLatin1String("maximum"), range.max);
        if (range.quantization != 0)
            property.setAttribute(QLatin1String("singleStep"), range.quantization);
    }

    if (option.type() == QtSaneScanner::Type::Value) {
        property.setAttribute(QLatin1String("singleStep"), 0.001);
        property.setAttribute(QLatin1String("decimals"), 4);
    }

    property.setEnabled(option.isSettable());
    property.setValue(
        option.allowedValues().isEmpty() ? option.value() :
        option.allowedValues().indexOf(option.value()));
}

void DevicePropertyBrowser::refreshProperties()
{
    disconnect(mPropertyManager, &QtVariantPropertyManager::valueChanged,
        this, &DevicePropertyBrowser::handleValueChanged);

    auto activeProperties = QList<QtProperty *>();
    for (auto property : qAsConst(mProperties))
        if (auto option = mScanner->findOption(property->whatsThis())) {
            if (option->isActive() && (option->isAdvanced() || mShowAdvanced)) {
                refreshProperty(*property, *option);
                activeProperties.append(property);
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

void DevicePropertyBrowser::handleOptionsChanged()
{
    refreshProperties();
}

void DevicePropertyBrowser::handleOptionChanged(const QtSaneScanner::Option &option)
{
    refreshProperty(*mProperties[option.name()], option);
}

void DevicePropertyBrowser::handleValueChanged(QtProperty *property, const QVariant &value)
{
    if (auto option = mScanner->findOption(property->whatsThis()))
        option->setValue(
            option->allowedValues().isEmpty() ? value :
            option->allowedValues().at(value.toInt()));
}
