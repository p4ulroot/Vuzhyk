#pragma once

#include <QMainWindow>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QVector>

class QEvent;
class QResizeEvent;
class QMenuBar;
class QToolBar;
class QTabWidget;
class QPlainTextEdit;
class QDockWidget;
class QAction;
class QTimer;
class QPropertyAnimation;
class QListWidget;
class DemoCursor;
class DemoRecorder;

class DemoStepWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit DemoStepWindow(QWidget *parent = nullptr);
    ~DemoStepWindow() override;

    void beginStepDemo();

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum EventType { TypeChars, ShowHint, MoveHint, AcceptHint, Backspace, Pause };
    enum AfterMove { AfterNone, AfterDebug, AfterStep };

    struct ScriptEvent {
        EventType type;
        QString text;
        QStringList items;
        int index;
        int count;
    };

    struct DebugStep {
        int line;
        QString output;
        bool finished;
    };

    void buildChrome();
    void buildEditor();
    void buildOutputDock();
    void buildScript();
    void runNextEvent();
    void typeNext();
    void appendTyped(const QString &chunk);
    void showHint(const QStringList &items, int index);
    void hideHint();
    void positionHint();
    void highlightLine(int line);
    void moveCursorTo(const QPoint &end, AfterMove after);
    void onCursorArrived();
    void clickDebug();
    void clickStep();
    void applyDebugStep();
    void finishDemo();
    QPoint cursorHotFor(QWidget *widget) const;
    QPoint appearFromEditor() const;
    QWidget *actionButton(QAction *action) const;

    QMenuBar *m_menuBar { nullptr };
    QWidget *m_titleBar { nullptr };
    QToolBar *m_toolBar { nullptr };
    QAction *m_debugAction { nullptr };
    QAction *m_stepAction { nullptr };
    QTabWidget *m_tabs { nullptr };
    QPlainTextEdit *m_editor { nullptr };
    QDockWidget *m_outputDock { nullptr };
    QPlainTextEdit *m_output { nullptr };
    QListWidget *m_hint { nullptr };
    DemoCursor *m_cursor { nullptr };
    DemoRecorder *m_recorder { nullptr };
    QTimer *m_typeTimer { nullptr };
    QPropertyAnimation *m_cursorAnim { nullptr };
    QVector<ScriptEvent> m_events;
    QVector<DebugStep> m_debugSteps;
    QString m_pendingType;
    int m_eventIndex { 0 };
    int m_typeIndex { 0 };
    int m_debugIndex { 0 };
    AfterMove m_afterMove { AfterNone };
    bool m_ignoreCursorFinish { false };
    QPoint m_dragOffset;
    bool m_dragging { false };
};
