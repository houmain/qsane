#include "MainWindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QTranslator>
#include <QLibraryInfo>

namespace
{
    void showUnhandledExceptionMessage(const QString &message)
    {
        const auto title = QCoreApplication::applicationName() +
            " - Unhandled exception";
        QMessageBox(QMessageBox::Critical, title, message).exec();
    }
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("qsane");
    QCoreApplication::setApplicationName("QSane");
#if __has_include("_version.h")
    QCoreApplication::setApplicationVersion(
            # include "_version.h"
                );
#endif

    auto locale = QLocale();
    auto translationsLocation = QStringLiteral("../share/qsane/translations");
#if !defined(NDEBUG)
    locale = QLocale("de");
    translationsLocation = QStringLiteral(".");
#endif

    auto qtTranslator = QTranslator();
    auto appTranslator = QTranslator();
    auto app = QApplication(argc, argv);
    try {
        if (qtTranslator.load(locale,
                QStringLiteral("qt"),
                QStringLiteral("_"),
                QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
            app.installTranslator(&qtTranslator);

        if (appTranslator.load(locale,
                QStringLiteral("lang"),
                QStringLiteral("_"),
                translationsLocation))
            app.installTranslator(&appTranslator);

        auto window = MainWindow();
        window.show();
        app.exec();
    }
    catch (const SaneException &ex)
    {
        showUnhandledExceptionMessage(
            QString(ex.what()) + " failed: " + ex.status_msg());
    }
    catch (const std::exception &ex)
    {
        showUnhandledExceptionMessage(ex.what());
    }
}

