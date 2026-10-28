#include "DemoRobotField.h"

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
    app.setApplicationName(QStringLiteral("pyrob"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/icons/logo/vuzhyk.ico")));

    DemoRobotField field;
    field.show();
    field.repaint();
    QApplication::processEvents();
    field.beginRecording();
    QTimer::singleShot(400, [&field]() { field.beginDemo(); });
    return app.exec();
}
