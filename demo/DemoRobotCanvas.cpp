#include "DemoRobotCanvas.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QResizeEvent>
#include <QPalette>

DemoRobotCanvas::DemoRobotCanvas(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(35, 35, 35));
    setPalette(pal);
    setAutoFillBackground(true);
    setStyleSheet(QStringLiteral("background-color: #232323; border: none;"));
    setGridSize(15, 15);
}

void DemoRobotCanvas::setGridSize(int rows, int cols)
{
    m_rows = qMax(1, rows);
    m_cols = qMax(1, cols);
    m_cells.fill(QVector<int>(m_cols, Empty), m_rows);
    m_wallRight.fill(QVector<bool>(m_cols, false), m_rows);
    m_wallBottom.fill(QVector<bool>(m_cols, false), m_rows);
    m_hasStart = false;
    m_hasParking = false;
    m_start = QPoint(-1, -1);
    m_parking = QPoint(-1, -1);
    updateCellSize();
    update();
}

void DemoRobotCanvas::setCell(int i, int j, CellType type)
{
    if (i < 0 || j < 0 || i >= m_rows || j >= m_cols)
        return;
    m_cells[i][j] = type;
    update();
}

void DemoRobotCanvas::setStart(int i, int j)
{
    if (i < 0 || j < 0 || i >= m_rows || j >= m_cols)
        return;
    m_hasStart = true;
    m_start = QPoint(i, j);
    update();
}

void DemoRobotCanvas::setParking(int i, int j)
{
    if (i < 0 || j < 0 || i >= m_rows || j >= m_cols)
        return;
    m_hasParking = true;
    m_parking = QPoint(i, j);
    update();
}

void DemoRobotCanvas::setWall(int i, int j, WallSide side, bool on)
{
    if (i < 0 || j < 0 || i >= m_rows || j >= m_cols)
        return;
    if (side == Right && j < m_cols) {
        m_wallRight[i][j] = on;
    } else if (side == Left && j > 0) {
        m_wallRight[i][j - 1] = on;
    } else if (side == Bottom && i < m_rows) {
        m_wallBottom[i][j] = on;
    } else if (side == Top && i > 0) {
        m_wallBottom[i - 1][j] = on;
    }
    update();
}

QPoint DemoRobotCanvas::wallClickLocal(int i, int j, WallSide side) const
{
    const QRect r = cellRectLocal(i, j);
    if (side == Left)
        return QPoint(r.left() + 1, r.center().y());
    if (side == Right)
        return QPoint(r.right() - 1, r.center().y());
    if (side == Top)
        return QPoint(r.center().x(), r.top() + 1);
    return QPoint(r.center().x(), r.bottom() - 1);
}

void DemoRobotCanvas::clearMarkers()
{
    m_hasStart = false;
    m_hasParking = false;
    update();
}

void DemoRobotCanvas::updateCellSize()
{
    const QSize avail = size();
    if (m_rows <= 0 || m_cols <= 0 || avail.width() < 4 || avail.height() < 4) {
        m_cell = 32.0;
        return;
    }
    m_cell = qMax(16.0, qMin((avail.width() - 2.0) / m_cols, (avail.height() - 2.0) / m_rows));
}

void DemoRobotCanvas::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateCellSize();
}

QRect DemoRobotCanvas::cellRectLocal(int i, int j) const
{
    return QRect(qRound(j * m_cell), qRound(i * m_cell), qRound(m_cell), qRound(m_cell));
}

QPoint DemoRobotCanvas::cellCenterLocal(int i, int j) const
{
    return cellRectLocal(i, j).center();
}

void DemoRobotCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(35, 35, 35));
    p.setRenderHint(QPainter::Antialiasing, false);
    updateCellSize();

    const qreal w = m_cols * m_cell;
    const qreal h = m_rows * m_cell;

    for (int i = 0; i < m_rows; ++i) {
        for (int j = 0; j < m_cols; ++j) {
            QColor fill(210, 210, 210);
            switch (m_cells[i][j]) {
            case ToBeFilled: fill = QColor(48, 186, 176); break;
            case FilledInitial: fill = QColor(255, 248, 209); break;
            default: break;
            }
            p.fillRect(QRectF(j * m_cell, i * m_cell, m_cell, m_cell), fill);
        }
    }

    QPen gridPen(QColor(165, 165, 165));
    gridPen.setWidthF(1.0);
    p.setPen(gridPen);
    for (int i = 0; i <= m_rows; ++i)
        p.drawLine(QPointF(0, i * m_cell), QPointF(w, i * m_cell));
    for (int j = 0; j <= m_cols; ++j)
        p.drawLine(QPointF(j * m_cell, 0), QPointF(j * m_cell, h));

    QPen wallPen(QColor(120, 120, 120));
    wallPen.setWidthF(3.0);
    p.setPen(wallPen);
    p.drawLine(QPointF(0, 0), QPointF(w, 0));
    p.drawLine(QPointF(0, h), QPointF(w, h));
    p.drawLine(QPointF(0, 0), QPointF(0, h));
    p.drawLine(QPointF(w, 0), QPointF(w, h));

    for (int i = 0; i < m_rows; ++i) {
        for (int j = 0; j < m_cols; ++j) {
            if (j < m_cols - 1 && m_wallRight[i][j])
                p.drawLine(QPointF((j + 1) * m_cell, i * m_cell),
                           QPointF((j + 1) * m_cell, (i + 1) * m_cell));
            if (i < m_rows - 1 && m_wallBottom[i][j])
                p.drawLine(QPointF(j * m_cell, (i + 1) * m_cell),
                           QPointF((j + 1) * m_cell, (i + 1) * m_cell));
        }
    }

    p.setRenderHint(QPainter::Antialiasing, true);
    if (m_hasStart) {
        const QRectF rc(m_start.y() * m_cell, m_start.x() * m_cell, m_cell, m_cell);
        QPen pen(Qt::blue);
        pen.setWidth(2);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(rc.adjusted(6, 6, -6, -6));
    }
    if (m_hasParking) {
        const QRectF rc(m_parking.y() * m_cell, m_parking.x() * m_cell, m_cell, m_cell);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black);
        p.drawEllipse(rc.adjusted(10, 10, -10, -10));
    }
}
