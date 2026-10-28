#pragma once

#include <QMainWindow>
#include <QPoint>
#include <QString>
#include <QVector>
#include <QPointF>

class QEvent;
class QResizeEvent;
class QMenuBar;
class QToolBar;
class QTabWidget;
class QPlainTextEdit;
class QTimer;
class QPropertyAnimation;
class QWidget;
class QToolButton;
class DemoTurtleTask;
class DemoCursor;
class DemoRecorder;

class DemoIdeWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit DemoIdeWindow(QWidget *parent = nullptr);
    ~DemoIdeWindow() override;

    void beginHouseDemo(const QString &recordPrefix = QStringLiteral("DemoTurtleHouse"),
                        bool prefillFirstTask = false);
    void beginSimpleTasksDemo();
    void applyDarkTheme();
    DemoTurtleTask *task() const { return m_task; }

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct Scene {
        int taskNumber { 1 };
        QString instruction;
        QString prefill;
        QString script;
        QVector<QPointF> path;
    };
    enum CursorTarget {
        CursorNone,
        CursorRun,
        CursorTask
    };

    void buildChrome();
    void buildHouseScenes();
    void buildSimpleScenes();
    void prepareScene(int index);
    void typeNext();
    void moveCursorTo(QWidget *widget, CursorTarget target);
    void onCursorArrived();
    void clickRun();
    void clickTaskButton();
    void startDrawing();
    void onPlaybackFinished();
    void advanceDemo();
    QVector<QPointF> housePath() const;
    QVector<QPointF> pathFromSpec(const QString &spec, const QPointF &origin = QPointF()) const;
    QPoint cursorHotFor(QWidget *widget) const;
    QPoint appearFromEditor() const;

    QMenuBar *m_menuBar { nullptr };
    QWidget *m_titleBar { nullptr };
    QToolBar *m_toolBar { nullptr };
    QToolButton *m_minBtn { nullptr };
    QToolButton *m_maxBtn { nullptr };
    QToolButton *m_closeBtn { nullptr };
    QString m_winIconColor { QStringLiteral("#333333") };
    QTabWidget *m_tabs { nullptr };
    DemoTurtleTask *m_task { nullptr };
    DemoCursor *m_cursor { nullptr };
    DemoRecorder *m_recorder { nullptr };
    QTimer *m_typeTimer { nullptr };
    QPropertyAnimation *m_cursorAnim { nullptr };
    QVector<Scene> m_scenes;
    int m_sceneIndex { 0 };
    QString m_script;
    int m_typeIndex { 0 };
    CursorTarget m_cursorTarget { CursorNone };
    bool m_ignoreCursorFinish { false };
    QPoint m_dragOffset;
    bool m_dragging { false };
    int m_drawDurationMs { 0 };
    int m_afterOkMs { 1800 };
    int m_startDelayMs { 1100 };
    int m_afterTypeMs { 500 };
    int m_cursorMs { 900 };
    int m_clickMs { 140 };
    int m_hideCursorMs { 350 };
    int m_afterSelectMs { 800 };
    int m_typeChunk { 1 };
    int m_typeIntervalMs { 0 };
    int m_endHoldMs { 0 };
};
