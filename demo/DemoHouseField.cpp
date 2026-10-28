#include "DemoHouseField.h"
#include "DemoRecorder.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QTimer>
#include <QPolygonF>
#include <QtMath>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

struct Turtle {
    QPointF p { 0.0, 0.0 };
    double heading { 0.0 };
    bool down { true };
    QVector<DemoHouseField::Seg> segs;

    void setpos(double x, double y)
    {
        const QPointF n(x, y);
        const QPointF d = n - p;
        const double len2 = d.x() * d.x() + d.y() * d.y();
        if (len2 > 0.0001) {
            DemoHouseField::Seg s;
            s.a = p;
            s.b = n;
            s.draw = down;
            segs.append(s);
            heading = std::atan2(d.y(), d.x()) * 180.0 / M_PI;
        }
        p = n;
    }

    void setheading(double deg) { heading = deg; }

    void fd(double dist)
    {
        const double rad = heading * M_PI / 180.0;
        setpos(p.x() + dist * std::cos(rad), p.y() + dist * std::sin(rad));
    }

    void lt(double deg) { heading += deg; }
    void rt(double deg) { heading -= deg; }
    void pu() { down = false; }
    void pd() { down = true; }

    void box(double x, double y, double w, double h)
    {
        pu();
        setpos(x, y);
        pd();
        setheading(0);
        fd(w);
        lt(90);
        fd(h);
        lt(90);
        fd(w);
        lt(90);
        fd(h);
        lt(90);
    }

    void window(double x, double y, double s)
    {
        box(x, y, s, s);
        pu();
        setpos(x, y + s * 0.5);
        pd();
        setheading(0);
        fd(s);
        pu();
        setpos(x + s * 0.5, y);
        pd();
        setheading(90);
        fd(s);
    }

    void circle(double cx, double cy, double r, int steps = 36)
    {
        pu();
        setpos(cx + r, cy);
        pd();
        for (int i = 1; i <= steps; ++i) {
            const double a = (360.0 * i) / steps * M_PI / 180.0;
            setpos(cx + r * std::cos(a), cy + r * std::sin(a));
        }
    }
};

} // namespace

DemoHouseField::DemoHouseField(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setFixedSize(800, 800);
    setWindowTitle(QStringLiteral("Vuzhyk"));
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::white);
    setPalette(pal);
    setAutoFillBackground(true);

    m_timer = new QTimer(this);
    m_timer->setInterval(16);
    connect(m_timer, &QTimer::timeout, this, [this]() { tick(); });

    buildPath();
    if (!m_segs.isEmpty())
        m_turtlePos = m_segs.first().a;

    m_recorder = new DemoRecorder(this);
}

DemoHouseField::~DemoHouseField()
{
    if (m_recorder)
        m_recorder->stop();
}

void DemoHouseField::beginRecording()
{
    if (m_recorder)
        m_recorder->start(this, QStringLiteral("DemoHouseField"));
}

void DemoHouseField::beginDrawing()
{
    m_strokes.clear();
    m_seg = 0;
    m_segT = 0.0;
    m_playing = !m_segs.isEmpty();
    m_closing = false;
    if (!m_segs.isEmpty())
        m_turtlePos = m_segs.first().a;
    if (m_playing)
        m_timer->start();
    update();
}

