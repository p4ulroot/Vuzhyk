#include "DemoRobotField.h"
#include "DemoRecorder.h"

#include <QFont>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPointF>
#include <QTimer>

namespace {

constexpr int kWin = 800;
constexpr int kFieldN = 9;
constexpr int kCell = 70;
constexpr int kWallThick = 4;
constexpr int kGridThick = 2;
constexpr int kXOff = (kWin - kCell * kFieldN) / 2;
constexpr int kYOff = kXOff;
constexpr int kRobotRadius = (kCell - 2 * kWallThick - 18) / 2;
constexpr int kRobotThick = 9;
constexpr int kParkRadius = kRobotRadius / 2;
constexpr int kMoveMs = 150;
constexpr int kCelebrateMs = 1000;

const QColor kWallColor("#bfc0c2");
const QColor kGridColor("#e6e6e6");
const QColor kRobotColor("#bdbec0");
const QColor kSuccessColor("#92dcb9");
const QColor kParkColor("#000000");
const QColor kEmpty("#ffffff");
const QColor kToFill("#a1dcd8");
const QColor kFilled("#fff8d1");
const QColor kBg("#d9d9d9");

} // namespace

DemoRobotField::DemoRobotField(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setFixedSize(kWin, kWin);
    setWindowTitle(QStringLiteral("pyrob"));
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, kBg);
    setPalette(pal);
    setAutoFillBackground(true);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, [this]() { tick(); });
    m_recorder = new DemoRecorder(this);
    loadTask();
}

DemoRobotField::~DemoRobotField()
{
    if (m_recorder)
        m_recorder->stop();
}

void DemoRobotField::beginRecording()
{
    if (!m_rows)
        loadTask();
    repaint();
    if (m_recorder)
        m_recorder->start(this, QStringLiteral("DemoRobotField"));
}

void DemoRobotField::beginDemo()
{
    m_steps.clear();
    m_step = 0;
    m_success = false;
    m_closing = false;
    loadTask();
    solveTaskTriangle();
    m_steps.append({ StepKind::Celebrate, m_i, m_j });
    m_steps.append({ StepKind::Finish, 0, 0 });

    loadTask();
    m_success = false;
    if (!m_steps.isEmpty())
        m_timer->start(kMoveMs);
    update();
}

void DemoRobotField::setFieldSize(int rows, int cols)
{
    m_rows = rows;
    m_cols = cols;
    m_field.fill(QVector<Cell>(cols), rows);
    for (int j = 0; j < cols; ++j) {
        m_field[0][j].walls |= WallTop;
        m_field[rows - 1][j].walls |= WallBottom;
    }
    for (int i = 0; i < rows; ++i) {
        m_field[i][0].walls |= WallLeft;
        m_field[i][cols - 1].walls |= WallRight;
    }
    m_i = 0;
    m_j = 0;
}

void DemoRobotField::putWall(int i, int j, int flags)
{
    if (i < 0 || i >= m_rows || j < 0 || j >= m_cols)
        return;
    m_field[i][j].walls |= flags;
    if ((flags & WallLeft) && j > 0)
        m_field[i][j - 1].walls |= WallRight;
    if ((flags & WallRight) && j + 1 < m_cols)
        m_field[i][j + 1].walls |= WallLeft;
    if ((flags & WallTop) && i > 0)
        m_field[i - 1][j].walls |= WallBottom;
    if ((flags & WallBottom) && i + 1 < m_rows)
        m_field[i + 1][j].walls |= WallTop;
}

void DemoRobotField::setToFill(int i, int j)
{
    if (i < 0 || i >= m_rows || j < 0 || j >= m_cols)
        return;
    m_field[i][j].type = ToFill;
}

bool DemoRobotField::wallAbove() const
{
    return m_field[m_i][m_j].walls & WallTop;
}

bool DemoRobotField::wallBeneath() const
{
    return m_field[m_i][m_j].walls & WallBottom;
}

bool DemoRobotField::wallLeft() const
{
    return m_field[m_i][m_j].walls & WallLeft;
}

bool DemoRobotField::wallRight() const
{
    return m_field[m_i][m_j].walls & WallRight;
}

void DemoRobotField::simMove(int di, int dj)
{
    m_i += di;
    m_j += dj;
    m_steps.append({ StepKind::Move, m_i, m_j });
}

void DemoRobotField::simFill()
{
    m_field[m_i][m_j].type = Filled;
    m_steps.append({ StepKind::Fill, m_i, m_j });
}

void DemoRobotField::loadTask()
{
    m_success = false;
    buildTaskTriangle();
}

void DemoRobotField::buildTaskTriangle()
{
    setFieldSize(kFieldN, kFieldN);
    for (int i = 1; i < kFieldN - 1; ++i) {
        for (int j = 1; j <= i; ++j)
            setToFill(i, j);
    }
    m_field[kFieldN - 1][1].parking = true;
    m_i = 0;
    m_j = 0;
}

void DemoRobotField::solveTaskTriangle()
{
    m_i = 0;
    m_j = 0;
    simMove(1, 0);
    simMove(0, 1);
    const int lastFill = m_rows - 2;
    for (int row = 1; row <= lastFill; ++row) {
        if (row % 2 == 1) {
            for (int j = 1; j <= row; ++j) {
                simFill();
                if (j < row)
                    simMove(0, 1);
            }
        } else {
            simMove(0, 1);
            for (int j = row; j >= 1; --j) {
                simFill();
                if (j > 1)
                    simMove(0, -1);
            }
        }
        if (row < lastFill)
            simMove(1, 0);
    }
    simMove(1, 0);
    while (m_j > 1)
        simMove(0, -1);
}

