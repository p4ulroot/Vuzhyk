#pragma once

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPointer>
#include <QTextEdit>
#include <QTextBrowser>
#include <QTreeView>
#include <QFileSystemModel>
#include "HoverActivatedMenuBar.h"
#include <QDockWidget>
#include <QTabWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QMap>
#include <QTimer>
#include <functional>
#include <QLocalServer>
#include <QLocalSocket>
#include <QList>
#include <QProgressBar>

#include "extensions/ExtensionManager.h"
#include "tasks/TurtleTaskWidget.h"

class LibrarySyncManager;
class AgentPanel;

class CodeEditor;
class TitleBar;
class WindowFrameOverlay;
class SettingsWidget;
class HelpWidget;
class PyrobEditorWidget;
class SnakeGame;
class ConsoleWidget;
class QMenu;
class QStringListModel;
class QPropertyAnimation;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString &theme = QString(), QWidget *parent = nullptr);
    void openFileFromPath(const QString &path);

private slots:
    void newFile();
    void newPyrobSolution();
    void openFile();
    void saveFile();
    void saveFileAs();
    void saveAll();
    void runScript();
    void runScriptWithDebug();
    void runScriptInTerminal();
    void terminateRun();
    void continueDebug();
    void checkDebugSyncFile();
    void onOutputInputReturnPressed();
    void choosePython();
    void toggleRepl(bool enabled);
    void toggleTheme();
    void openProjectFolder();
    void startRepl();
    void stopRepl();
    void sendReplInput();

    void onProcessStarted();
    void onProcessStdout();
    void onProcessStderr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

    void onReplStarted();
    void onReplStdout();
    void onReplStderr();
    void onReplFinished(int exitCode, QProcess::ExitStatus status);
    void onOutputAnchorClicked(const QUrl &url);
    void onReplAnchorClicked(const QUrl &url);
    void onFsDoubleClicked(const QModelIndex &index);
    void onTabCloseRequested(int index);
    void onTabChanged(int index);
    void showTabContextMenu(const QPoint &pos);
    void closeTab(int index);
    void closeAllTabsExceptCurrent();
    void openSettings();
    void closeSettingsTab();
    void openHelp();
    void openPythonDocumentation();
    void openTextbook();
    void openTextbook7();
    void openPyrobEditor();
    void openSnakeGame();
    void toggleAgentPanel();
    void openTurtleTaskWorkbook();

private slots:
    void onNewConnection();
    void readSocketData();

private:
    void setupUi();
    void setupActions();
    void setupConnections();
    void setupExtensions();
    void refreshExtensionActions();
    void clearExtensionActions();
    void setupLocalServer();
    bool maybeSave();
    bool saveToPath(const QString &path);
    void loadFromPath(const QString &path);
    QString detectPythonExecutable() const;
    QString embeddedPythonPath() const;
    QString configuredPythonPath() const;
    void setCurrentFilePath(const QString &path);
    QString saveUnsavedToTemp(CodeEditor *editor);
    bool isTempPyrobSolutionPath(const QString &path) const;
    void appendOutput(const QString &text, bool isError);
    void toggleOutputMode();
    void appendReplOutput(const QString &text, bool isError);
    bool eventFilter(QObject *obj, QEvent *event) override;
    bool handleReplKeyPress(class QKeyEvent *ke);
    void keyPressEvent(QKeyEvent *event) override;
    void openFileAt(const QString &path, int line);
    void updateCompletionFromDocument();
    QStringList loadPythonCompletions();
    QStringList loadPyrobTasks();
    void setupEditorCompletions(CodeEditor *editor, QStringListModel *baseModel);
    void ensureReplDock();
    void ensureProjectDock();
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void updateRoundedCorners();
    void updateBorderStyle();
    void applySnapOnRelease();
    void applyTheme(const QString &theme);
    void saveWindowState();
    void restoreWindowState();
    CodeEditor *currentEditor() const;
    CodeEditor *getEditorFromTabWidget(int index) const;
    CodeEditor *getEditorFromWidget(QWidget *widget) const;
    TurtleTaskWidget *currentTurtleTask() const;
    TurtleTaskWidget *turtleTaskFromWidget(QWidget *widget) const;
    void openTurtleTask(const TurtleTaskDefinition &task);
    void updateTabText(CodeEditor *editor);
    int findTabByPath(const QString &path) const;
    void setupEditor(CodeEditor *editor);
    void updateWindowTitle();
    void applyFontSizeToAllEditors(int size);
    void applyShortcut(const QString &actionName, const QKeySequence &sequence);
    void refreshFileTypeActions();
    bool isHtmlFile(const QString &path) const;
    bool openEmbeddedBrowserForFile(const QString &filePath);
    bool openInSystemDefault(const QString &filePath);
    bool startExtensionRunProcess(const ExtensionManager::ExtensionAction &action, const QString &absoluteFilePath);
    void loadShortcutsFromSettings();
    void applyEditorShortcuts();
    QKeySequence shortcutFromSettings(const QString &name, const QKeySequence &defaultSequence) const;
    void animateTabOpening(QWidget *widget, int tabIndex);
    void animateTabSwitch(int fromIndex, int toIndex, std::function<void()> onFinished = {});
    void parseErrorAndHighlight(const QString &errorText);
    void setupLibrarySync();
    void updateSyncCountdownDisplay();
    void ensureAgentDock();
    void applyAgentDockWidth();
    void saveAgentDockWidth();
    int agentDockMinWidth() const;
    void updateAgentAvailability();
    bool isAgentFeatureAllowed() const;
    QString agentEditorContext() const;
    bool applyAgentResponseToEditor(const QString &assistantResponse, QString *statusMessage);
    