void DemoHouseField::buildPath()
{
    Turtle t;
    t.pu();
    t.setpos(-160, -180);
    t.pd();
    t.setheading(0);

    t.fd(320);
    t.lt(90);
    t.fd(200);
    t.lt(90);
    t.fd(320);
    t.lt(90);
    t.fd(200);

    t.pu();
    t.setpos(-160, 20);
    t.pd();
    t.setpos(0, 190);
    t.setpos(160, 20);

    auto roofY = [](double x) {
        return 190.0 - 170.0 * x / 160.0;
    };
    const double chimL = 78.0;
    const double chimR = 114.0;
    const double chimTop = 186.0;
    t.pu();
    t.setpos(chimL, roofY(chimL));
    t.pd();
    t.setpos(chimL, chimTop);
    t.setpos(chimR, chimTop);
    t.setpos(chimR, roofY(chimR));
    t.pu();
    t.setpos(chimL - 5.0, chimTop);
    t.pd();
    t.setpos(chimR + 5.0, chimTop);
    t.setpos(chimR + 5.0, chimTop + 8.0);
    t.setpos(chimL - 5.0, chimTop + 8.0);
    t.setpos(chimL - 5.0, chimTop);

    t.box(-36, -180, 72, 116);
    t.pu();
    t.setpos(20, -116);
    t.pd();
    t.circle(26, -116, 6, 16);

    t.window(-124, -76, 64);
    t.window(60, -76, 64);
    t.window(-24, 76, 48);

    t.pu();
    t.setpos(-36, -180);
    t.pd();
    t.setpos(-100, -350);
    t.pu();
    t.setpos(36, -180);
    t.pd();
    t.setpos(100, -350);

    t.box(-304, -180, 28, 80);
    t.pu();
    t.setpos(-352, -100);
    t.pd();
    t.setpos(-290, 56);
    t.setpos(-228, -100);
    t.setpos(-352, -100);
    t.pu();
    t.setpos(-336, -36);
    t.pd();
    t.setpos(-290, 96);
    t.setpos(-244, -36);
    t.setpos(-336, -36);

    t.circle(276, 276, 44, 32);
    for (int i = 0; i < 8; ++i) {
        const double a = i * 45.0 * M_PI / 180.0;
        const QPointF c(276, 276);
        t.pu();
        t.setpos(c.x() + 52 * std::cos(a), c.y() + 52 * std::sin(a));
        t.pd();
        t.setpos(c.x() + 80 * std::cos(a), c.y() + 80 * std::sin(a));
    }

    t.pu();
    t.setpos(216, -180);
    t.pd();
    t.setheading(0);
    t.fd(124);
    for (int i = 0; i < 7; ++i) {
        t.pu();
        t.setpos(216.0 + i * 18.0, -180);
        t.pd();
        t.setheading(90);
        t.fd(36);
        t.rt(90);
        t.fd(18);
        t.rt(90);
        t.fd(36);
    }

    m_segs = t.segs;
}

QPointF DemoHouseField::toWidget(const QPointF &logical) const
{
    return QPointF(width() * 0.5 + logical.x(),
                   height() * 0.5 - logical.y());
}

void DemoHouseField::tick()
{
    if (!m_playing || m_seg >= m_segs.size()) {
        m_timer->stop();
        m_playing = false;
        finishAndClose();
        return;
    }

    const double speed = 7.2;
    double remaining = speed;
    while (remaining > 0.0 && m_seg < m_segs.size()) {
        const Seg &s = m_segs.at(m_seg);
        const QPointF d = s.b - s.a;
        const double len = std::sqrt(d.x() * d.x() + d.y() * d.y());
        if (len < 0.01 || !s.draw) {
            m_turtlePos = s.b;
            if (len >= 0.01)
                m_heading = std::atan2(d.y(), d.x()) * 180.0 / M_PI;
            m_seg++;
            m_segT = 0.0;
            continue;
        }

        const double left = 1.0 - m_segT;
        const double stepT = remaining / len;
        if (m_segT < 0.0001 && (m_strokes.isEmpty() || m_strokes.last().isEmpty()
                                || m_strokes.last().last() != s.a)) {
            m_strokes.append(QVector<QPointF>() << s.a);
        }
        if (stepT >= left) {
            remaining -= left * len;
            m_seg++;
            m_segT = 0.0;
            m_turtlePos = s.b;
            m_heading = std::atan2(d.y(), d.x()) * 180.0 / M_PI;
            if (!m_strokes.isEmpty())
                m_strokes.last().append(s.b);
        } else {
            m_segT += stepT;
            m_turtlePos = s.a + d * m_segT;
            m_heading = std::atan2(d.y(), d.x()) * 180.0 / M_PI;
            remaining = 0.0;
        }
    }

    if (m_seg >= m_segs.size()) {
        m_playing = false;
        m_timer->stop();
        finishAndClose();
    }
    update();
}

void DemoHouseField::finishAndClose()
{
    if (m_closing)
        return;
    m_closing = true;
    update();
    QTimer::singleShot(900, this, [this]() {
        if (m_recorder)
            m_recorder->stop();
        close();
    });
}

