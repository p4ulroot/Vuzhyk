#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include "BrowserWindow.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication app(argc, argv);
    QApplication::setApplicationName("Embedded Browser Extension");
    QApplication::setOrganizationName("Vuzhyk");

    QString initialPath;
    if (argc > 1) {
        initialPath = QString::fromLocal8Bit(argv[1]);
    }

    BrowserWindow window(initialPath);
    window.show();

    return app.exec();
}