public:
    QString showFilePicker(const QString &filter, const QString &initialDir = QString(), bool isSave = false, bool isDirectory = false);
    QString pythonPath() const; // Возвращает путь к текущему интерпретатору Python

private:
    QTabWidget *m_tabWidget {nullptr};
    QMap<CodeEditor*, QString> m_editorToPath; // Связь редактора с путем к файлу
    int m_previousTabIndex { -1 }; // Предыдущий индекс вкладки для анимации
    bool m_isInitializing { true }; // Флаг инициализации программы
    SettingsWidget *m_settingsWidget { nullptr };
    int m_settingsTabIndex { -1 }; // Индекс вкладки настроек
    HelpWidget *m_helpWidget { nullptr };
    int m_helpTabIndex { -1 }; // Индекс вкладки справки
    PyrobEditorWidget *m_pyrobEditorWidget { nullptr };
    int m_pyrobEditorTabIndex { -1 }; // Индекс вкладки редактора pyrob
    SnakeGame *m_snakeGame { nullptr };
    QWidget *m_snakeGameContainer { nullptr };
    int m_snakeGameTabIndex { -1 }; // Индекс вкладки игры
    QTextBrowser *m_output {nullptr};
    QLineEdit *m_outputInput {nullptr}; // Поле ввода для input()
    ConsoleWidget *m_console {nullptr};
    QStackedWidget *m_outputStack {nullptr};
    QDockWidget *m_outputDock {nullptr};
    bool m_outputModeIsConsole {false}; // false = вывод, true = консоль
    bool m_scriptRunningInConsole {false}; // Флаг: скрипт запущен в консоли или через QProcess
    QTextBrowser *m_replOutput {nullptr};
    QTextEdit *m_replInput {nullptr};
    QPointer<QProcess> m_process;
    QPointer<QProcess> m_replProcess;
    QList<QProcess*> m_terminalProcesses; // Список процессов терминалов для завершения при закрытии IDE
    QString m_runningFilePath; // Путь к файлу, который выполняется
    QString m_stderrBuffer; // Буфер для накопления stderr
    QString m_stdoutBuffer; // Буфер для накопления stdout
    bool m_isDebugging { false }; // Флаг отладки
    int m_currentBreakpointLine { -1 }; // Текущая строка с точкой останова
    QSet<int> m_debugBreakpoints; // Точки останова для отладки
    QString m_debugSyncFile; // Файл для синхронизации отладки
    QTimer *m_debugTimer { nullptr }; // Таймер для проверки файла синхронизации
    bool m_hotkeyRegistered { false }; // Флаг регистрации глобального хоткея F9
    bool m_terminateHotkeyRegistered { false }; // Флаг регистрации глобального хоткея Shift+F5
    static const int HOTKEY_ID = 1; // ID для глобального хоткея F9
    static const int HOTKEY_TERMINATE_ID = 2; // ID для глобального хоткея Shift+F5
    QStringList m_replHistory;
    int m_replHistoryIndex { -1 };
    QFileSystemModel *m_fsModel { nullptr };
    QTreeView *m_fsView { nullptr };
    QDockWidget *m_replDock { nullptr };
    QDockWidget *m_projectDock { nullptr };
    HoverActivatedMenuBar *m_menuBar { nullptr };
    QMenu *m_toolsMenu { nullptr };
    TitleBar *m_titleBar { nullptr };
    WindowFrameOverlay *m_frameOverlay { nullptr };
    QToolBar *m_toolBar { nullptr };
    QList<QAction*> m_extensionActions;
    ExtensionManager *m_extensionManager { nullptr };
    LibrarySyncManager *m_librarySyncManager { nullptr };
    AgentPanel *m_agentPanel { nullptr };
    QDockWidget *m_agentDock { nullptr };
    QAction *m_actToggleAgent { nullptr };
    int m_agentDockWidth { 260 };
    bool m_applyingAgentDockWidth { false };
    QLabel *m_syncProgressLabel { nullptr };
    QProgressBar *m_syncProgressBar { nullptr };
    QLabel *m_syncCountdownLabel { nullptr };
    QTimer *m_syncCountdownTimer { nullptr };
    QProcess *m_embeddedBrowserProcess { nullptr };
    QString m_currentTheme;
    QTimer *m_completionUpdateTimer { nullptr }; // Таймер для debounce обновления автодополнения
    QList<QPropertyAnimation*> m_activeAnimations; // Список активных анимаций для очистки
    bool m_animationsEnabled { true }; // Флаг включения анимаций (можно отключить для экономии памяти)
    int m_tabSwitchCount { 0 }; // Счетчик переключений вкладок для периодической очистки кэшей
    QStringList m_cachedBaseCompletions; // Кэш базовых дополнений (один для всех файлов)
    QSet<QString> m_cachedBaseCompletionsSet; // QSet для быстрой проверки префиксов
    QStringListModel *m_sharedBaseCompletionsModel { nullptr }; // Общая модель для базовых дополнений
    QMap<CodeEditor*, QStringList> m_fileCompletionsCache; // Кэш слов из файлов для каждого редактора
    QMap<CodeEditor*, QString> m_fileContentHash; // Хеш содержимого файла для отслеживания изменений
    QMap<CodeEditor*, QStringListModel*> m_editorCompletionsModels; // Модели completer для каждого редактора
    QMap<CodeEditor*, QString> m_editorTaskMarker; // Маркер для отслеживания добавленных задач в документ
    QStringList m_cachedPyrobTasks; // Кэш задач pyrob (загружается один раз)
    QAction *m_actNew { nullptr };
    QAction *m_actOpen { nullptr };
    QAction *m_actSave { nullptr };
    QAction *m_actSaveAs { nullptr };
    QAction *m_actSaveAll { nullptr };
    QAction *m_actCut { nullptr };
    QAction *m_actCopy { nullptr };
    QAction *m_actPaste { nullptr };
    QAction *m_actUndo { nullptr };
    QAction *m_actRedo { nullptr };
    QAction *m_actRun { nullptr };
    QAction *m_actTerminate { nullptr };
    QAction *m_actDebug { nullptr };
    QAction *m_actDebugNext { nullptr };
    QAction *m_actRunInTerminal { nullptr };
    QAction *m_actPythonDocs { nullptr };
    QAction *m_actTextbook { nullptr };
    QAction *m_actTextbook7 { nullptr };
    
    // Single instance support
    QLocalServer *m_localServer { nullptr };
    
    // Aero Snap support
    bool m_isDragging { false };
    QPoint m_dragStartPos;
    QRect m_normalGeometry; // Сохраняем нормальную геометрию окна
    bool m_isMaximized { false };
    bool m_isSnapped { false };
    bool m_needRestoreFromHalfScreen { false }; // Флаг для восстановления размера из полэкрана в WM_MOVING
};


