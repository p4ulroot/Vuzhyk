#pragma once

#include <QColor>
#include <QPoint>
#include <QSize>
#include <QVector>
#include <QWidget>

class DemoRobotCanvas : public QWidget {
    Q_OBJECT
public:
    enum CellType { Empty = 0, ToBeFilled = 1, FilledInitial = 2 };
    enum WallSide { Left = 0, Right = 1, Top = 2, Bottom = 3 };

    explicit DemoRobotCanvas(QWidget *parent = nullptr);

    void setGridSize(int rows, int cols);
    void setCell(int i, int j, CellType type);
    void setStart(int i, int j);
    void setParking(int i, int j);
    void setWall(int i, int j, WallSide side, bool on);
    void clearMarkers();

    int rows() const { return m_rows; }
    int cols() const { return m_cols; }
    QPoint cellCenterLocal(int i, int j) const;
    QRect cellRectLocal(int i, int j) const;
    QPoint wallClickLocal(int i, int j, WallSide side) const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateCellSize();

    int m_rows { 15 };
    int m_cols { 15 };
    qreal m_cell { 32.0 };
    QVector<QVector<int>> m_cells;
    QVector<QVector<bool>> m_wallRight;
    QVector<QVector<bool>> m_wallBottom;
    bool m_hasStart { false };
    bool m_hasParking { false };
    QPoint m_start { -1, -1 };
    QPoint m_parking { -1, -1 };
};
