#include "DemoTurtleCanvas.h"
#include "DemoScale.h"

#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QTimer>
#include <QtMath>
#include <QPolygonF>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

DemoTurtleCanvas::DemoTurtleCanvas(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(dsSize(160, 120));
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::white);
    setPalette(pal);

    m_timer = new QTimer(this);
    m_timer->setInterval(16);
    connect(m_timer, &QTimer::timeout, this, &DemoTurtleCanvas::tick);
}

int DemoTurtleCanvas::heightForWidth(int width) const
{
    return qMax(ds(120), (width * 3 + 2) / 4);
}

QSize DemoTurtleCanvas::sizeHint() const
{
    const int w = qMax(width() > 0 ? width() : ds(320), ds(240));
    return QSize(w, heightForWidth(w));
}

QSize DemoTurtleCanvas::minimumSizeHint() const
{
    return dsSize(160, 120);
}

void DemoTurtleCanvas::setSamplePath(const QVector<QPointF> &points)
{
    m_sample = points;
    update();
}

void DemoTurtleCanvas::resetStudent()
{
    m_timer->stop();
    m_playing = false;
    m_path.clear();
    m_drawn.clear();
    m_seg = 0;
    m_segT = 0.0;
    m_heading = 0.0;
    m_turtlePos = QPointF(0, 0);
    update();
}

void DemoTurtleCanvas::playPath(const QVector<QPointF> &points, int durationMs)
{
    m_path = points;
    m_drawn.clear();
    m_seg = 0;
    m_segT = 0.0;
    m_unitsPerTick = 2.4;
    m_playing = !m_path.isEmpty();
    if (!m_path.isEmpty()) {
        m_turtlePos = m_path.first();
        m_drawn.append(m_turtlePos);
    }
    if (durationMs > 0 && m_path.size() >= 2) {
        double total = 0.0;
        for (int i = 1; i < m_path.size(); ++i) {
            const QPointF d = m_path.at(i) - m_path.at(i - 1);
            total += qSqrt(d.x() * d.x() + d.y() * d.y());
        }
        const double ticks = qMax(1.0, durationMs / 16.0);
        m_unitsPerTick = qMax(0.4, total / ticks);
    }
    if (m_playing)
        m_timer->start();
    update();
}

void DemoTurtleCanvas::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

QPointF DemoTurtleCanvas::toWidget(const QPointF &logical) const
{
    const double sx = width() / double(m_logicalW);
    const double sy = height() / double(m_logicalH);
    const double x = width() * 0.5 + logical.x() * sx;
    const double y = height() * 0.5 - logical.y() * sy;
    return QPointF(x, y);
}

void DemoTurtleCanvas::tick()
{
    if (!m_playing || m_path.size() < 2) {
        m_timer->stop();
        m_playing = false;
        emit playbackFinished();
        return;
    }

    const double speed = m_unitsPerTick;
    double remaining = speed;
    while (remaining > 0.0 && m_seg + 1 < m_path.size()) {
        const QPointF a = m_path.at(m_seg);
        const QPointF b = m_path.at(m_seg + 1);
        const QPointF d = b - a;
        const double len = qSqrt(d.x() * d.x() + d.y() * d.y());
        if (len < 0.01) {
            m_seg++;
            m_segT = 0.0;
            continue;
        }
        const double left = 1.0 - m_segT;
        const double stepT = remaining / len;
        if (stepT >= left) {
            remaining -= left * len;
            m_seg++;
            m_segT = 0.0;
            m_turtlePos = b;
            m_heading = qAtan2(d.y(), d.x()) * 180.0 / M_PI;
            m_drawn.append(b);
        } else {
            m_segT += stepT;
            m_turtlePos = a + d * m_segT;
            m_heading = qAtan2(d.y(), d.x()) * 180.0 / M_PI;
            remaining = 0.0;
        }
    }

    if (m_seg + 1 >= m_path.size()) {
        m_playing = false;
        m_timer->stop();
        if (!m_path.isEmpty())
            m_turtlePos = m_path.last();
        emit playbackFinished();
    }
    update();
}

void DemoTurtleCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::white);
    p.setRenderHint(QPainter::Antialiasing, true);

    auto drawPolyline = [&](const QVector<QPointF> &pts, const QColor &color, double width) {
        if (pts.size() < 2)
            return;
        QPolygonF poly;
        for (const QPointF &pt : pts)
            poly << toWidget(pt);
        QPen pen(color, width);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(poly);
    };

    drawPolyline(m_sample, QColor("#8EC5FF"), 2.4);

    QVector<QPointF> live = m_drawn;
    if (m_playing || !m_drawn.isEmpty())
        live.append(m_turtlePos);
    drawPolyline(live, QColor("#222222"), 1.8);

    const QPointF tip = toWidget(m_turtlePos);
    const double ang = -m_heading * M_PI / 180.0;
    const double s = ds(9);
    QPolygonF turtle;
    turtle << QPointF(s, 0)
           << QPointF(-s * 0.7, s * 0.55)
           << QPointF(-s * 0.35, 0)
           << QPointF(-s * 0.7, -s * 0.55);
    for (int i = 0; i < turtle.size(); ++i) {
        const QPointF r = turtle[i];
        const double x = r.x() * qCos(ang) - r.y() * qSin(ang);
        const double y = r.x() * qSin(ang) + r.y() * qCos(ang);
        turtle[i] = tip + QPointF(x, y);
    }
    p.setPen(QPen(QColor("#1a1a1a"), 1.0));
    p.setBrush(QColor("#3cb371"));
    p.drawPolygon(turtle);
}