void DemoHouseField::drawAxes(QPainter &p) const
{
    p.setRenderHint(QPainter::Antialiasing, false);
    const int xLimit = width() / 2;
    const int yLimit = height() / 2;
    const int step = 50;
    const QColor gridColor(QStringLiteral("lightblue"));
    const QColor axisColor(QStringLiteral("green"));

    auto gridCoords = [step](int limit) {
        QVector<int> out;
        for (int n = 0; ; ++n) {
            const int v = n * step;
            if (v > limit)
                break;
            out.append(v);
            if (n)
                out.append(-v);
        }
        return out;
    };
    const QVector<int> xCoords = gridCoords(xLimit);
    const QVector<int> yCoords = gridCoords(yLimit);

    p.setPen(QPen(gridColor, 1));
    for (int x : xCoords) {
        if (x == 0)
            continue;
        p.drawLine(toWidget(QPointF(x, -yLimit)), toWidget(QPointF(x, yLimit)));
    }
    for (int y : yCoords) {
        if (y == 0)
            continue;
        p.drawLine(toWidget(QPointF(-xLimit, y)), toWidget(QPointF(xLimit, y)));
    }

    p.setPen(QPen(axisColor, 2));
    p.drawLine(toWidget(QPointF(0, -yLimit)), toWidget(QPointF(0, yLimit)));
    p.drawLine(toWidget(QPointF(-xLimit, 0)), toWidget(QPointF(xLimit, 0)));

    auto drawText = [&](const QPointF &pos, const QString &text, int align) {
        const QFontMetrics fm(p.font());
        const int w = fm.horizontalAdvance(text);
        const int h = fm.height();
        QPoint topLeft(qRound(pos.x()), qRound(pos.y()));
        if (align & Qt::AlignHCenter)
            topLeft.rx() -= w / 2;
        else if (align & Qt::AlignRight)
            topLeft.rx() -= w;
        if (align & Qt::AlignVCenter)
            topLeft.ry() -= h / 2;
        else if (align & Qt::AlignBottom)
            topLeft.ry() -= h;
        p.setPen(axisColor);
        p.drawText(topLeft.x(), topLeft.y() + fm.ascent(), text);
    };

    p.setFont(QFont(QStringLiteral("Arial"), 8));
    for (int x : xCoords) {
        if (x == 0)
            continue;
        p.setPen(QPen(axisColor, 2));
        p.drawLine(toWidget(QPointF(x, 0)), toWidget(QPointF(x, 5)));
        p.drawLine(toWidget(QPointF(x, 0)), toWidget(QPointF(x, -5)));
        drawText(toWidget(QPointF(x, -20)), QString::number(x), Qt::AlignHCenter | Qt::AlignTop);
    }
    for (int y : yCoords) {
        if (y == 0)
            continue;
        p.setPen(QPen(axisColor, 2));
        p.drawLine(toWidget(QPointF(0, y)), toWidget(QPointF(-5, y)));
        p.drawLine(toWidget(QPointF(0, y)), toWidget(QPointF(5, y)));
        drawText(toWidget(QPointF(-20, y)), QString::number(y), Qt::AlignRight | Qt::AlignVCenter);
    }

    p.setFont(QFont(QStringLiteral("Arial"), 14, QFont::Bold));
    drawText(toWidget(QPointF(10, yLimit - 30)), QStringLiteral("Y"), Qt::AlignHCenter | Qt::AlignVCenter);
    drawText(toWidget(QPointF(xLimit - 30, -20)), QStringLiteral("X"), Qt::AlignHCenter | Qt::AlignVCenter);
}

void DemoHouseField::drawTurtle(QPainter &p) const
{
    const QPointF tip = toWidget(m_turtlePos);
    const double ang = -m_heading * M_PI / 180.0;
    const double s = 11.0;
    QPolygonF turtle;
    turtle << QPointF(s, 0)
           << QPointF(-s * 0.7, s * 0.55)
           << QPointF(-s * 0.35, 0)
           << QPointF(-s * 0.7, -s * 0.55);
    for (int i = 0; i < turtle.size(); ++i) {
        const QPointF r = turtle[i];
        turtle[i] = tip + QPointF(r.x() * std::cos(ang) - r.y() * std::sin(ang),
                                  r.x() * std::sin(ang) + r.y() * std::cos(ang));
    }
    p.setPen(QPen(QColor("#1a1a1a"), 1.0));
    p.setBrush(QColor("#3cb371"));
    p.drawPolygon(turtle);
}

void DemoHouseField::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::white);
    drawAxes(p);
    p.setRenderHint(QPainter::Antialiasing, true);

    QPen pen(QColor("#222222"), 2.0);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    for (const QVector<QPointF> &stroke : m_strokes) {
        if (stroke.size() < 2)
            continue;
        QPolygonF poly;
        for (const QPointF &pt : stroke)
            poly << toWidget(pt);
        p.drawPolyline(poly);
    }
    if (m_playing && m_seg < m_segs.size()) {
        const Seg &s = m_segs.at(m_seg);
        if (s.draw && m_segT > 0.0)
            p.drawLine(toWidget(s.a), toWidget(m_turtlePos));
    }
    drawTurtle(p);
}
