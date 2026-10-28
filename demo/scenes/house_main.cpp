#include "DemoIdeWindow.h"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    SetProcessDPIAware();
#endif
    QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Vuzhyk"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/icons/logo/vuzhyk.ico")));
    QFont appFont = app.font();
    if (appFont.pointSizeF() > 0)
        appFont.setPointSizeF(appFont.pointSizeF() * 1.5);
    else if (appFont.pixelSize() > 0)
        appFont.setPixelSize(qRound(appFont.pixelSize() * 1.5));
    app.setFont(appFont);

    DemoIdeWindow window;
    window.show();
    window.beginHouseDemo();
    return app.exec();
}
