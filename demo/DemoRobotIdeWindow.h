#pragma once

#include <QMainWindow>
#include <QPoint>
#include <QVector>

class QEvent;
class QResizeEvent;
class QMenuBar;
class QTabWidget;
class QPropertyAnimation;
class QWidget;
class DemoRobotTask;
class DemoCursor;
class DemoRecorder;

class DemoRobotIdeWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit DemoRobotIdeWindow(QWidget *parent = nullptr);
    ~DemoRobotIdeWindow() override;

    void beginPatternDemo();

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum AfterMove {
        AfterNone,
        AfterSelectTeal,
        AfterBeginCircle,
        AfterBeginSquare,
        AfterSelectWall,
        AfterBeginWalls,
        AfterSelectStart,
        AfterPlaceStart,
        AfterSelectPark,
        AfterPlacePark
    };

    void buildChrome();
    void buildPatternCells();
    void moveCursorTo(const QPoint &end, AfterMove after);
    void onCursorArrived();
    void clickTool(int id);
    void paintNext();
    void wallNext();
    void finishDemo();
    QPoint cellHotPos(int i, int j) const;
    QPoint toolHotPos(int toolId) const;
    QPoint wallHotPos(int i, int j, int side) const;

    struct WallMark {
        int i { 0 };
        int j { 0 };
        int side { 0 };
    };

    QMenuBar *m_menuBar { nullptr };
    QWidget *m_titleBar { nullptr };
    QTabWidget *m_tabs { nullptr };
    DemoRobotTask *m_task { nullptr };
    DemoCursor *m_cursor { nullptr };
    DemoRecorder *m_recorder { nullptr };
    QPropertyAnimation *m_cursorAnim { nullptr };
    QVector<QPoint> m_circle;
    QVector<QPoint> m_square;
    QVector<WallMark> m_walls;
    const QVector<QPoint> *m_paintList { nullptr };
    int m_paintIndex { 0 };
    int m_wallIndex { 0 };
    AfterMove m_afterMove { AfterNone };
    bool m_ignoreCursorFinish { false };
    QPoint m_dragOffset;
    bool m_dragging { false };
};
