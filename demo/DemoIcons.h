#pragma once

#include <QByteArray>
#include <QColor>
#include <QFile>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QSvgRenderer>

inline QIcon demoThemedIcon(const QString &resourcePath, const QString &color, const QSize &size = QSize(16, 16))
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly))
        return QIcon();
    QByteArray svgData = file.readAll();
    file.close();
    if (color != QLatin1String("#000000") && color != QLatin1String("black")) {
        QString svg = QString::fromUtf8(svgData);
        svg.replace(QStringLiteral("#000000"), color, Qt::CaseInsensitive);
        svg.replace(QStringLiteral("black"), color, Qt::CaseInsensitive);
        svg.replace(QStringLiteral("rgb(0,0,0)"), color, Qt::CaseInsensitive);
        svgData = svg.toUtf8();
    }
    QSvgRenderer renderer(svgData);
    if (!renderer.isValid())
        return QIcon();
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter);
    painter.end();
    QIcon icon;
    icon.addPixmap(pixmap);
    return icon;
}
