#pragma once

#include <QMainWindow>
#include <QPoint>
#include <QString>
#include <QVector>

class QEvent;
class QResizeEvent;
class QMenuBar;
class QToolBar;
class QTabWidget;
class QPlainTextEdit;
class QDockWidget;
class QTimer;
class QPropertyAnimation;
class QScrollArea;
class QWidget;
class QVBoxLayout;
class QLabel;
class QToolButton;
class DemoCursor;
class DemoRecorder;

class DemoAgentWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit DemoAgentWindow(QWidget *parent = nullptr);
    ~DemoAgentWindow() override;

    void beginAgentDemo();

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum AfterMove { AfterNone, AfterInput, AfterSend };

    struct EditStep {
        QString title;
        QString body;
        QString editorText;
        int startLine;
        int endLine;
    };

    struct Turn {
        QString prompt;
        QString waitStage;
        QString intro;
        QVector<EditStep> edits;
    };

    void buildChrome();
    void buildEditor();
    void buildAgentDock();
    void appendUserBubble(const QString &text);
    void startWaiting();
    void stopWaiting();
    void onWaitTick();
    void updateWaitDots();
    void startAssistantStream();
    void streamNext();
    void showNextEdit();
    void applyEditorEdit(const EditStep &step);
    void startNextTurn();
    void typeNext();
    void clickSend();
    void moveCursorTo(const QPoint &end, AfterMove after);
    void onCursorArrived();
    void finishDemo();
    void scrollChatToBottom();
    void insertBeforeWait(QWidget *w);
    QPoint inputHotPos() const;
    QPoint sendHotPos() const;

    QMenuBar *m_menuBar { nullptr };
    QWidget *m_titleBar { nullptr };
    QToolBar *m_toolBar { nullptr };
    QTabWidget *m_tabs { nullptr };
    QPlainTextEdit *m_editor { nullptr };
    QDockWidget *m_agentDock { nullptr };
    QScrollArea *m_chatScroll { nullptr };
    QWidget *m_chatInner { nullptr };
    QVBoxLayout *m_messagesLayout { nullptr };
    QLabel *m_emptyHint { nullptr };
    QWidget *m_waitWidget { nullptr };
    QWidget *m_waitDotsHost { nullptr };
    QLabel *m_waitDot[3] { nullptr, nullptr, nullptr };
    QLabel *m_waitStageLabel { nullptr };
    QTimer *m_waitTimer { nullptr };
    QWidget *m_assistantWrap { nullptr };
    QVBoxLayout *m_assistantLay { nullptr };
    QLabel *m_assistantLabel { nullptr };
    QPlainTextEdit *m_input { nullptr };
    QToolButton *m_sendBtn { nullptr };
    DemoCursor *m_cursor { nullptr };
    DemoRecorder *m_recorder { nullptr };
    QTimer *m_typeTimer { nullptr };
    QPropertyAnimation *m_cursorAnim { nullptr };
    QVector<Turn> m_turns;
    int m_turnIndex { 0 };
    int m_editIndex { 0 };
    int m_waitFrame { 0 };
    QString m_prompt;
    int m_typeIndex { 0 };
    int m_streamIndex { 0 };
    AfterMove m_afterMove { AfterNone };
    bool m_ignoreCursorFinish { false };
    QPoint m_dragOffset;
    bool m_dragging { false };
};
