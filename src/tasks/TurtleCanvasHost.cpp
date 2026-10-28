#include "TurtleCanvasHost.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPolygonF>
#include <QPen>
#include <QFont>
#include <QImage>
#include <QJsonArray>
#include <QJsonValue>
#include <QResizeEvent>
#include <QSize>
#include <QSizePolicy>

TurtleCanvasHost::TurtleCanvasHost(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMinimumSize(160, 120);
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    setFocusPolicy(Qt::ClickFocus);
    setMouseTracking(true);
}

bool TurtleCanvasHost::hasHeightForWidth() const
{
    return true;
}

int TurtleCanvasHost::heightForWidth(int width) const
{
    return qMax(120, (width * 3 + 2) / 4);
}

QSize TurtleCanvasHost::sizeHint() const
{
    const int w = qMax(width() > 0 ? width() : 320, 240);
    return QSize(w, heightForWidth(w));
}

QSize TurtleCanvasHost::minimumSizeHint() const
{
    return QSize(160, 120);
}

void TurtleCanvasHost::setSampleForward(int pixels)
{
    m_sampleForward = qMax(0, pixels);
}

void TurtleCanvasHost::setLogicalCanvasSize(int width, int height)
{
    const int w = qMax(50, width);
    const int h = qMax(50, height);
    if (m_logicalWidth == w && m_logicalHeight == h)
        return;
    m_logicalWidth = w;
    m_logicalHeight = h;
    update();
}

void TurtleCanvasHost::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void TurtleCanvasHost::applyScene(const QJsonObject &scene)
{
    const QJsonArray items = scene.value(QStringLiteral("items")).toArray();
    if (items.isEmpty() && !m_hasScene)
        return;

    if (scene.contains(QStringLiteral("w")))
        m_logicalWidth = qMax(50, scene.value(QStringLiteral("w")).toInt(m_logicalWidth));
    if (scene.contains(QStringLiteral("h")))
        m_logicalHeight = qMax(50, scene.value(QStringLiteral("h")).toInt(m_logicalHeight));
    m_bg = scene.value(QStringLiteral("bg")).toString(QStringLiteral("white"));
    m_items.clear();
    m_items.reserve(items.size());
    for (const QJsonValue &val : items) {
        const QJsonObject o = val.toObject();
        FvtCanvasItem it;
        it.id = o.value(QStringLiteral("id")).toInt();
        it.type = o.value(QStringLiteral("type")).toString();
        const QJsonArray coords = o.value(QStringLiteral("coords")).toArray();
        it.coords.reserve(coords.size());
        for (const QJsonValue &c : coords)
            it.coords.append(c.toDouble());
        it.fill = o.value(QStringLiteral("fill")).toString();
        it.outline = o.value(QStringLiteral("outline")).toString();
        it.width = o.value(QStringLiteral("width")).toDouble(1.0);
        it.capstyle = o.value(QStringLiteral("capstyle")).toString();
        it.text = o.value(QStringLiteral("text")).toString();
        it.anchor = o.value(QStringLiteral("anchor")).toString();
        it.fontFamily = o.value(QStringLiteral("fontFamily")).toString(QStringLiteral("Arial"));
        it.fontSize = o.value(QStringLiteral("fontSize")).toInt(8);
        it.fontWeight = o.value(QStringLiteral("fontWeight")).toString();
        it.imagePath = o.value(QStringLiteral("image")).toString();
        m_items.append(it);
    }
    m_hasScene = true;
    update();
}

QColor TurtleCanvasHost::parseColor(const QString &name, bool *ok) const
{
    if (ok)
        *ok = false;
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || trimmed == QLatin1String("none"))
        return QColor();
    QColor c(trimmed);
    if (!c.isValid() && !trimmed.startsWith(QLatin1Char('#')))
        c = QColor(trimmed.toLower());
    if (!c.isValid())
        return QColor();
    if (ok)
        *ok = true;
    return c;
}

QPointF TurtleCanvasHost::toWidget(double cx, double cy, const QRect &box) const
{
    const qreal scale = qMin(box.width() / qreal(m_logicalWidth),
                             box.height() / qreal(m_logicalHeight));
    const QPointF origin(box.center());
    return QPointF(origin.x() + cx * scale, origin.y() + cy * scale);
}

QPointF TurtleCanvasHost::widgetToCanvas(const QPoint &pos) const
{
    const QRect box = rect().adjusted(1, 1, -2, -2);
    const qreal scale = qMin(box.width() / qreal(m_logicalWidth),
                             box.height() / qreal(m_logicalHeight));
    const QPointF origin(box.center());
    const qreal cx = (pos.x() - origin.x()) / scale + m_logicalWidth / 2.0;
    const qreal cy = (pos.y() - origin.y()) / scale + m_logicalHeight / 2.0;
    return QPointF(cx, cy);
}

void TurtleCanvasHost::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    bool bgOk = false;
    QColor bg = parseColor(m_bg, &bgOk);
    p.fillRect(rect(), bgOk ? bg : QColor("#ffffff"));

    QRect box = rect().adjusted(1, 1, -2, -2);
    if (box.width() < 4 || box.height() < 4)
        return;

    if (m_hasScene)
        drawScene(p, box);

    p.setPen(QColor("#d0d0d0"));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

