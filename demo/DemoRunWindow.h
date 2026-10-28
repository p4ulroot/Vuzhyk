#pragma once

#include <QMainWindow>
#include <QPoint>
#include <QString>
#include <QStringList>

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
class DemoCursor;
class DemoRecorder;

class DemoRunWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit DemoRunWindow(QWidget *parent = nullptr);
    ~DemoRunWindow() override;

    void beginRunDemo();

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildChrome();
    void buildEditor();
    void buildOutputDock();
    void typeNext();
    void moveCursorToRun();
    void onCursorArrived();
    void clickRun();
    void startOutput();
    void emitNextOutputLine();
    void finishDemo();
    QPoint cursorHotFor(QWidget *widget) const;
    QPoint appearFromEditor() const;
    QWidget *runButtonWidget() const;
    void appendTyped(const QString &chunk);

    QMenuBar *m_menuBar { nullptr };
    QWidget *m_titleBar { nullptr };
    QToolBar *m_toolBar { nullptr };
    QAction *m_runAction { nullptr };
    QTabWidget *m_tabs { nullptr };
    QPlainTextEdit *m_editor { nullptr };
    QDockWidget *m_outputDock { nullptr };
    QPlainTextEdit *m_output { nullptr };
    DemoCursor *m_cursor { nullptr };
    DemoRecorder *m_recorder { nullptr };
    QTimer *m_typeTimer { nullptr };
    QPropertyAnimation *m_cursorAnim { nullptr };
    QString m_code;
    QStringList m_outputLines;
    int m_typeIndex { 0 };
    int m_outputLine { 0 };
    bool m_ignoreCursorFinish { false };
    QPoint m_dragOffset;
    bool m_dragging { false };
};
