#include "DemoHouseField.h"

#include <QApplication>
#include <QIcon>
#include <QTimer>
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

    DemoHouseField field;
    field.show();
    field.beginRecording();
    QTimer::singleShot(400, [&field]() { field.beginDrawing(); });
    return app.exec();
}