void TurtleCanvasHost::drawScene(QPainter &p, const QRect &box) const
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setClipRect(box);

    for (const FvtCanvasItem &it : m_items) {
        if (it.type == QLatin1String("line")) {
            if (it.coords.size() < 4)
                continue;
            bool ok = false;
            QColor fill = parseColor(it.fill, &ok);
            if (!ok)
                continue;
            QPolygonF poly;
            for (int i = 0; i + 1 < it.coords.size(); i += 2)
                poly << toWidget(it.coords[i], it.coords[i + 1], box);
            QPen pen(fill, qMax(1.0, it.width));
            pen.setCapStyle(it.capstyle == QLatin1String("butt") ? Qt::FlatCap : Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawPolyline(poly);
        } else if (it.type == QLatin1String("polygon")) {
            if (it.coords.size() < 6)
                continue;
            QPolygonF poly;
            for (int i = 0; i + 1 < it.coords.size(); i += 2)
                poly << toWidget(it.coords[i], it.coords[i + 1], box);
            bool fillOk = false;
            bool outlineOk = false;
            QColor fill = parseColor(it.fill, &fillOk);
            QColor outline = parseColor(it.outline, &outlineOk);
            if (!fillOk && !outlineOk)
                continue;
            p.setBrush(fillOk ? QBrush(fill) : Qt::NoBrush);
            if (outlineOk) {
                QPen pen(outline, qMax(1.0, it.width));
                p.setPen(pen);
            } else if (fillOk) {
                p.setPen(QPen(fill, 1));
            } else {
                p.setPen(Qt::NoPen);
            }
            p.drawPolygon(poly);
        } else if (it.type == QLatin1String("text")) {
            if (it.coords.size() < 2 || it.text.isEmpty())
                continue;
            bool ok = false;
            QColor fill = parseColor(it.fill, &ok);
            if (!ok)
                fill = Qt::black;
            QFont font(it.fontFamily.isEmpty() ? QStringLiteral("Arial") : it.fontFamily);
            font.setPixelSize(qMax(8, it.fontSize));
            font.setBold(it.fontWeight.contains(QLatin1String("bold"), Qt::CaseInsensitive));
            font.setItalic(it.fontWeight.contains(QLatin1String("italic"), Qt::CaseInsensitive));
            p.setFont(font);
            p.setPen(fill);
            const QPointF pos = toWidget(it.coords[0], it.coords[1], box);
            const QFontMetrics fm(font);
            QRect br = fm.boundingRect(it.text);
            const QString a = it.anchor.toLower();
            if (a.contains(QLatin1Char('s')))
                br.moveBottom(qRound(pos.y()));
            else if (a.contains(QLatin1Char('n')))
                br.moveTop(qRound(pos.y()));
            else
                br.moveCenter(QPoint(br.center().x(), qRound(pos.y())));
            if (a.contains(QLatin1Char('e')))
                br.moveRight(qRound(pos.x()));
            else if (a.contains(QLatin1Char('w')))
                br.moveLeft(qRound(pos.x()));
            else
                br.moveCenter(QPoint(qRound(pos.x()), br.center().y()));
            p.drawText(br, Qt::AlignCenter, it.text);
        } else if (it.type == QLatin1String("image")) {
            if (it.coords.size() < 2 || it.imagePath.isEmpty())
                continue;
            QImage img(it.imagePath);
            if (img.isNull())
                continue;
            const QPointF pos = toWidget(it.coords[0], it.coords[1], box);
            p.drawImage(QPointF(pos.x() - img.width() / 2.0, pos.y() - img.height() / 2.0), img);
        }
    }
    p.restore();
}

void TurtleCanvasHost::mousePressEvent(QMouseEvent *event)
{
    const QPointF c = widgetToCanvas(event->pos());
    QJsonObject o;
    o.insert(QStringLiteral("type"), QStringLiteral("click"));
    o.insert(QStringLiteral("x"), c.x());
    o.insert(QStringLiteral("y"), c.y());
    o.insert(QStringLiteral("button"), event->button() == Qt::RightButton ? 3 : 1);
    emit fvtEvent(o);
    setFocus();
}

void TurtleCanvasHost::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF c = widgetToCanvas(event->pos());
    QJsonObject o;
    if (event->buttons() != Qt::NoButton)
        o.insert(QStringLiteral("type"), QStringLiteral("drag"));
    else
        o.insert(QStringLiteral("type"), QStringLiteral("move"));
    o.insert(QStringLiteral("x"), c.x());
    o.insert(QStringLiteral("y"), c.y());
    o.insert(QStringLiteral("button"), 1);
    emit fvtEvent(o);
}

void TurtleCanvasHost::mouseReleaseEvent(QMouseEvent *event)
{
    const QPointF c = widgetToCanvas(event->pos());
    QJsonObject o;
    o.insert(QStringLiteral("type"), QStringLiteral("release"));
    o.insert(QStringLiteral("x"), c.x());
    o.insert(QStringLiteral("y"), c.y());
    o.insert(QStringLiteral("button"), event->button() == Qt::RightButton ? 3 : 1);
    emit fvtEvent(o);
}

void TurtleCanvasHost::keyPressEvent(QKeyEvent *event)
{
    QJsonObject o;
    o.insert(QStringLiteral("type"), QStringLiteral("key"));
    o.insert(QStringLiteral("key"), event->text().isEmpty()
             ? QString::number(event->key())
             : event->text());
    o.insert(QStringLiteral("char"), event->text());
    emit fvtEvent(o);
}

Qt::Alignment TurtleCanvasHost::alignmentFromAnchor(const QString &anchor) const
{
    Q_UNUSED(anchor);
    return Qt::AlignCenter;
}