void DemoRobotField::tick()
{
    if (m_step >= m_steps.size()) {
        m_timer->stop();
        finishAndClose();
        return;
    }
    const Step s = m_steps.at(m_step++);
    switch (s.kind) {
    case StepKind::Move:
        m_i = s.i;
        m_j = s.j;
        m_timer->start(kMoveMs);
        break;
    case StepKind::Fill:
        m_field[s.i][s.j].type = Filled;
        m_timer->start(kMoveMs);
        break;
    case StepKind::Celebrate:
        m_success = true;
        m_timer->start(kCelebrateMs);
        break;
    case StepKind::Finish:
        m_timer->stop();
        finishAndClose();
        break;
    }
    update();
}

void DemoRobotField::finishAndClose()
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

QRect DemoRobotField::cellRect(int i, int j) const
{
    return QRect(kXOff + j * kCell, kYOff + i * kCell, kCell, kCell);
}

QPointF DemoRobotField::cellCenter(int i, int j) const
{
    return QPointF(kXOff + (j + 0.5) * kCell, kYOff + (i + 0.5) * kCell);
}

QColor DemoRobotField::cellColor(int type) const
{
    if (type == ToFill)
        return kToFill;
    if (type == Filled)
        return kFilled;
    return kEmpty;
}

void DemoRobotField::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), kBg);
    p.setRenderHint(QPainter::Antialiasing, false);

    auto fillRectInc = [&](int x1, int y1, int x2, int y2, const QColor &c) {
        p.fillRect(QRect(QPoint(x1, y1), QPoint(x2, y2)), c);
    };

    for (int i = 0; i < m_rows; ++i) {
        for (int j = 0; j < m_cols; ++j) {
            const QRect r = cellRect(i, j);
            p.fillRect(r, cellColor(m_field[i][j].type));
        }
    }

    auto drawEdges = [&](bool wallsOnly) {
        for (int i = 0; i < m_rows; ++i) {
            for (int j = 0; j < m_cols; ++j) {
                const int x = kXOff + j * kCell;
                const int y = kYOff + i * kCell;
                const int walls = m_field[i][j].walls;
                auto edge = [&](bool blocked, int x1, int y1, int x2, int y2) {
                    if (blocked != wallsOnly)
                        return;
                    fillRectInc(x1, y1, x2, y2, blocked ? kWallColor : kGridColor);
                };
                const int wtL = (walls & WallLeft) ? kWallThick : kGridThick;
                const int wtT = (walls & WallTop) ? kWallThick : kGridThick;
                const int wtR = (walls & WallRight) ? kWallThick : kGridThick;
                const int wtB = (walls & WallBottom) ? kWallThick : kGridThick;
                edge(walls & WallLeft, x, y, x + wtL - 1, y + kCell - 1);
                edge(walls & WallTop, x, y, x + kCell - 1, y + wtT - 1);
                edge(walls & WallRight, x + kCell - wtR, y, x + kCell - 1, y + kCell - 1);
                edge(walls & WallBottom, x, y + kCell - wtB, x + kCell - 1, y + kCell - 1);
            }
        }
    };
    drawEdges(false);
    drawEdges(true);

    const int x0 = kXOff;
    const int y0 = kYOff;
    const int x1 = kXOff + m_cols * kCell;
    const int y1 = kYOff + m_rows * kCell;
    p.fillRect(x0 - kWallThick, y0 - kWallThick, (x1 - x0) + 2 * kWallThick, kWallThick, kWallColor);
    p.fillRect(x0 - kWallThick, y1, (x1 - x0) + 2 * kWallThick, kWallThick, kWallColor);
    p.fillRect(x0 - kWallThick, y0 - kWallThick, kWallThick, (y1 - y0) + 2 * kWallThick, kWallColor);
    p.fillRect(x1, y0 - kWallThick, kWallThick, (y1 - y0) + 2 * kWallThick, kWallColor);

    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(kParkColor);
    for (int i = 0; i < m_rows; ++i) {
        for (int j = 0; j < m_cols; ++j) {
            if (!m_field[i][j].parking)
                continue;
            const QPointF c = cellCenter(i, j);
            p.drawEllipse(c, qreal(kParkRadius), qreal(kParkRadius));
        }
    }
    p.setRenderHint(QPainter::Antialiasing, false);

    QFont lab(QStringLiteral("Helvetica"), 24);
    lab.setStyleHint(QFont::SansSerif);
    p.setFont(lab);
    p.setPen(Qt::black);
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < m_rows; ++i) {
        const QRect box(kXOff - kCell - kWallThick, kYOff + kCell * i, kCell, kCell);
        p.fillRect(box, kBg);
        p.drawText(box, Qt::AlignCenter, QString::number(i + 1));
    }
    for (int j = 0; j < m_cols; ++j) {
        const QRect box(kXOff + kCell * j, kYOff - kCell - kWallThick, kCell, kCell);
        p.fillRect(box, kBg);
        p.drawText(box, Qt::AlignCenter, QString::number(j + 1));
    }

    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(m_success ? kSuccessColor : kRobotColor, kRobotThick);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const QPointF rc = cellCenter(m_i, m_j);
    p.drawEllipse(rc, qreal(kRobotRadius), qreal(kRobotRadius));
}
