#pragma once

#include <QWidget>
#include <QPointer>
#include <QProcess>
#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <QVector>
#include <functional>

class CodeEditor;
class TurtleCanvasHost;
class QEvent;
class QLabel;
class QPushButton;
class QScrollArea;
class QSplitter;

struct TurtleTaskDefinition {
    QString id;
    QString title;
    QString instruction;
    QString starterCode;
    QString samplePath;
    int sampleForward { 100 };
    QString checkId;
};

struct TurtleTaskChapter {
    QString title;
    QVector<TurtleTaskDefinition> tasks;
};

TurtleTaskDefinition turtleTaskForward100();
QVector<TurtleTaskChapter> turtleTaskCatalog();

class TurtleTaskWidget : public QWidget {
    Q_OBJECT
public:
    explicit TurtleTaskWidget(const TurtleTaskDefinition &task, QWidget *parent = nullptr);
    ~TurtleTaskWidget() override;

    CodeEditor *editor() const { return m_editor; }
    QString taskId() const { return QStringLiteral("turtle_workbook"); }
    QString tabTitle() const;

    void setTheme(const QString &theme);
    void setPythonGetter(const std::function<QString()> &getter);

    bool isRunning() const;
    void run(const QString &pythonExe, bool stepMode);
    void stop();
    void ensureSession();

signals:
    void processOutput(const QString &text, bool isError);

private slots:
    void onRunClicked();
    void onStepClicked();
    void onResetClicked();
    void onStdout();
    void onStderr();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onCanvasEvent(const QJsonObject &event);
    void onCatalogTaskClicked();

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void applyPanelTheme();
    void buildCatalog();
    void applyTask(const TurtleTaskDefinition &task, bool restartSession);
    void updateCatalogSelection();
    void updateCanvasHeight();
    TurtleTaskDefinition resolveInitialTask(const TurtleTaskDefinition &task) const;
    void requestStep();
    QString pythonExecutable() const;
    QString writeTempFile(const QString &name, const QByteArray &data) const;
    QString loadRunnerScript() const;
    void handleRunnerLine(const QString &line);
    void handleFvtPayload(const QJsonObject &obj);
    void cleanupTempDir();
    void sendToProcess(const QString &line);
    bool sessionAlive() const;
    void startSession(const QString &pythonExe = QString());
    void killSession();
    bool writeStudentScript();
    void sendRunCommand();
    void flushPending();
    void showStatus(const QString &text, const QString &kind);

    TurtleTaskDefinition m_task;
    QVector<TurtleTaskChapter> m_catalog;
    QString m_theme { QStringLiteral("light") };
    std::function<QString()> m_pythonGetter;

    CodeEditor *m_editor { nullptr };
    QSplitter *m_splitter { nullptr };
    QWidget *m_panel { nullptr };
    QLabel *m_instruction { nullptr };
    TurtleCanvasHost *m_canvas { nullptr };
    QPushButton *m_runBtn { nullptr };
    QPushButton *m_stepBtn { nullptr };
    QPushButton *m_resetBtn { nullptr };
    QWidget *m_controlRow { nullptr };
    QScrollArea *m_catalogScroll { nullptr };
    QWidget *m_catalogHost { nullptr };
    QVector<QPushButton *> m_taskButtons;

    QPointer<QProcess> m_process;
    QString m_stdoutBuf;
    QString m_tempDir;
    bool m_stepMode { false };
    bool m_waitingStep { false };
    bool m_executing { false };
    bool m_sessionReady { false };
    bool m_killing { false };
    bool m_pendingRun { false };
    bool m_pendingReset { false };
    QString m_statusKind { QStringLiteral("hint") };
};
