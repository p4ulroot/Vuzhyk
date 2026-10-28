#include "DemoCursor.h"

#include <QFile>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

namespace {

quint16 curU16(const char *p)
{
    return quint16(uchar(p[0])) | (quint16(uchar(p[1])) << 8);
}

quint32 curU32(const char *p)
{
    return quint32(uchar(p[0])) | (quint32(uchar(p[1])) << 8)
         | (quint32(uchar(p[2])) << 16) | (quint32(uchar(p[3])) << 24);
}

} // namespace

DemoCursor::DemoCursor(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    if (!loadFromCur(QStringLiteral(":/cursors/Pointer.cur")))
        setFixedSize(28, 28);
    hide();
}

bool DemoCursor::loadFromCur(const QString &resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray data = file.readAll();
    if (data.size() < 22)
        return false;

    const char *base = data.constData();
    const quint16 count = curU16(base + 4);
    if (count < 1)
        return false;

    int best = 0;
    int bestSize = -1;
    for (int i = 0; i < int(count); ++i) {
        const char *e = base + 6 + i * 16;
        const int w = uchar(e[0]) ? uchar(e[0]) : 256;
        if (w > bestSize) {
            bestSize = w;
            best = i;
        }
    }

    const char *entry = base + 6 + best * 16;
    m_hotspot = QPoint(int(curU16(entry + 4)), int(curU16(entry + 6)));
    const quint32 bytesInRes = curU32(entry + 8);
    const quint32 imageOffset = curU32(entry + 12);
    if (imageOffset + bytesInRes > quint32(data.size()))
        return false;

    const QByteArray image = data.mid(int(imageOffset), int(bytesInRes));
    if (image.startsWith("\x89PNG"))
        m_pixmap.loadFromData(image, "PNG");
    if (m_pixmap.isNull())
        m_pixmap.loadFromData(data, "CUR");
    if (m_pixmap.isNull())
        m_pixmap.loadFromData(data, "ICO");
    if (m_pixmap.isNull())
        return false;

    if (m_hotspot.x() < 0 || m_hotspot.x() >= m_pixmap.width())
        m_hotspot.setX(1);
    if (m_hotspot.y() < 0 || m_hotspot.y() >= m_pixmap.height())
        m_hotspot.setY(1);

    const QSize small(
        qMax(1, qRound(m_pixmap.width() / 4.0)),
        qMax(1, qRound(m_pixmap.height() / 4.0)));
    m_hotspot = QPoint(
        qRound(m_hotspot.x() * (small.width() / qreal(m_pixmap.width()))),
        qRound(m_hotspot.y() * (small.height() / qreal(m_pixmap.height()))));
    m_pixmap = m_pixmap.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    setFixedSize(m_pixmap.size() + QSize(2, 2));
    return true;
}

void DemoCursor::setHotPos(const QPoint &pos)
{
    m_hotPos = pos;
    const QPoint extra = m_pressed ? QPoint(1, 1) : QPoint(0, 0);
    move(pos - m_hotspot + extra);
    raise();
}

void DemoCursor::setPressed(bool pressed)
{
    m_pressed = pressed;
    setHotPos(m_hotPos);
    update();
}

void DemoCursor::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (!m_pixmap.isNull()) {
        p.drawPixmap(0, 0, m_pixmap);
        return;
    }

    QPolygonF body;
    body << QPointF(1, 1)
         << QPointF(1, 18)
         << QPointF(5, 14)
         << QPointF(8, 22)
         << QPointF(10.5, 21)
         << QPointF(7, 13)
         << QPointF(14, 13);

    QPainterPath shadow;
    shadow.addPolygon(body.translated(1, 1));
    p.fillPath(shadow, QColor(0, 0, 0, 70));
    QPainterPath main;
    main.addPolygon(body);
    p.fillPath(main, Qt::white);
    p.setPen(QPen(Qt::black, 1.1));
    p.drawPath(main);
}
