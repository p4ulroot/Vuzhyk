#pragma once

#include <QMainWindow>
#include <QPoint>
#include <QString>
#include <QElapsedTimer>

class QEvent;
class QResizeEvent;
class QMenuBar;
class QToolBar;
class QTabWidget;
class QPlainTextEdit;
class QPropertyAnimation;
class QWidget;
class QToolButton;
class QAction;
class QFrame;
class QLabel;
class DemoCursor;
class DemoRecorder;

class DemoPythonHighlighter;

class DemoThemeWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit DemoThemeWindow(QWidget *parent = nullptr);
    ~DemoThemeWindow() override;

    void beginThemeDemo();

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum AfterMove {
        AfterNone,
        AfterZoom,
        AfterOpenTools,
        AfterClickTheme,
        AfterClickTab
    };

    QPlainTextEdit *makeEditor();
    void buildChrome();
    void buildEditor();
    void buildFakeMenu();
    void fillSampleCode();
    void applyDarkTheme();
    void applyEditorTheme(bool dark);
    void styleOneEditor(QPlainTextEdit *editor, DemoPythonHighlighter *highlighter, bool dark);
    void startZoom();
    void applyEditorZoom(qreal size);
    qreal editorFontSize() const;
    void clickTools();
    void showFakeMenu();
    void clipFakeMenu(int visibleHeight);
    void startMenuOpenAnimation();
    void startMenuCloseAnimation();
    void clickTheme();
    void clickSecondTab();
    void animateTabSwitch(int fromIndex, int toIndex);
    void moveCursorTo(const QPoint &end, AfterMove after, int durationMs = 280);
    void onCursorArrived();
    void finishDemo();
    void applyDarkThemeAtDeadline();
    int msUntil(int targetMs) const;
    QPoint editorHotPos() const;
    QPoint toolsHotPos() const;
    QPoint themeItemHotPos() const;
    QPoint secondTabHotPos() const;
    void styleMenuItem(QLabel *item, bool selected);

    QMenuBar *m_menuBar { nullptr };
    QWidget *m_titleBar { nullptr };
    QToolBar *m_toolBar { nullptr };
    QToolButton *m_minBtn { nullptr };
    QToolButton *m_maxBtn { nullptr };
    QToolButton *m_closeBtn { nullptr };
    QString m_winIconColor { QStringLiteral("#333333") };
    QAction *m_toolsAction { nullptr };
    QTabWidget *m_tabs { nullptr };
    QPlainTextEdit *m_editor { nullptr };
    QPlainTextEdit *m_editor2 { nullptr };
    DemoPythonHighlighter *m_highlighter { nullptr };
    DemoPythonHighlighter *m_highlighter2 { nullptr };
    QFrame *m_fakeMenu { nullptr };
    QLabel *m_themeItem { nullptr };
    DemoCursor *m_cursor { nullptr };
    DemoRecorder *m_recorder { nullptr };
    QPropertyAnimation *m_cursorAnim { nullptr };
    QElapsedTimer m_demoClock;
    AfterMove m_afterMove { AfterNone };
    bool m_ignoreCursorFinish { false };
    QPoint m_dragOffset;
    bool m_dragging { false };
};
