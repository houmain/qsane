#include "MainWindow.h"
#include <QApplication>
#include <QTranslator>
#include <QLibraryInfo>
#include <QDebug>

int main(int argc, char *argv[]) try
{
    QCoreApplication::setOrganizationName("qsane");
    QCoreApplication::setApplicationName("QSane");
#if __has_include("_version.h")
    QCoreApplication::setApplicationVersion(
            # include "_version.h"
                );
#endif

    auto qtTranslator = QTranslator();
    auto appTranslator = QTranslator();
    auto app = QApplication(argc, argv);

    auto locale = QLocale();
    auto translationsDir = QCoreApplication::applicationDirPath();
    translationsDir += "/../share/qsane/translations";
#if !defined(NDEBUG)
    locale = QLocale("de");
    translationsDir = ".";
#endif

    if (qtTranslator.load(locale,
            QStringLiteral("qt"),
            QStringLiteral("_"),
            QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslator);

    if (appTranslator.load(locale,
            QStringLiteral("lang"),
            QStringLiteral("_"),
            translationsDir))
        app.installTranslator(&appTranslator);

    auto window = MainWindow();
    window.show();
    app.exec();
}
catch (const std::exception &ex)
{
    qCritical() << "unhandled exception: " << ex.what() << '\n';
}
