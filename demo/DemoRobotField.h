#pragma once

#include <QVector>
#include <QWidget>
#include <QColor>
#include <QPointF>
#include <QRect>

class QTimer;
class DemoRecorder;

class DemoRobotField : public QWidget {
public:
    explicit DemoRobotField(QWidget *parent = nullptr);
    ~DemoRobotField() override;

    void beginRecording();
    void beginDemo();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    enum CellType { Empty = 0, ToFill = 1, Filled = 2 };
    enum {
        WallLeft = 1 << 0,
        WallRight = 1 << 1,
        WallTop = 1 << 2,
        WallBottom = 1 << 3
    };
    enum class StepKind { Move, Fill, Celebrate, Finish };

    struct Cell {
        int walls { 0 };
        int type { Empty };
        bool parking { false };
    };
    struct Step {
        StepKind kind { StepKind::Move };
        int i { 0 };
        int j { 0 };
    };

    void setFieldSize(int rows, int cols);
    void putWall(int i, int j, int flags);
    void setToFill(int i, int j);
    bool wallAbove() const;
    bool wallBeneath() const;
    bool wallLeft() const;
    bool wallRight() const;
    void simMove(int di, int dj);
    void simFill();
    void loadTask();
    void buildTaskTriangle();
    void solveTaskTriangle();
    void tick();
    void finishAndClose();
    QRect cellRect(int i, int j) const;
    QPointF cellCenter(int i, int j) const;
    QColor cellColor(int type) const;

    int m_rows { 0 };
    int m_cols { 0 };
    int m_i { 0 };
    int m_j { 0 };
    QVector<QVector<Cell>> m_field;
    QVector<Step> m_steps;
    int m_step { 0 };
    bool m_success { false };
    bool m_closing { false };
    QTimer *m_timer { nullptr };
    DemoRecorder *m_recorder { nullptr };
};
