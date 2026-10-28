#include "MainWindow.h"

#include "SettingsWidget.h"
#include "HelpWidget.h"
#include "agent/AgentPanel.h"
#include "agent/AgentEdit.h"
#include "sync/LibrarySyncManager.h"
#include "sync/LibrarySyncServerControl.h"
#include "CodeEditor.h"
#include "TitleBar.h"
#include "WindowFrameOverlay.h"
#include "AnimatedMenu.h"
#include "ConsoleWidget.h"
#include "pyrobeditor/PyrobEditorWidget.h"
#include "tasks/TurtleTaskWidget.h"
// SnakeGame.h включаем для корректного вызова деструктора при удалении
#include "sea/SnakeGame.h"
#include "extensions/ExtensionManager.h"
#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QCloseEvent>
#include <QDebug>
#include <QScreen>
#include <QMoveEvent>
#include <QDockWidget>
#include <QByteArray>
#include <QFile>
#include <QFileDialog>
#include <QDialog>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStatusBar>
#include <QProgressBar>
#include <QSignalBlocker>
#include <QTextStream>
#include <QToolBar>
#include <QDir>
#include <QRegularExpression>
#include <QLabel>
#include <QPushButton>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QTextEdit>
#include <QTabWidget>
#include <QTabBar>
#include <QFileInfo>
#include <QCompleter>
#include <QStringListModel>
#include <QAbstractItemView>
#include <QKeyEvent>
#include <QTextBrowser>
#include <QLineEdit>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QSet>
#include <QPainterPath>
#include <QPainter>
#include <QRegion>
#include <QProcess>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QVBoxLayout>
#include <QEasingCurve>
#include <QGraphicsEffect>
#include <QGraphicsProxyWidget>
#include <QEventLoop>
#include <QTimer>
#include <QElapsedTimer>
#include <QThread>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QIcon>
#include <QColor>
#include <QPalette>
#include <QSvgRenderer>
#include <QFile>
#include <QBuffer>
#include <QByteArray>
#include <QDesktopServices>
#ifdef _WIN32
#  include <windows.h>
#  include <shellapi.h>
#  include <shlobj.h>
#  include <objbase.h>
#  include <QToolButton>
#  include <vector>
#endif
#include "CodeEditor.h"
#include "TitleBar.h"
#include "WindowFrameOverlay.h"
#include <QSizePolicy>
#include <QCryptographicHash>
#include <QPixmapCache>

namespace {
constexpr auto SETTINGS_GROUP = "runtime";
constexpr auto SETTINGS_PYTHON_PATH = "pythonPath";

// Вспомогательная функция для перекрашивания SVG
QByteArray recolorSvg(const QString &resourcePath, const QString &newColor) {
    // Загружаем SVG из ресурсов
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    QByteArray svgData = file.readAll();
    file.close();
    
    // Заменяем цвета в SVG
    QString svgString = QString::fromUtf8(svgData);
    // Заменяем черный цвет на новый цвет
    svgString.replace("#000000", newColor, Qt::CaseInsensitive);
    svgString.replace("black", newColor, Qt::CaseInsensitive);
    svgString.replace("rgb(0,0,0)", newColor, Qt::CaseInsensitive);
    
    return svgString.toUtf8();
}

// Вспомогательная функция для создания иконки из SVG ресурса с перекрашиванием
QIcon createIconFromResource(const QString &resourcePath, const QString &color = "#000000", int size = 24) {
    QIcon icon;
    QByteArray svgData;
    
    // Если цвет не черный, перекрашиваем SVG
    if (color != "#000000" && color != "black") {
        svgData = recolorSvg(resourcePath, color);
    } else {
        // Загружаем оригинальный SVG
        QFile file(resourcePath);
        if (file.open(QIODevice::ReadOnly)) {
            svgData = file.readAll();
            file.close();
        }
    }
    
    if (svgData.isEmpty()) {
        return icon;
    }
    
    // Создаем QSvgRenderer из перекрашенного SVG
    QSvgRenderer renderer(svgData);
    if (renderer.isValid()) {
        // Создаем несколько размеров для лучшего качества
        for (int s : {16, 24, 32}) {
            QPixmap pixmap(s, s);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::Antialiasing);
            renderer.render(&painter);
            painter.end();
            icon.addPixmap(pixmap);
        }
    }
    
    return icon;
}
}

MainWindow::MainWindow(const QString &theme, QWidget *parent)
    : QMainWindow(parent) {
    // Определяем тему из аргумента или из настроек
    if (theme.isEmpty()) {
        QSettings settings;
        m_currentTheme = settings.value("theme", "light").toString();
    } else {
        m_currentTheme = theme;
        // Сохраняем тему в настройки
        QSettings settings;
        settings.setValue("theme", theme);
    }
    
    setupUi();
    setupActions();
    setupConnections();
    setupExtensions();
    updateRoundedCorners();
    updateBorderStyle();
    
    // Применяем тему после инициализации действий
    applyTheme(m_currentTheme);
    
    // Восстанавливаем состояние окна (включая положение тулбара)
    restoreWindowState();

    // После restoreState снова закрепляем правый угол за агентом:
    // нижняя панель не должна заходить под агента.
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
    
    // Инициализируем таймер для debounce обновления автодополнения
    m_completionUpdateTimer = new QTimer(this);
    m_completionUpdateTimer->setSingleShot(true);
    m_completionUpdateTimer->setInterval(500); // Обновление через 500мс после последнего изменения
    connect(m_completionUpdateTimer, &QTimer::timeout, this, &MainWindow::updateCompletionFromDocument);
    
    // Регистрируем глобальный хоткей Shift+F5 для завершения выполнения
#ifdef _WIN32
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd && !m_terminateHotkeyRegistered) {
        // MOD_SHIFT = 0x0004, VK_F5 = 0x74 (116)
        if (RegisterHotKey(hwnd, HOTKEY_TERMINATE_ID, 0x0004, 0x74)) {
            m_terminateHotkeyRegistered = true;
        }
    }
#endif
    
    // Создаем локальный сервер для единого экземпляра приложения
    setupLocalServer();
    setupLibrarySync();
}

void MainWindow::setupUi() {
    resize(1000, 700);

    // Отключаем нативный заголовок
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);

    m_menuBar = new HoverActivatedMenuBar(this);
    m_menuBar->setNativeMenuBar(false);
    m_titleBar = new TitleBar(this, m_menuBar, this);
    setMenuWidget(m_titleBar);

    // Создаем виджет вкладок
    m_tabWidget = new QTabWidget(this);
    // Загружаем настройку отображения кнопок закрытия
    QSettings settings;
    bool showCloseButton = settings.value("interface/showCloseButtonOnTabs", false).toBool();
    m_tabWidget->setTabsClosable(showCloseButton);
    m_tabWidget->setMovable(true);
    setCentralWidget(m_tabWidget);
    
    // Устанавливаем обработчик контекстного меню для вкладок
    m_tabWidget->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tabWidget->tabBar(), &QTabBar::customContextMenuRequested, 
            this, &MainWindow::showTabContextMenu);
    
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);
    
    // Устанавливаем фильтр событий для перехвата кликов по вкладкам
    m_tabWidget->tabBar()->installEventFilter(this);
    
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    
    // Инициализируем предыдущий индекс вкладки
    if (m_tabWidget->count() > 0) {
        m_previousTabIndex = m_tabWidget->currentIndex();
    }
    
    // Создаем первую вкладку с пустым файлом (без анимации)
    m_isInitializing = true;
    newFile();
    m_isInitializing = false;

    // Создаем dock widget для вывода/консоли
    m_outputDock = new QDockWidget(tr("Вывод"), this);
    m_outputDock->setObjectName("OutputDock");
    
    // Создаем виджет-переключатель для заголовка
    QWidget *titleWidget = new QWidget();
    QHBoxLayout *titleLayout = new QHBoxLayout(titleWidget);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(0);
    
    QLabel *titleLabel = new QLabel(tr("Вывод"));
    titleLabel->setStyleSheet("QLabel { padding: 4px; }");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    
    QPushButton *toggleButton = new QPushButton(tr("Консоль"));
    toggleButton->setFlat(true);
    toggleButton->setStyleSheet(
        "QPushButton { padding: 4px 8px; border: 1px solid transparent; }"
        "QPushButton:hover { background-color: rgba(255, 255, 255, 0.1); border: 1px solid rgba(255, 255, 255, 0.2); }"
        "QPushButton:pressed { background-color: rgba(255, 255, 255, 0.15); }"
    );
    connect(toggleButton, &QPushButton::clicked, this, &MainWindow::toggleOutputMode);
    titleLayout->addWidget(toggleButton);
    
    m_outputDock->setTitleBarWidget(titleWidget);
    
    // Создаем QStackedWidget для переключения между режимами
    m_outputStack = new QStackedWidget();
    
    // Режим вывода - создаем виджет с выводом и полем ввода
    QWidget *outputWidget = new QWidget();
    QVBoxLayout *outputLayout = new QVBoxLayout(outputWidget);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->setSpacing(0);
    
    m_output = new QTextBrowser();
    m_output->setOpenLinks(false);
    connect(m_output, &QTextBrowser::anchorClicked, this, &MainWindow::onOutputAnchorClicked);
    outputLayout->addWidget(m_output);
    
    // Поле ввода для команд input()
    m_outputInput = new QLineEdit();
    m_outputInput->setPlaceholderText(tr("Введите данные для input()..."));
    m_outputInput->setVisible(false); // Скрыто по умолчанию
    connect(m_outputInput, &QLineEdit::returnPressed, this, &MainWindow::onOutputInputReturnPressed);
    outputLayout->addWidget(m_outputInput);
    
    m_outputStack->addWidget(outputWidget);
    
    // Режим консоли - создаем только при первом переключении на вкладку консоли
    // Это экономит память, если консоль не используется
    m_console = nullptr;
    
    // Создаем заглушку для консоли, чтобы m_outputStack имел правильный размер
    QWidget *consolePlaceholder = new QWidget();
    m_outputStack->addWidget(consolePlaceholder);
    
    // Устанавливаем начальный режим (вывод)
    m_outputStack->setCurrentIndex(0);
    m_outputModeIsConsole = false;
    
    m_outputDock->setWidget(m_outputStack);
    addDockWidget(Qt::BottomDockWidgetArea, m_outputDock);

    // Правый dock (Агент) занимает угол снизу справа — на всю высоту до статус-бара;
    // нижний dock (Вывод/Консоль) тогда идёт только до панели агента.
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);

    // REPL и Проект создаются по требованию
    m_replDock = nullptr;
    m_projectDock = nullptr;

    statusBar()->showMessage(tr("Готов"));

    m_syncProgressLabel = new QLabel(this);
    m_syncProgressLabel->setVisible(false);
    m_syncProgressBar = new QProgressBar(this);
    m_syncProgressBar->setVisible(false);
    m_syncProgressBar->setMinimumWidth(160);
    m_syncProgressBar->setMaximumWidth(220);
    m_syncProgressBar->setTextVisible(true);
    statusBar()->addPermanentWidget(m_syncProgressLabel);
    statusBar()->addPermanentWidget(m_syncProgressBar);
    m_syncCountdownLabel = new QLabel(this);
    m_syncCountdownLabel->setVisible(false);
    statusBar()->addPermanentWidget(m_syncCountdownLabel);

    // Overlay border (on top of all children)
    m_frameOverlay = new WindowFrameOverlay(this);
    m_frameOverlay->setRadius(8);
    m_frameOverlay->setPenWidth(2);
    m_frameOverlay->setPenColor(QColor("#686868"));
    m_frameOverlay->setGeometry(rect());
    m_frameOverlay->raise();
}

void MainWindow::setupActions() {
    auto *fileMenu = new AnimatedMenu(tr("Файл"), this);
    connect(fileMenu, &QMenu::aboutToHide, this, [this]() { if (m_menuBar) m_menuBar->deactivate(); });
    m_menuBar->addMenu(fileMenu);
    m_actNew = fileMenu->addAction(tr("Новый"));
    fileMenu->addAction(tr("Новое решение pyrob"), this, &MainWindow::newPyrobSolution);
    m_actOpen = fileMenu->addAction(tr("Открыть..."));
    m_actSave = fileMenu->addAction(tr("Сохранить"));
    m_actSaveAll = fileMenu->addAction(tr("Сохранить все"));
    m_actSaveAs = fileMenu->addAction(tr("Сохранить как..."));
    fileMenu->addAction(tr("Открыть папку..."), this, &MainWindow::openProjectFolder);
    fileMenu->addSeparator();
    auto *actExit = fileMenu->addAction(tr("Выход"));

    auto *runMenu = new AnimatedMenu(tr("Запуск"), this);
    connect(runMenu, &QMenu::aboutToHide, this, [this]() { if (m_menuBar) m_menuBar->deactivate(); });
    m_menuBar->addMenu(runMenu);
    m_actRun = runMenu->addAction(tr("Запустить скрипт"));

    auto *toolsMenu = new AnimatedMenu(tr("Инструменты"), this);
    connect(toolsMenu, &QMenu::aboutToHide, this, [this]() { if (m_menuBar) m_menuBar->deactivate(); });
    m_menuBar->addMenu(toolsMenu);
    m_toolsMenu = toolsMenu;
    auto *actChoosePy = toolsMenu->addAction(tr("Выбрать Python..."));
    toolsMenu->addSeparator();
    auto *actToggleRepl = toolsMenu->addAction(tr("REPL"));
    actToggleRepl->setCheckable(true);
    toolsMenu->addSeparator();
    auto *actPyrobEditor = toolsMenu->addAction(tr("Редактор задач pyrob"));
    connect(actPyrobEditor, &QAction::triggered, this, &MainWindow::openPyrobEditor);
    toolsMenu->addSeparator();
    auto *actToggleTheme = toolsMenu->addAction(tr("Переключить тему"));
    actToggleTheme->setCheckable(true);
    actToggleTheme->setChecked(false);

    auto *tasksMenu = new AnimatedMenu(tr("Задачи"), this);
    connect(tasksMenu, &QMenu::aboutToHide, this, [this]() { if (m_menuBar) m_menuBar->deactivate(); });
    m_menuBar->addMenu(tasksMenu);
    auto *actTurtleWorkbook = tasksMenu->addAction(tr("Задачник черепаха"));

    auto *helpMenu = new AnimatedMenu(tr("Помощь"), this);
    connect(helpMenu, &QMenu::aboutToHide, this, [this]() { if (m_menuBar) m_menuBar->deactivate(); });
    m_menuBar->addMenu(helpMenu);
    auto *actHelp = helpMenu->addAction(tr("Справка"));
    m_actPythonDocs = helpMenu->addAction(tr("Документация Python"));
    m_actTextbook = helpMenu->addAction(tr("Учебник 6 класс"));
    m_actTextbook7 = helpMenu->addAction(tr("Учебник 7 класс"));
    helpMenu->addSeparator();
    auto *actAbout = helpMenu->addAction(tr("О программе"));

    m_actToggleAgent = new QAction(tr("Агент"), this);
    m_actToggleAgent->setCheckable(true);
    m_actToggleAgent->setChecked(false);
    m_menuBar->addAction(m_actToggleAgent);
    connect(m_actToggleAgent, &QAction::triggered, this, &MainWindow::toggleAgentPanel);

    // Toolbar
    m_toolBar = addToolBar(tr("Основное"));
    m_toolBar->setObjectName("MainToolBar");
    m_toolBar->setMovable(true); // Разрешаем перетаскивание тулбара
    // Разрешаем размещение только сверху, слева и справа (запрещаем снизу)
    m_toolBar->setAllowedAreas(Qt::TopToolBarArea | Qt::LeftToolBarArea | Qt::RightToolBarArea);
    // Иконки из ресурсов - используем light-theme, но перекрашиваем в зависимости от темы
    QString iconBasePath = ":/icons/icons/light-theme/";
    QString iconColor = (m_currentTheme == "dark") ? "#e0e0e0" : "#000000";
    
    m_actNew->setIcon(createIconFromResource(iconBasePath + "file-new.svg", iconColor));
    m_actOpen->setIcon(createIconFromResource(iconBasePath + "file-open.svg", iconColor));
    m_actSave->setIcon(createIconFromResource(iconBasePath + "file-save.svg", iconColor));
    m_actSaveAll->setIcon(createIconFromResource(iconBasePath + "file-save-all.svg", iconColor));

    m_actCut = new QAction(createIconFromResource(iconBasePath + "cut.svg", iconColor), tr("Вырезать"), this);
    m_actCopy = new QAction(createIconFromResource(iconBasePath + "copy.svg", iconColor), tr("Копировать"), this);
    m_actPaste = new QAction(createIconFromResource(iconBasePath + "paste-clipboard.svg", iconColor), tr("Вставить"), this);
    m_actUndo = new QAction(createIconFromResource(iconBasePath + "undo.svg", iconColor), tr("Отменить"), this);
    m_actRedo = new QAction(createIconFromResource(iconBasePath + "redo.svg", iconColor), tr("Восстановить"), this);

    m_actRun->setIcon(createIconFromResource(iconBasePath + "play.svg", iconColor));
    m_actTerminate = new QAction(createIconFromResource(iconBasePath + "stop.svg", iconColor), tr("Завершить"), this);

    m_toolBar->addAction(m_actNew);
    m_toolBar->addAction(m_actOpen);
    m_toolBar->addAction(m_actSave);
    m_toolBar->addAction(m_actSaveAll);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_actCut);
    m_toolBar->addAction(m_actCopy);
    m_toolBar->addAction(m_actPaste);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_actUndo);
    m_toolBar->addAction(m_actRedo);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_actRun);
    m_toolBar->addAction(m_actTerminate);
    m_actDebug = new QAction(createIconFromResource(iconBasePath + "bug.svg", iconColor), tr("Запуск с отладкой"), this);
    m_toolBar->addAction(m_actDebug);
    m_actDebugNext = new QAction(createIconFromResource(iconBasePath + "next-bug.svg", iconColor), tr("Следующий шаг отладки"), this);
    m_toolBar->addAction(m_actDebugNext);
    m_actRunInTerminal = new QAction(createIconFromResource(iconBasePath + "run-in-terminal.svg", iconColor), tr("Запуск в системном браузере"), this);
    m_toolBar->addAction(m_actRunInTerminal);

    // Shortcuts
    // Горячие клавиши будут загружены из настроек в loadShortcutsFromSettings()
    // Дефолтные значения устанавливаются там же
    // Загружаем горячие клавиши из настроек или используем дефолтные
    // (вызывается после создания всех действий)

    // Connections
    connect(m_actNew, &QAction::triggered, this, &MainWindow::newFile);
    connect(m_actOpen, &QAction::triggered, this, &MainWindow::openFile);
    connect(m_actSave, &QAction::triggered, this, &MainWindow::saveFile);
    connect(m_actSaveAll, &QAction::triggered, this, &MainWindow::saveAll);
    connect(m_actSaveAs, &QAction::triggered, this, &MainWindow::saveFileAs);
    connect(m_actRun, &QAction::triggered, this, &MainWindow::runScript);
    connect(m_actTerminate, &QAction::triggered, this, &MainWindow::terminateRun);
    connect(m_actDebug, &QAction::triggered, this, &MainWindow::runScriptWithDebug);
    connect(m_actDebugNext, &QAction::triggered, this, &MainWindow::continueDebug);
    connect(m_actRunInTerminal, &QAction::triggered, this, &MainWindow::runScriptInTerminal);
    connect(actChoosePy, &QAction::triggered, this, &MainWindow::choosePython);
    connect(actToggleRepl, &QAction::toggled, this, &MainWindow::toggleRepl);
    connect(actToggleTheme, &QAction::triggered, this, &MainWindow::toggleTheme);
    connect(actHelp, &QAction::triggered, this, &MainWindow::openHelp);
    connect(m_actPythonDocs, &QAction::triggered, this, &MainWindow::openPythonDocumentation);
    connect(m_actTextbook, &QAction::triggered, this, &MainWindow::openTextbook);
    connect(m_actTextbook7, &QAction::triggered, this, &MainWindow::openTextbook7);
    connect(actAbout, &QAction::triggered, this, [this]() {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("О программе Vuzhyk"));
        dialog.setModal(true);

        auto *dialogLayout = new QVBoxLayout(&dialog);
        auto *contentLayout = new QHBoxLayout();
        contentLayout->setSpacing(20);

        auto *iconLabel = new QLabel(&dialog);
        iconLabel->setPixmap(QApplication::windowIcon().pixmap(96, 96));
        iconLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        iconLabel->setMinimumWidth(112);
        contentLayout->addWidget(iconLabel);

        auto *infoLayout = new QVBoxLayout();
        auto *nameLabel = new QLabel(QStringLiteral("<b>Vuzhyk</b>"), &dialog);
        nameLabel->setTextFormat(Qt::RichText);
        infoLayout->addWidget(nameLabel);

        auto *authorLabel = new QLabel(tr("Автор: Корнеев Павел"), &dialog);
        authorLabel->setToolTip(tr("В IDE есть пасхалка — можете попробовать её найти."));
        infoLayout->addWidget(authorLabel);

        auto addLink = [&dialog, infoLayout](const QString &text, const QString &url) {
            auto *linkLabel = new QLabel(
                QStringLiteral("<a href=\"%1\">%2</a>").arg(url, text), &dialog);
            linkLabel->setTextFormat(Qt::RichText);
            linkLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
            linkLabel->setOpenExternalLinks(true);
            infoLayout->addWidget(linkLabel);
        };
        addLink(tr("GitHub проекта"), QStringLiteral("https://github.com/p4ulroot/Vuzhyk"));
        addLink(tr("Сайт проекта"), QStringLiteral("https://vuzhyk.by/"));
        addLink(tr("Лицензия: GNU General Public License v3.0 (GPL-3.0)"),
                QStringLiteral("https://www.gnu.org/licenses/gpl-3.0.html"));

        contentLayout->addLayout(infoLayout, 1);
        dialogLayout->addLayout(contentLayout);

        auto *closeButton = new QPushButton(tr("Закрыть"), &dialog);
        connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
        auto *buttonLayout = new QHBoxLayout();
        buttonLayout->addStretch();
        buttonLayout->addWidget(closeButton);
        dialogLayout->addLayout(buttonLayout);

        dialog.setMinimumWidth(460);
        dialog.exec();
    });
    connect(actTurtleWorkbook, &QAction::triggered, this, &MainWindow::openTurtleTaskWorkbook);
    connect(actExit, &QAction::triggered, qApp, &QApplication::quit);
    // REPL ввод появится при первом открытии

    // Редакторские действия
    // Подключаем действия к текущему редактору (будет обновляться при смене вкладки)
    // Используем слоты, которые будут находить текущий редактор
    connect(m_actCut, &QAction::triggered, this, [this]() {
        CodeEditor *editor = currentEditor();
        if (editor) editor->cut();
    });
    connect(m_actCopy, &QAction::triggered, this, [this]() {
        CodeEditor *editor = currentEditor();
        if (editor) editor->copy();
    });
    connect(m_actPaste, &QAction::triggered, this, [this]() {
        CodeEditor *editor = currentEditor();
        if (editor) editor->paste();
    });
    connect(m_actUndo, &QAction::triggered, this, [this]() {
        CodeEditor *editor = currentEditor();
        if (editor) editor->undo();
    });
    connect(m_actRedo, &QAction::triggered, this, [this]() {
        CodeEditor *editor = currentEditor();
        if (editor) editor->redo();
    });
    
    // Загружаем горячие клавиши из настроек после создания всех действий
    loadShortcutsFromSettings();
}

void MainWindow::setupExtensions()
{
    m_extensionManager = new ExtensionManager(this);
    connect(m_extensionManager, &ExtensionManager::extensionsChanged,
            this, &MainWindow::refreshExtensionActions);
    m_extensionManager->loadExtensions();
}

void MainWindow::clearExtensionActions()
{
    for (QAction *action : m_extensionActions) {
        if (!action) {
            continue;
        }
        if (m_toolsMenu) {
            m_toolsMenu->removeAction(action);
        }
        if (m_toolBar) {
            m_toolBar->removeAction(action);
        }
        action->deleteLater();
    }
    m_extensionActions.clear();
}

void MainWindow::refreshExtensionActions()
{
    if (!m_extensionManager) {
        // Если менеджер расширений не инициализирован, скрываем пункт меню учебника
        return;
    }

    clearExtensionActions();
    for (const ExtensionManager::ExtensionInfo &extension : m_extensionManager->extensions()) {
        if (!extension.enabled) {
            continue;
        }
        for (const ExtensionManager::ExtensionAction &extensionAction : extension.actions) {
            ExtensionManager::ExtensionAction action = extensionAction;
            QAction *menuAction = new QAction(this);
            menuAction->setText(action.label);
            if (!action.iconPath.isEmpty()) {
                menuAction->setIcon(QIcon(action.iconPath));
            }
            if (!action.description.isEmpty()) {
                menuAction->setToolTip(action.description);
            }

            connect(menuAction, &QAction::triggered, this, [this, action]() {
                if (m_extensionManager) {
                    m_extensionManager->executeAction(action);
                }
            });

            const QString location = action.location.toLower().trimmed();
            bool inserted = false;
            if (location == "toolbar" && m_toolBar) {
                if (action.separatorBefore) {
                    m_toolBar->addSeparator();
                }
                m_toolBar->addAction(menuAction);
                inserted = true;
            }

            if (!inserted && m_toolsMenu) {
                if (action.separatorBefore) {
                    m_toolsMenu->addSeparator();
                }
                m_toolsMenu->addAction(menuAction);
                inserted = true;
            }

            if (inserted) {
                m_extensionActions.append(menuAction);
            } else {
                delete menuAction;
            }
        }
    }
}

void MainWindow::loadShortcutsFromSettings() {
    QSettings settings;
    settings.beginGroup("shortcuts");
    
    // Дефолтные горячие клавиши
    QMap<QString, QKeySequence> defaultShortcuts = {
        {"new", QKeySequence::New},
        {"open", QKeySequence::Open},
        {"save", QKeySequence::Save},
        {"saveAs", QKeySequence::SaveAs},
        {"saveAll", QKeySequence(QStringLiteral("Ctrl+Shift+S"))},
        {"run", QKeySequence(Qt::Key_F5)},
        {"runInTerminal", QKeySequence(Qt::Key_F6)},
        {"debug", QKeySequence(Qt::Key_F8)},
        {"commentLines", QKeySequence(QStringLiteral("Alt+3"))},
        {"uncommentLines", QKeySequence(QStringLiteral("Alt+Ctrl+3"))},
    };
    
    // Применяем горячие клавиши
    if (m_actNew) {
        QString seqStr = settings.value("new", defaultShortcuts.value("new").toString()).toString();
        m_actNew->setShortcut(QKeySequence(seqStr));
    }
    if (m_actOpen) {
        QString seqStr = settings.value("open", defaultShortcuts.value("open").toString()).toString();
        m_actOpen->setShortcut(QKeySequence(seqStr));
    }
    if (m_actSave) {
        QString seqStr = settings.value("save", defaultShortcuts.value("save").toString()).toString();
        m_actSave->setShortcut(QKeySequence(seqStr));
    }
    if (m_actSaveAs) {
        QString seqStr = settings.value("saveAs", defaultShortcuts.value("saveAs").toString()).toString();
        m_actSaveAs->setShortcut(QKeySequence(seqStr));
    }
    if (m_actSaveAll) {
        QString seqStr = settings.value("saveAll", defaultShortcuts.value("saveAll").toString()).toString();
        m_actSaveAll->setShortcut(QKeySequence(seqStr));
    }
    if (m_actRun) {
        QString seqStr = settings.value("run", defaultShortcuts.value("run").toString()).toString();
        m_actRun->setShortcut(QKeySequence(seqStr));
    }
    if (m_actRunInTerminal) {
        QString seqStr = settings.value("runInTerminal", defaultShortcuts.value("runInTerminal").toString()).toString();
        m_actRunInTerminal->setShortcut(QKeySequence(seqStr));
    }
    if (m_actDebug) {
        QString seqStr = settings.value("debug", defaultShortcuts.value("debug").toString()).toString();
        m_actDebug->setShortcut(QKeySequence(seqStr));
    }
    
    settings.endGroup();
    applyEditorShortcuts();
}

QKeySequence MainWindow::shortcutFromSettings(const QString &name, const QKeySequence &defaultSequence) const
{
    QSettings settings;
    settings.beginGroup("shortcuts");
    const QString seqStr = settings.value(name, defaultSequence.toString()).toString();
    settings.endGroup();
    return QKeySequence(seqStr);
}

void MainWindow::applyEditorShortcuts()
{
    const QKeySequence commentSeq = shortcutFromSettings(
        QStringLiteral("commentLines"), QKeySequence(QStringLiteral("Alt+3")));
    const QKeySequence uncommentSeq = shortcutFromSettings(
        QStringLiteral("uncommentLines"), QKeySequence(QStringLiteral("Alt+Ctrl+3")));

    if (!m_tabWidget) {
        return;
    }

    for (int i = 0; i < m_tabWidget->count(); ++i) {
        CodeEditor *editor = getEditorFromTabWidget(i);
        if (!editor) {
            continue;
        }
        editor->setCommentShortcut(commentSeq);
        editor->setUncommentShortcut(uncommentSeq);
    }
}

void MainWindow::applyShortcut(const QString &actionName, const QKeySequence &sequence) {
    if (actionName == "new" && m_actNew) {
        m_actNew->setShortcut(sequence);
    } else if (actionName == "open" && m_actOpen) {
        m_actOpen->setShortcut(sequence);
    } else if (actionName == "save" && m_actSave) {
        m_actSave->setShortcut(sequence);
    } else if (actionName == "saveAs" && m_actSaveAs) {
        m_actSaveAs->setShortcut(sequence);
    } else if (actionName == "saveAll" && m_actSaveAll) {
        m_actSaveAll->setShortcut(sequence);
    } else if (actionName == "run" && m_actRun) {
        m_actRun->setShortcut(sequence);
    } else if (actionName == "runInTerminal" && m_actRunInTerminal) {
        m_actRunInTerminal->setShortcut(sequence);
    } else if (actionName == "debug" && m_actDebug) {
        m_actDebug->setShortcut(sequence);
    } else if (actionName == "commentLines" || actionName == "uncommentLines") {
        applyEditorShortcuts();
    }
}

#ifdef _WIN32
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result) {
    Q_UNUSED(eventType);
    MSG *msg = static_cast<MSG*>(message);
    
    if (msg->message == WM_NCHITTEST) {
        const int border = 6;
        const QPoint globalPos = QCursor::pos();
        const QPoint pos = mapFromGlobal(globalPos);
        const QRect r = rect();
        const bool left   = pos.x() <= border;
        const bool right  = pos.x() >= r.width() - border;
        const bool top    = pos.y() <= border;
        const bool bottom = pos.y() >= r.height() - border;

        if (top && left)   { *result = HTTOPLEFT;   return true; }
        if (top && right)  { *result = HTTOPRIGHT;  return true; }
        if (bottom && left){ *result = HTBOTTOMLEFT;return true; }
        if (bottom && right){*result = HTBOTTOMRIGHT;return true; }
        if (left)          { *result = HTLEFT;      return true; }
        if (right)         { *result = HTRIGHT;     return true; }
        if (top)           { *result = HTTOP;       return true; }
        if (bottom)        { *result = HTBOTTOM;    return true; }

        // Область заголовка — позволяем системное перетаскивание (HTCAPTION),
        // но исключаем меню и его дочерние виджеты, чтобы клики работали
        if (m_titleBar) {
            const QPoint pTitle = m_titleBar->mapFromGlobal(globalPos);
            if (m_titleBar->rect().contains(pTitle)) {
                QWidget *child = m_titleBar->childAt(pTitle);
                // Если под курсором меню, разрешаем перетаскивание ТОЛЬКО в пустых местах между пунктами
                if (child == m_menuBar || (child && child->parentWidget() == m_menuBar)) {
                    const QPoint pMenu = m_menuBar->mapFromGlobal(globalPos);
                    QAction *act = m_menuBar->actionAt(pMenu);
                    if (!act) { *result = HTCAPTION; return true; }
                } else if (qobject_cast<QLabel*>(child) || qobject_cast<QPushButton*>(child)) {
                    // QLabel или QPushButton (иконка) - не обрабатываем как перетаскивание, чтобы клики работали
                    return false;
                } else if (!qobject_cast<QToolButton*>(child)) {
                    *result = HTCAPTION;
                    return true;
                }
            }
        }
    }
    else if (msg->message == WM_ENTERSIZEMOVE) {
        // Когда начинается перетаскивание или изменение размера
        // Если окно максимизировано или закреплено, восстанавливаем его
        QRect currentGeometry = geometry();
        QScreen *screen = QApplication::screenAt(currentGeometry.center());
        if (!screen) {
            screen = QApplication::primaryScreen();
        }
        if (!screen) {
            return QMainWindow::nativeEvent(eventType, message, result);
        }
        
        QRect screenGeometry = screen->availableGeometry();
        bool isFullScreenSnapped = false;
        bool isHalfScreenSnapped = false;
        
        if (m_isSnapped) {
            isFullScreenSnapped = (currentGeometry == screenGeometry);
            // Проверяем, закреплено ли окно на пол экрана (слева или справа)
            if (!isFullScreenSnapped) {
                int halfWidth = screenGeometry.width() / 2;
                // Используем небольшую погрешность для проверки (на случай округления)
                const int tolerance = 2;
                bool isLeftHalf = (qAbs(currentGeometry.left() - screenGeometry.left()) <= tolerance && 
                                   qAbs(currentGeometry.width() - halfWidth) <= tolerance);
                bool isRightHalf = (qAbs(currentGeometry.left() - (screenGeometry.left() + halfWidth)) <= tolerance && 
                                    qAbs(currentGeometry.width() - halfWidth) <= tolerance);
                isHalfScreenSnapped = (isLeftHalf || isRightHalf);
            }
        }
        
        if (m_isMaximized || isFullScreenSnapped) {
            // Если геометрия не сохранена, пытаемся получить её из normalGeometry()
            if (m_normalGeometry.isEmpty() || !m_normalGeometry.isValid()) {
                QRect normalGeo = normalGeometry();
                if (normalGeo.isValid() && !normalGeo.isEmpty() && normalGeo != screenGeometry) {
                    m_normalGeometry = normalGeo;
                }
            }
            
            if (m_normalGeometry.isValid() && !m_normalGeometry.isEmpty()) {
                // Восстанавливаем нормальную геометрию (и позицию, и размер)
                HWND hwnd = reinterpret_cast<HWND>(winId());
                if (hwnd) {
                    if (m_isMaximized) {
                        ShowWindow(hwnd, SW_RESTORE);
                    }
                    SetWindowPos(hwnd, NULL, 
                                m_normalGeometry.x(), m_normalGeometry.y(),
                                m_normalGeometry.width(), m_normalGeometry.height(),
                                SWP_NOZORDER | SWP_NOACTIVATE);
                } else {
                    if (m_isMaximized) {
                        setWindowState(windowState() & ~Qt::WindowMaximized);
                    }
                    setGeometry(m_normalGeometry);
                }
                m_isMaximized = false;
                m_isSnapped = false;
            } else {
                HWND hwnd = reinterpret_cast<HWND>(winId());
                if (hwnd && m_isMaximized) {
                    ShowWindow(hwnd, SW_RESTORE);
                    m_isMaximized = false;
                } else if (m_isMaximized) {
                    setWindowState(windowState() & ~Qt::WindowMaximized);
                    m_isMaximized = false;
                }
                m_isSnapped = false;
            }
        }
        else if (isHalfScreenSnapped) {
            // Окно закреплено на пол экрана - устанавливаем флаг для восстановления размера в WM_MOVING
            // Если геометрия не сохранена, пытаемся получить её из normalGeometry()
            if (m_normalGeometry.isEmpty() || !m_normalGeometry.isValid()) {
                QRect normalGeo = normalGeometry();
                if (normalGeo.isValid() && !normalGeo.isEmpty() && normalGeo != screenGeometry) {
                    // Проверяем, что это не половина экрана
                    int halfWidth = screenGeometry.width() / 2;
                    const int tolerance = 2;
                    bool isLeftHalf = (qAbs(normalGeo.left() - screenGeometry.left()) <= tolerance && 
                                       qAbs(normalGeo.width() - halfWidth) <= tolerance);
                    bool isRightHalf = (qAbs(normalGeo.left() - (screenGeometry.left() + halfWidth)) <= tolerance && 
                                        qAbs(normalGeo.width() - halfWidth) <= tolerance);
                    if (!isLeftHalf && !isRightHalf) {
                        m_normalGeometry = normalGeo;
                    }
                }
            }
            
            // Устанавливаем флаг для восстановления размера в WM_MOVING
            if (m_normalGeometry.isValid() && !m_normalGeometry.isEmpty()) {
                m_needRestoreFromHalfScreen = true;
                m_isSnapped = false;
            } else {
                // Если геометрия не сохранена, просто снимаем snap
                m_isSnapped = false;
    m_needRestoreFromHalfScreen = false;
}
        }
    }
    else if (msg->message == WM_MOVING) {
        // Обрабатываем перетаскивание окна
        // Если нужно восстановить размер из полэкрана, делаем это в первом WM_MOVING
        if (m_needRestoreFromHalfScreen && m_normalGeometry.isValid() && !m_normalGeometry.isEmpty()) {
            RECT *rect = reinterpret_cast<RECT*>(msg->lParam);
            if (rect) {
                // Восстанавливаем размер, сохраняя текущую позицию
                int width = m_normalGeometry.width();
                int height = m_normalGeometry.height();
                rect->right = rect->left + width;
                rect->bottom = rect->top + height;
                // Сбрасываем флаг после первого восстановления
                m_needRestoreFromHalfScreen = false;
                *result = TRUE;
                return true;
            }
        }
    }
    else if (msg->message == WM_EXITSIZEMOVE) {
        // Применяем Aero Snap когда окно заканчивает перемещение
        // Это срабатывает после отпускания кнопки мыши при перетаскивании
        // Сбрасываем флаг на всякий случай
        m_needRestoreFromHalfScreen = false;
        applySnapOnRelease();
    }
    else if (msg->message == WM_HOTKEY) {
        // Обработка глобального хоткея F9 для продолжения отладки
        if (msg->wParam == HOTKEY_ID && m_isDebugging) {
            continueDebug();
            *result = 0;
            return true;
        }
        // Обработка глобального хоткея Shift+F5 для завершения выполнения
        else if (msg->wParam == HOTKEY_TERMINATE_ID) {
            terminateRun();
            *result = 0;
            return true;
        }
    }
    else if (msg->message == WM_SYSCOMMAND) {
        // Отслеживаем максимизацию/восстановление окна
        if (msg->wParam == SC_MAXIMIZE) {
            // Сохраняем нормальную геометрию перед максимизацией
            // ВСЕГДА сохраняем, если окно еще не максимизировано
            if (!m_isMaximized) {
                QRect currentGeo = geometry();
                QScreen *screen = QApplication::screenAt(currentGeo.center());
                if (!screen) {
                    screen = QApplication::primaryScreen();
                }
                if (screen) {
                    QRect screenGeo = screen->availableGeometry();
                    // Сохраняем только если текущая геометрия не равна геометрии экрана
                    if (currentGeo != screenGeo) {
                        m_normalGeometry = currentGeo;
                    }
                }
            }
            m_isMaximized = true;
            m_isSnapped = false;
        } else if (msg->wParam == SC_RESTORE) {
            m_isMaximized = false;
            m_isSnapped = false;
        }
    }
    
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    updateRoundedCorners();
    updateBorderStyle();
}

void MainWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        updateRoundedCorners();
        updateBorderStyle();
        // Отслеживаем максимизацию
        bool wasMaximized = m_isMaximized;
        m_isMaximized = isMaximized();
        
        if (m_isMaximized && !wasMaximized) {
            // Сохраняем нормальную геометрию перед максимизацией
            // Используем geometry() вместо normalGeometry(), так как normalGeometry() может быть неточным
            QRect currentGeo = geometry();
            // Если окно еще не максимизировано, сохраняем текущую геометрию
            // Но нужно проверить, не развернуто ли оно уже на весь экран
            QScreen *screen = QApplication::screenAt(currentGeo.center());
            if (!screen) {
                screen = QApplication::primaryScreen();
            }
            if (screen) {
                QRect screenGeo = screen->availableGeometry();
                // Если текущая геометрия не равна геометрии экрана, сохраняем её
                if (currentGeo != screenGeo) {
                    m_normalGeometry = currentGeo;
                } else if (m_normalGeometry.isEmpty() || !m_normalGeometry.isValid()) {
                    // Если окно уже развернуто, но геометрия не сохранена, 
                    // пытаемся получить её из normalGeometry() как fallback
                    QRect normalGeo = normalGeometry();
                    if (normalGeo.isValid() && !normalGeo.isEmpty() && normalGeo != screenGeo) {
                        m_normalGeometry = normalGeo;
                    }
                }
            }
            m_isSnapped = false;
        } else if (!m_isMaximized) {
            m_isSnapped = false;
        }
    }
}

void MainWindow::moveEvent(QMoveEvent *event) {
    QMainWindow::moveEvent(event);
    // Aero Snap обрабатывается через WM_MOVING в nativeEvent
}

void MainWindow::updateRoundedCorners() {
    const int radius = 10; // px (≈ +20% к базовым 8)
    if (isMaximized() || isFullScreen()) {
        clearMask();
        return;
    }
    QPainterPath path;
    path.addRoundedRect(rect(), radius, radius);
    QRegion region(path.toFillPolygon().toPolygon());
    setMask(region);
}

void MainWindow::updateBorderStyle() {
    if (!m_frameOverlay)
        return;
    const bool visible = !(isMaximized() || isFullScreen());
    m_frameOverlay->setVisible(visible);
    if (visible) {
        m_frameOverlay->setGeometry(rect());
        m_frameOverlay->raise();
        m_frameOverlay->update();
    }
}

void MainWindow::setupConnections() {
    // Подключаем сигнал клика на шестеренку
    if (m_titleBar) {
        connect(m_titleBar, &TitleBar::settingsClicked, this, &MainWindow::openSettings);
        connect(m_titleBar, &TitleBar::iconTripleClicked, this, &MainWindow::openSnakeGame);
    }
}

CodeEditor *MainWindow::getEditorFromWidget(QWidget *widget) const {
    if (!widget) return nullptr;
    
    // Если виджет сам является редактором
    CodeEditor *editor = qobject_cast<CodeEditor*>(widget);
    if (editor) return editor;
    
    // Обычные вкладки: редактор — прямой ребёнок контейнера
    QList<CodeEditor*> editors = widget->findChildren<CodeEditor*>(QString(), Qt::FindDirectChildrenOnly);
    if (!editors.isEmpty()) {
        return editors.first();
    }

    // Вкладки задач: редактор внутри сплиттера
    editors = widget->findChildren<CodeEditor*>();
    if (!editors.isEmpty()) {
        return editors.first();
    }
    
    return nullptr;
}

TurtleTaskWidget *MainWindow::turtleTaskFromWidget(QWidget *widget) const {
    return qobject_cast<TurtleTaskWidget*>(widget);
}

TurtleTaskWidget *MainWindow::currentTurtleTask() const {
    if (!m_tabWidget)
        return nullptr;
    return turtleTaskFromWidget(m_tabWidget->currentWidget());
}

CodeEditor *MainWindow::getEditorFromTabWidget(int index) const {
    if (!m_tabWidget || index < 0 || index >= m_tabWidget->count()) {
        return nullptr;
    }
    return getEditorFromWidget(m_tabWidget->widget(index));
}

CodeEditor *MainWindow::currentEditor() const {
    if (!m_tabWidget || m_tabWidget->count() == 0)
        return nullptr;
    return getEditorFromWidget(m_tabWidget->currentWidget());
}

int MainWindow::findTabByPath(const QString &path) const {
    if (path.isEmpty() || !m_tabWidget)
        return -1;
    QFileInfo pathInfo(path);
    QString canonicalPath = pathInfo.canonicalFilePath();
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        CodeEditor *editor = getEditorFromTabWidget(i);
        if (editor && m_editorToPath.contains(editor)) {
            QString tabPath = m_editorToPath[editor];
            if (!tabPath.isEmpty() && QFileInfo(tabPath).canonicalFilePath() == canonicalPath) {
                return i;
            }
        }
    }
    return -1;
}

void MainWindow::setupEditor(CodeEditor *editor) {
    if (!editor) return;
    
    // Применяем размер шрифта из настроек
    QSettings settings;
    int fontSize = settings.value("editor/fontSize", 10).toInt();
    QFont font = editor->font();
    font.setPointSize(fontSize);
    // Устанавливаем семейство шрифта Consolas с fallback
    font.setFamily("Consolas, 'Courier New', monospace");
    editor->setFont(font);
    
    // Подсветка Python встроена в QScintilla через лексер
    // Устанавливаем тему для редактора
    editor->setTheme(m_currentTheme);

    // Автодополнение (Ctrl+Space)
    // Загружаем базовые дополнения только если еще не загружены (кэшируем для всех файлов)
    if (m_cachedBaseCompletions.isEmpty()) {
        m_cachedBaseCompletions = loadPythonCompletions();
        if (m_cachedBaseCompletions.isEmpty()) {
            // Fallback на статический список, если не удалось загрузить из Python
            m_cachedBaseCompletions = QStringList({
            // Ключевые слова Python
            "False","await","else","import","pass","None","break","except","in","raise",
            "True","class","finally","is","return","and","continue","for","lambda","try",
            "as","def","from","nonlocal","while","assert","del","global","not","with",
            "async","elif","if","or","yield",
            // Встроенные функции
            "abs","delattr","hash","memoryview","set","all","dict","help","min","setattr",
            "any","dir","hex","next","slice","ascii","divmod","id","object","sorted",
            "bin","enumerate","input","oct","staticmethod","bool","eval","int","open","str",
            "breakpoint","exec","isinstance","ord","sum","bytearray","filter","issubclass","pow","super",
            "bytes","float","iter","print","tuple","callable","format","len","property","type",
            "chr","frozenset","list","range","vars","classmethod","getattr","locals","repr","zip",
            "compile","globals","map","reversed","__import__","complex","hasattr","max","round",
            // Константы
            "Ellipsis","NotImplemented","__debug__",
            // Встроенные исключения
            "BaseException","Exception","ArithmeticError","AssertionError","AttributeError",
            "BlockingIOError","BrokenPipeError","BufferError","BytesWarning",
            "ChildProcessError","ConnectionAbortError","ConnectionError",
            "ConnectionRefusedError","ConnectionResetError","DeprecationWarning",
            "EOFError","EnvironmentError","FileExistsError",
            "FileNotFoundError","FloatingPointError","FutureWarning","GeneratorExit",
            "IOError","ImportError","ImportWarning","IndentationError","IndexError",
            "InterruptedError","IsADirectoryError","KeyError","KeyboardInterrupt",
            "LookupError","MemoryError","ModuleNotFoundError","NameError",
            "NotImplementedError","OSError","OverflowError","PendingDeprecationWarning",
            "PermissionError","ProcessLookupError","RecursionError","ReferenceError",
            "ResourceWarning","RuntimeError","RuntimeWarning","StopAsyncIteration",
            "StopIteration","SyntaxError","SyntaxWarning","SystemError","SystemExit",
            "TabError","TimeoutError","TypeError","UnboundLocalError",
            "UnicodeDecodeError","UnicodeEncodeError","UnicodeError",
            "UnicodeTranslateError","UnicodeWarning","UserWarning","ValueError",
            "Warning","WindowsError","ZeroDivisionError",
            // Модули стандартной библиотеки
            "abc","atexit","argparse","array","asyncio","base64","bdb","binascii","bisect",
            "builtins","bz2","calendar","collections","concurrent","configparser","contextlib",
            "copy","csv","ctypes","datetime","decimal","difflib","dis","doctest","email","enum",
            "errno","faulthandler","filecmp","fileinput","fnmatch","fractions","functools",
            "gc","getopt","getpass","gettext","glob","gzip","hashlib","heapq","hmac","html","http",
            "imaplib","imp","importlib","inspect","io","ipaddress","itertools","json","keyword",
            "linecache","locale","logging","lzma","math","mimetypes","mmap","modulefinder",
            "multiprocessing","netrc","numbers","operator","os","pathlib","pdb","pickle","pipes",
            "pkgutil","platform","plistlib","poplib","posixpath","pprint","profile","pstats","pty",
            "pyclbr","pydoc","queue","quopri","random","re","readline","reprlib","resource",
            "rlcompleter","sched","secrets","select","selectors","shelve","shlex","shutil",
            "signal","site","smtpd","smtplib","socket","socketserver","sqlite3","ssl","stat",
            "statistics","string","stringprep","struct","subprocess","symtable","sys","sysconfig",
            "tabnanny","tarfile","tempfile","termios","textwrap","threading","time","timeit",
            "tkinter","token","tokenize","traceback","tracemalloc","types","typing","unicodedata",
            "unittest","urllib","uuid","venv","warnings","wave","weakref","webbrowser","xml",
            "xmlrpc","zipapp","zipfile","zipimport","zlib"
            });
        }
    }
    
    // Создаем QSet для быстрой проверки префиксов (один раз)
    if (m_cachedBaseCompletionsSet.isEmpty() && !m_cachedBaseCompletions.isEmpty()) {
        m_cachedBaseCompletionsSet = QSet<QString>::fromList(m_cachedBaseCompletions);
    }
    
    // Создаем общую модель для базовых дополнений, если еще не создана
    if (!m_sharedBaseCompletionsModel) {
        m_sharedBaseCompletionsModel = new QStringListModel(m_cachedBaseCompletions, this);
    }
    
    // Используем общую модель для базовых дополнений
    // QCompleter отключен, используем встроенный автокомплит QScintilla
    // setupEditorCompletions(editor, m_sharedBaseCompletionsModel);
    
    // Инициализируем кэш слов из файла для этого редактора
    m_fileCompletionsCache[editor] = QStringList();
    m_fileContentHash[editor] = QString();
    
    // Обновляем шрифт popup при изменении размера шрифта редактора
    connect(editor, &CodeEditor::fontSizeChanged, this, [editor]() {
        QCompleter *comp = editor->completer();
        if (comp && comp->popup()) {
            QAbstractItemView *popup = comp->popup();
            popup->setFont(editor->font());
            int lineHeight = editor->fontMetrics().height();
            popup->setStyleSheet(QString("QAbstractItemView::item { height: %1px; }").arg(lineHeight));
        }
    });
    
    // Используем общий список базовых слов для updateCompletionFromDocument
    // (сохраняем его в модели completer)

    // Обновление списка автодополнения по документу с debounce для оптимизации
    connect(editor->document(), &QTextDocument::contentsChanged, this, [this]() {
        if (m_completionUpdateTimer) {
            m_completionUpdateTimer->stop();
            m_completionUpdateTimer->start();
        }
    });
    
    // Обновление автокомплита при изменении позиции курсора (для проверки @task)
    connect(editor, &QsciScintilla::cursorPositionChanged, this, [this, editor](int line, int index) {
        Q_UNUSED(line);
        Q_UNUSED(index);
        // Проверяем, является ли этот редактор текущим
        CodeEditor *current = currentEditor();
        if (current == editor) {
            // Обновляем автокомплит с небольшой задержкой для оптимизации
            if (m_completionUpdateTimer) {
                m_completionUpdateTimer->stop();
                m_completionUpdateTimer->start(100); // 100ms задержка
            }
        }
    });
    
    // Изменение размера шрифта колесиком мыши (Ctrl+колесико)
    connect(editor, &CodeEditor::fontSizeChanged, this, &MainWindow::applyFontSizeToAllEditors);
    
    // Обновляем заголовок окна при изменении документа
    connect(editor->document(), &QTextDocument::modificationChanged, this, [this, editor]() {
        updateWindowTitle();
        updateTabText(editor);
    });
    
    // Автодополнение будет обновлено после загрузки содержимого файла (если файл открывается)
    // или при смене вкладки через onTabChanged

    editor->setCommentShortcut(shortcutFromSettings(
        QStringLiteral("commentLines"), QKeySequence(QStringLiteral("Alt+3"))));
    editor->setUncommentShortcut(shortcutFromSettings(
        QStringLiteral("uncommentLines"), QKeySequence(QStringLiteral("Alt+Ctrl+3"))));
}

void MainWindow::updateTabText(CodeEditor *editor) {
    if (!editor || !m_tabWidget) return;
    
    // Находим индекс вкладки через поиск контейнера, в котором находится редактор
    int index = -1;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        QWidget *widget = m_tabWidget->widget(i);
        CodeEditor *ed = getEditorFromWidget(widget);
        if (ed == editor) {
            index = i;
            break;
        }
    }
    if (index < 0) return;
    
    QString filePath = m_editorToPath.value(editor, QString());
    QString tabText;
    TurtleTaskWidget *task = turtleTaskFromWidget(m_tabWidget->widget(index));
    if (!filePath.isEmpty()) {
        tabText = QFileInfo(filePath).fileName();
    } else if (task) {
        tabText = task->tabTitle();
    } else {
        tabText = tr("Новый файл");
    }
    // Показываем * только если файл изменен (несохранен или изменен после сохранения)
    if (editor->document()->isModified()) {
        tabText += "*";
    }
    m_tabWidget->setTabText(index, tabText);
}

void MainWindow::updateWindowTitle() {
    CodeEditor *editor = currentEditor();
    if (!editor) {
        setWindowTitle("Vuzhyk");
        return;
    }
    
    QString filePath = m_editorToPath.value(editor, QString());
    QString title;
    TurtleTaskWidget *task = currentTurtleTask();
    if (!filePath.isEmpty()) {
        title = "Vuzhyk - " + QFileInfo(filePath).fileName();
    } else if (task) {
        title = "Vuzhyk - " + task->tabTitle();
    } else {
        title = "Vuzhyk - Новый файл";
    }
    // Показываем * только если файл изменен (несохранен или изменен после сохранения)
    if (editor->document()->isModified()) {
        title += "*";
    }
    setWindowTitle(title);
}

void MainWindow::refreshFileTypeActions()
{
    CodeEditor *editor = currentEditor();
    QString filePath = editor ? m_editorToPath.value(editor) : QString();
    bool isHtml = isHtmlFile(filePath);
    QString fileExt = QFileInfo(filePath).suffix().toLower();

    if (m_actDebug) {
        m_actDebug->setVisible(!isHtml);
    }
    if (m_actDebugNext) {
        m_actDebugNext->setVisible(!isHtml);
    }

    // Кнопка Run: текст и видимость по инструкции из расширений (location "run" + fileTypes)
    if (m_actRun && m_extensionManager) {
        QVector<const ExtensionManager::ExtensionAction *> runActions = m_extensionManager->runActionsForFileType(fileExt);
        if (!runActions.isEmpty()) {
            m_actRun->setText(runActions.first()->label);
            m_actRun->setVisible(true);
        } else {
            m_actRun->setText(tr("Запустить скрипт"));
            m_actRun->setVisible(true);
        }
    } else if (m_actRun) {
        m_actRun->setText(tr("Запустить скрипт"));
        m_actRun->setVisible(true);
    }

    if (m_actTerminate) {
        m_actTerminate->setVisible(true);
    }

    if (m_actRunInTerminal) {
        if (isHtml) {
            m_actRunInTerminal->setText(tr("Запуск в системном браузере"));
        } else {
            m_actRunInTerminal->setText(tr("Запуск в отдельном окне консоли"));
        }
        m_actRunInTerminal->setVisible(true);
    }
}

bool MainWindow::isHtmlFile(const QString &path) const
{
    if (path.isEmpty()) {
        return false;
    }
    QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == "html" || suffix == "htm" || suffix == "xhtml";
}

bool MainWindow::openEmbeddedBrowserForFile(const QString &filePath)
{
    QString executable;
    QStringList baseArguments;
    if (m_extensionManager) {
        if (const ExtensionManager::ExtensionAction *action =
                m_extensionManager->findActionById("open-embedded-browser")) {
            executable = action->executable;
            baseArguments = action->arguments;
        }
    }

    if (executable.isEmpty()) {
        const QDir appDir(QCoreApplication::applicationDirPath());
        const QString candidate = appDir.filePath("extensions/EmbeddedBrowserExtension/EmbeddedBrowserExtension.exe");
        if (QFile::exists(candidate)) {
            executable = candidate;
        }
    }

    if (executable.isEmpty()) {
        return false;
    }

    QFileInfo fi(filePath);
    QString absoluteFilePath = fi.absoluteFilePath();
    if (absoluteFilePath.isEmpty() || !QFile::exists(absoluteFilePath)) {
        return false;
    }

    if (m_embeddedBrowserProcess && m_embeddedBrowserProcess->state() != QProcess::NotRunning) {
        m_embeddedBrowserProcess->terminate();
        if (!m_embeddedBrowserProcess->waitForFinished(1000)) {
            m_embeddedBrowserProcess->kill();
        }
        m_embeddedBrowserProcess->deleteLater();
        m_embeddedBrowserProcess = nullptr;
    }

    m_embeddedBrowserProcess = new QProcess(this);
    connect(m_embeddedBrowserProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus status) {
                if (status != QProcess::NormalExit || exitCode != 0) {
                    statusBar()->showMessage(tr("Встроенный браузер закрылся (код %1)").arg(exitCode), 5000);
                }
                if (m_embeddedBrowserProcess) {
                    m_embeddedBrowserProcess->deleteLater();
                    m_embeddedBrowserProcess = nullptr;
                }
            });
    connect(m_embeddedBrowserProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        const QString errMsg = tr("Ошибка запуска встроенного браузера: %1").arg(m_embeddedBrowserProcess ? m_embeddedBrowserProcess->errorString() : tr("неизвестно"));
        statusBar()->showMessage(errMsg, 5000);
        qWarning() << "Embedded browser failed to start:" << errMsg;
    });
    connect(m_embeddedBrowserProcess, &QProcess::readyReadStandardError, this, [this]() {
        if (m_embeddedBrowserProcess) {
            qDebug() << "EmbeddedBrowser stderr:" << m_embeddedBrowserProcess->readAllStandardError();
        }
    });
    connect(m_embeddedBrowserProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        if (m_embeddedBrowserProcess) {
            qDebug() << "EmbeddedBrowser stdout:" << m_embeddedBrowserProcess->readAllStandardOutput();
        }
    });

    QStringList args = baseArguments;
    if (!args.contains(absoluteFilePath)) {
        args.append(absoluteFilePath);
    }

    m_embeddedBrowserProcess->start(executable, args);
    return m_embeddedBrowserProcess->state() != QProcess::NotRunning;
}

bool MainWindow::startExtensionRunProcess(const ExtensionManager::ExtensionAction &action, const QString &absoluteFilePath)
{
    if (action.executable.isEmpty() || !QFile::exists(action.executable)) {
        return false;
    }

    if (m_embeddedBrowserProcess && m_embeddedBrowserProcess->state() != QProcess::NotRunning) {
        m_embeddedBrowserProcess->terminate();
        if (!m_embeddedBrowserProcess->waitForFinished(1000)) {
            m_embeddedBrowserProcess->kill();
        }
        m_embeddedBrowserProcess->deleteLater();
        m_embeddedBrowserProcess = nullptr;
    }

    m_embeddedBrowserProcess = new QProcess(this);
    connect(m_embeddedBrowserProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus status) {
                if (status != QProcess::NormalExit || exitCode != 0) {
                    statusBar()->showMessage(tr("Процесс расширения завершился (код %1)").arg(exitCode), 5000);
                }
                if (m_embeddedBrowserProcess) {
                    m_embeddedBrowserProcess->deleteLater();
                    m_embeddedBrowserProcess = nullptr;
                }
            });
    connect(m_embeddedBrowserProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        const QString errMsg = tr("Ошибка запуска: %1").arg(m_embeddedBrowserProcess ? m_embeddedBrowserProcess->errorString() : tr("неизвестно"));
        statusBar()->showMessage(errMsg, 5000);
        qWarning() << "Extension run process failed:" << errMsg;
    });

    QStringList args = action.arguments;
    if (!args.contains(absoluteFilePath)) {
        args.append(absoluteFilePath);
    }

    m_embeddedBrowserProcess->start(action.executable, args);
    return m_embeddedBrowserProcess->state() != QProcess::NotRunning;
}

bool MainWindow::openInSystemDefault(const QString &filePath)
{
    const QString absolutePath = QFileInfo(filePath).absoluteFilePath();
    if (absolutePath.isEmpty() || !QFile::exists(absolutePath)) {
        statusBar()->showMessage(tr("Файл не найден: %1").arg(filePath), 3000);
        return false;
    }

    QUrl fileUrl = QUrl::fromLocalFile(absolutePath);
    bool success = QDesktopServices::openUrl(fileUrl);
    
    if (success) {
        statusBar()->showMessage(tr("HTML открыт в системном браузере"), 3000);
    } else {
        statusBar()->showMessage(tr("Не удалось открыть HTML в системном браузере"), 3000);
    }
    
    return success;
}

void MainWindow::newFile() {
    CodeEditor *editor = currentEditor();
    if (editor && editor->document()->isModified()) {
        if (!maybeSave())
            return;
    }
    
    // Ограничиваем количество открытых вкладок для экономии памяти
    const int MAX_TABS = 50;
    int codeEditorCount = 0;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        CodeEditor *editor = getEditorFromTabWidget(i);
        if (editor) codeEditorCount++;
    }
    if (codeEditorCount >= MAX_TABS) {
        QMessageBox::warning(this, tr("Превышен лимит"), 
            tr("Достигнут максимум %1 открытых вкладок редактора. Закройте некоторые вкладки перед открытием новых.").arg(MAX_TABS));
        return;
    }
    
    // Создаем новую вкладку с обёрткой для анимации
    QWidget *container = new QWidget(m_tabWidget);
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    CodeEditor *newEditor = new CodeEditor(container);
    setupEditor(newEditor);
    layout->addWidget(newEditor);
    
    // Убеждаемся, что контейнер и редактор правильно растягиваются
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    newEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    int index = m_tabWidget->addTab(container, tr("Новый файл"));
    m_editorToPath[newEditor] = QString(); // Пустой путь для нового файла
    // Не переключаемся на вкладку сразу при анимации — иначе контент мелькнёт на месте; анимация переключит в конце
    if (m_isInitializing) {
        m_tabWidget->setCurrentIndex(index);
    } else {
        container->hide(); // Скрываем до старта анимации, чтобы контент не мелькал на месте текущей вкладки
    }
    
    // Помечаем редактор как начальный файл, если он создан при инициализации
    if (m_isInitializing) {
        newEditor->setProperty("isInitialFile", true);
    }
    
    // Сбрасываем флаг изменения для нового пустого файла
    // Это предотвратит показ диалога сохранения при открытии другого файла
    if (newEditor->toPlainText().trimmed().isEmpty()) {
        newEditor->document()->setModified(false);
    }
    
    // Анимируем появление вкладки только если это не инициализация
    if (!m_isInitializing) {
        animateTabOpening(container, index);
    }
    
    // Обновляем название вкладки и заголовок окна
    updateTabText(newEditor);
    updateWindowTitle();
    refreshFileTypeActions();
}

void MainWindow::openTurtleTaskWorkbook()
{
    openTurtleTask(turtleTaskForward100());
}

void MainWindow::openTurtleTask(const TurtleTaskDefinition &task)
{
    if (!m_tabWidget)
        return;

    for (int i = 0; i < m_tabWidget->count(); ++i) {
        TurtleTaskWidget *existing = turtleTaskFromWidget(m_tabWidget->widget(i));
        if (existing && existing->taskId() == QLatin1String("turtle_workbook")) {
            m_tabWidget->setCurrentIndex(i);
            return;
        }
    }

    const int MAX_TABS = 50;
    int codeEditorCount = 0;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        if (getEditorFromTabWidget(i))
            ++codeEditorCount;
    }
    if (codeEditorCount >= MAX_TABS) {
        QMessageBox::warning(this, tr("Превышен лимит"),
            tr("Достигнут максимум %1 открытых вкладок редактора. Закройте некоторые вкладки перед открытием новых.").arg(MAX_TABS));
        return;
    }

    auto *widget = new TurtleTaskWidget(task, m_tabWidget);
    setupEditor(widget->editor());
    widget->editor()->setFileExtension(QStringLiteral("py"));
    widget->editor()->setPlainText(task.starterCode);
    widget->editor()->setModified(false);
    widget->setTheme(m_currentTheme);
    widget->setPythonGetter([this]() { return detectPythonExecutable(); });
    connect(widget, &TurtleTaskWidget::processOutput, this, [this](const QString &text, bool isError) {
        appendOutput(text, isError);
    });

    m_editorToPath[widget->editor()] = QString();
    const int index = m_tabWidget->addTab(widget, task.title);
    if (m_isInitializing) {
        m_tabWidget->setCurrentIndex(index);
    } else {
        widget->hide();
        animateTabOpening(widget, index);
    }
    updateTabText(widget->editor());
    updateWindowTitle();
    refreshFileTypeActions();
}

void MainWindow::newPyrobSolution() {
    CodeEditor *editor = currentEditor();
    if (editor && editor->document()->isModified()) {
        if (!maybeSave())
            return;
    }
    
    // Ограничиваем количество открытых вкладок для экономии памяти
    const int MAX_TABS = 50;
    int codeEditorCount = 0;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        CodeEditor *editor = getEditorFromTabWidget(i);
        if (editor) codeEditorCount++;
    }
    if (codeEditorCount >= MAX_TABS) {
        QMessageBox::warning(this, tr("Превышен лимит"), 
            tr("Достигнут максимум %1 открытых вкладок редактора. Закройте некоторые вкладки перед открытием новых.").arg(MAX_TABS));
        return;
    }
    
    // Генерируем случайный шестизначный код
    int randomCode = QRandomGenerator::global()->bounded(100000, 999999);
    QString fileName = QString("pyrob-solution-%1.py").arg(randomCode);
    
    // Получаем путь к временной папке Windows
    QString tempPath = QDir::tempPath();
    QString filePath = QDir(tempPath).filePath(fileName);
    
    // Создаем файл с шаблоном
    QString templateContent = 
        "#!/usr/bin/python3\n\n"
        "from pyrob.api import *\n\n\n"
        "@task\n"
        "def task_name(): #Замените task_name на название задачи\n"
        "    pass\n\n\n"
        "if __name__ == '__main__':\n"
        "    run_tasks()\n";
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("Ошибка"), 
            tr("Не удалось создать файл: %1").arg(file.errorString()));
        return;
    }
    
    // Записываем напрямую UTF-8 байты
    QByteArray utf8Data = templateContent.toUtf8();
    file.write(utf8Data);
    file.close();
    
    // Открываем файл в редакторе
    loadFromPath(filePath);
    
    // Принудительно вызываем обновление автокомплита для загрузки задач pyrob
    // Используем больший таймаут для первого запуска, чтобы убедиться, что редактор полностью инициализирован
    QTimer::singleShot(300, this, [this]() {
        CodeEditor *editor = currentEditor();
        if (editor && editor->lexer()) {
            try {
                updateCompletionFromDocument();
            } catch (...) {
                // Игнорируем ошибки при обновлении автокомплита
            }
        }
    });
}

QString MainWindow::pythonPath() const {
    return detectPythonExecutable();
}

QString MainWindow::showFilePicker(const QString &filter, const QString &initialDir, bool isSave, bool isDirectory) {
    QString pickerExe = QCoreApplication::applicationDirPath() + "/FilePicker.exe";
    
    if (!QFile::exists(pickerExe)) {
        // Fallback на обычный диалог, если FilePicker.exe не найден
        QFileDialog dialog(this, isSave ? tr("Сохранить файл") : (isDirectory ? tr("Открыть папку") : tr("Открыть файл")), 
                          initialDir, filter);
        dialog.setFileMode(isDirectory ? QFileDialog::Directory : (isSave ? QFileDialog::AnyFile : QFileDialog::ExistingFile));
        dialog.setAcceptMode(isSave ? QFileDialog::AcceptSave : QFileDialog::AcceptOpen);
        dialog.setOption(QFileDialog::DontUseNativeDialog, true);
        if (dialog.exec() == QDialog::Accepted) {
            return dialog.selectedFiles().first();
        }
        return QString();
    }
    
    QProcess process;
    QStringList args;
    
    // Извлекаем расширение из фильтра
    QString extension;
    if (!filter.isEmpty()) {
        QString firstFilter;
        if (filter.contains(";;")) {
            // Берем первый фильтр из списка
            firstFilter = filter.split(";;").first();
        } else {
            firstFilter = filter;
        }
        
        // Извлекаем паттерн: "Python файлы (*.py)" -> ".py"
        // Ищем последнюю открывающую скобку и соответствующую закрывающую
        int openBrace = firstFilter.lastIndexOf('(');
        int closeBrace = firstFilter.lastIndexOf(')');
        
        if (openBrace >= 0 && closeBrace > openBrace) {
            QString pattern = firstFilter.mid(openBrace + 1, closeBrace - openBrace - 1).trimmed();
            
            // Если паттерн содержит несколько расширений, берем первое
            // "*.py *.pyw" -> "*.py"
            if (pattern.contains(" ")) {
                QStringList parts = pattern.split(" ", QString::SkipEmptyParts);
                if (!parts.isEmpty()) {
                    pattern = parts.first();
                }
            }
            
            // Убираем возможные пробелы и звездочку
            pattern = pattern.trimmed();
            if (pattern.startsWith("*")) {
                pattern = pattern.mid(1);
            }
            
            extension = pattern;
        } else {
            // Если формат не распознан, пробуем найти паттерн в скобках другим способом
            if (firstFilter.contains("(") && firstFilter.contains(")")) {
                int start = firstFilter.indexOf("(");
                int end = firstFilter.indexOf(")");
                if (start < end) {
                    QString pattern = firstFilter.mid(start + 1, end - start - 1).trimmed();
                    if (pattern.contains(" ")) {
                        pattern = pattern.split(" ").first();
                    }
                    if (pattern.startsWith("*")) {
                        pattern = pattern.mid(1);
                    }
                    extension = pattern;
                }
            }
        }
    }
    
    // Если расширение не найдено, используем дефолтное .py
    if (extension.isEmpty()) {
        extension = ".py";
    }
    
    // Передаем расширение как обязательный параметр
    args << "-e" << extension;
    if (!initialDir.isEmpty()) {
        // Нормализуем путь: делаем абсолютным и заменяем слеши на обратные для Windows
        QDir dir(initialDir);
        QString normalizedDir = dir.absolutePath();
        normalizedDir.replace('/', '\\');
        args << "-d" << normalizedDir;
    }
    if (isSave) {
        args << "-s";
    }
    if (isDirectory) {
        args << "--dir";
    }
    
    process.start(pickerExe, args);
    if (!process.waitForStarted(5000)) {
        // Если процесс не запустился, возвращаем пустую строку
        return QString();
    }
    
    // Неблокирующее ожидание с обработкой событий UI
    QElapsedTimer timer;
    timer.start();
    const int timeoutMs = 300000; // 5 минут — пользователь может долго выбирать файл/папку
    
    // Обрабатываем события, пока процесс не завершится или не истечет таймаут
    while (process.state() == QProcess::Running && timer.elapsed() < timeoutMs) {
        QApplication::processEvents(QEventLoop::AllEvents, 100);
        QThread::msleep(10); // Небольшая задержка, чтобы не нагружать CPU
    }
    
    if (process.state() == QProcess::Running) {
        // Таймаут истек
        process.kill();
        process.waitForFinished(1000);
        return QString();
    }
    
    QByteArray output = process.readAllStandardOutput();
    QString path = QString::fromUtf8(output).trimmed();
    
    return path;
}

bool MainWindow::isTempPyrobSolutionPath(const QString &path) const
{
    if (path.isEmpty()) {
        return false;
    }

    const QFileInfo fi(path);
    if (!fi.fileName().startsWith(QStringLiteral("pyrob-solution-"), Qt::CaseInsensitive)) {
        return false;
    }
    if (fi.suffix().compare(QStringLiteral("py"), Qt::CaseInsensitive) != 0) {
        return false;
    }

    const QString fileDir = fi.absolutePath();
    const QString tempDir = QDir(QDir::tempPath()).absolutePath();
    if (!fileDir.compare(tempDir, Qt::CaseInsensitive)) {
        return true;
    }

    const QString canonicalFileDir = QDir(fileDir).canonicalPath();
    const QString canonicalTempDir = QDir(tempDir).canonicalPath();
    return !canonicalFileDir.isEmpty()
        && !canonicalTempDir.isEmpty()
        && !canonicalFileDir.compare(canonicalTempDir, Qt::CaseInsensitive);
}

QString MainWindow::saveUnsavedToTemp(CodeEditor *editor) {
    if (!editor) {
        return QString();
    }
    
    // Получаем путь к временной папке
    QString tempDir = QDir::tempPath();
    QString tempFilePath = QDir(tempDir).filePath("vuzhyk_temp.py");
    
    // Удаляем существующий файл, если он есть
    if (QFile::exists(tempFilePath)) {
        QFile::remove(tempFilePath);
    }
    
    // Сохраняем содержимое редактора во временный файл
    QFile file(tempFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    
    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << editor->toPlainText();
    file.close();
    
    return tempFilePath;
}

void MainWindow::openFile() {
    CodeEditor *editor = currentEditor();
    if (editor) {
        // Проверяем, является ли файл пустым, новым (без пути) и начальным файлом при запуске
        QString filePath = m_editorToPath.value(editor, QString());
        QString text = editor->toPlainText();
        bool isEmpty = text.trimmed().isEmpty();
        bool isNewFile = filePath.isEmpty();
        bool isInitialFile = editor->property("isInitialFile").toBool();
        
        // Если файл пустой, новый и является начальным файлом при запуске, просто закрываем его без диалога
        // Не проверяем isModified, так как пустой новый файл не нужно сохранять
        if (isEmpty && isNewFile && isInitialFile) {
            // Сбрасываем флаг изменения перед закрытием, чтобы избежать диалога
            editor->document()->setModified(false);
            // Закрываем текущую вкладку
            int currentIndex = m_tabWidget->currentIndex();
            if (currentIndex >= 0) {
                closeTab(currentIndex);
            }
        } else if (editor->document()->isModified()) {
            // Для непустых или сохраненных файлов показываем диалог только если файл изменен
            if (!maybeSave())
                return;
        }
    }
    
    QString selectedPath = showFilePicker(
        "Python файлы (*.py)", // Фильтр для извлечения расширения
        QString(), // Директория не передается из редактора кода
        false,
        false
    );
    
    if (selectedPath.isEmpty()) return;
    
    // Проверяем, не открыт ли файл уже в другой вкладке
    int existingTab = findTabByPath(selectedPath);
    if (existingTab >= 0) {
        m_tabWidget->setCurrentIndex(existingTab);
        return;
    }
    
    loadFromPath(selectedPath);
}

void MainWindow::saveFile() {
    CodeEditor *editor = currentEditor();
    if (!editor) return;
    
    QString filePath = m_editorToPath.value(editor, QString());
    if (filePath.isEmpty() || isTempPyrobSolutionPath(filePath)) {
        saveFileAs();
        return;
    }
    saveToPath(filePath);
}

void MainWindow::saveFileAs() {
    CodeEditor *editor = currentEditor();
    if (!editor) return;
    
    QString currentPath = m_editorToPath.value(editor, QString());
    QString selectedPath = showFilePicker(
        "Python файлы (*.py)", // Фильтр для извлечения расширения
        QString(), // Директория не передается из редактора кода
        true,
        false
    );
    
    if (selectedPath.isEmpty()) return;
    // Гарантируем расширение .py по умолчанию
    if (!selectedPath.endsWith(".py", Qt::CaseInsensitive))
        selectedPath += ".py";
    saveToPath(selectedPath);
}

void MainWindow::saveAll() {
    if (!m_tabWidget) return;
    
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        CodeEditor *editor = getEditorFromTabWidget(i);
        if (!editor) continue;
        
        if (editor->document()->isModified()) {
            QString filePath = m_editorToPath.value(editor, QString());
            if (filePath.isEmpty()) {
                // Пропускаем несохраненные файлы без пути
                continue;
            }
            
            QFile f(filePath);
            if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                QMessageBox::critical(this, tr("Ошибка записи"), 
                    tr("Не удалось сохранить файл %1: %2").arg(filePath, f.errorString()));
                continue;
            }
            QTextStream ts(&f);
            ts.setCodec("UTF-8");
            ts << editor->toPlainText();
            f.close();
            editor->document()->setModified(false);
            
            // Обновляем имя вкладки
            updateTabText(editor);
        }
    }
    
    updateWindowTitle();
    statusBar()->showMessage(tr("Все файлы сохранены"), 3000);
}

void MainWindow::runScript() {
    if (TurtleTaskWidget *task = currentTurtleTask()) {
        const QString python = detectPythonExecutable();
        if (python.isEmpty()) {
            QMessageBox::critical(this, tr("Python не найден"), tr("Не удалось найти интерпретатор Python."));
            return;
        }
        task->run(python, false);
        return;
    }

    CodeEditor *editor = currentEditor();
    if (!editor) return;
    
    QString filePath = m_editorToPath.value(editor, QString());
    
    // Если файл не сохранен, сохраняем его во временную папку
    if (filePath.isEmpty()) {
        filePath = saveUnsavedToTemp(editor);
        if (filePath.isEmpty()) {
            QMessageBox::critical(this, tr("Ошибка"), tr("Не удалось сохранить файл во временную папку."));
            return;
        }
    } else if (editor->document()->isModified()) {
        // Файл сохранен, но есть несохраненные изменения - сохраняем их
        QFile f(filePath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::critical(this, tr("Ошибка записи"), 
                tr("Не удалось сохранить файл %1: %2").arg(filePath, f.errorString()));
            return;
        }
        QTextStream ts(&f);
        ts.setCodec("UTF-8");
        ts << editor->toPlainText();
        f.close();
        editor->document()->setModified(false);
        updateTabText(editor);
    }

    // Если для типа файла есть действие расширения с location "run" — запускаем процесс (чтобы кнопка «Завершить» могла его закрыть)
    QString fileExt = QFileInfo(filePath).suffix().toLower();
    if (m_extensionManager) {
        QVector<const ExtensionManager::ExtensionAction *> runActions = m_extensionManager->runActionsForFileType(fileExt);
        if (!runActions.isEmpty()) {
            QString absolutePath = QFileInfo(filePath).absoluteFilePath();
            if (startExtensionRunProcess(*runActions.first(), absolutePath)) {
                statusBar()->showMessage(runActions.first()->label + tr(" — выполнено"), 3000);
            } else {
                statusBar()->showMessage(tr("Не удалось запустить: %1").arg(runActions.first()->label), 5000);
            }
            return;
        }
    }

    // Определяем, какой виджет сейчас виден
    bool isConsoleVisible = (m_outputStack && m_outputStack->currentIndex() == 1);
    
    // Проверяем, запущен ли скрипт
    if (isConsoleVisible && m_console && m_console->isRunning()) {
        QMessageBox::information(this, tr("Запуск"), tr("Скрипт уже выполняется."));
        return;
    }
    
    if (!isConsoleVisible && m_process && m_process->state() != QProcess::NotRunning) {
        QMessageBox::information(this, tr("Запуск"), tr("Скрипт уже выполняется."));
        return;
    }

    // Очищаем подсветку ошибки во всех редакторах
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        CodeEditor *ed = getEditorFromTabWidget(i);
        if (ed) {
            ed->clearErrorHighlight();
        }
    }

    if (isHtmlFile(filePath)) {
        if (openInSystemDefault(filePath)) {
            statusBar()->showMessage(tr("HTML открыт в системном браузере"), 3000);
        } else {
            statusBar()->showMessage(tr("Не удалось открыть HTML в системном браузере"), 3000);
        }
        return;
    }

    const QString python = detectPythonExecutable();
    if (python.isEmpty()) {
        QMessageBox::critical(this, tr("Python не найден"), tr("Не удалось найти интерпретатор Python."));
        return;
    }

    m_runningFilePath = filePath;
    m_stderrBuffer.clear();
    m_stdoutBuffer.clear();

    QFileInfo fi(filePath);
    
    if (isConsoleVisible) {
        // Консоль видна - запускаем через консоль
        m_scriptRunningInConsole = true;
        
        // Создаем консоль, если она еще не создана (ленивая инициализация)
        if (!m_console) {
            m_console = new ConsoleWidget();
            m_console->setPythonPath(python);
            // Подключаем сигналы
            connect(m_console, &ConsoleWidget::outputReceived, this, [this](const QString &text, bool isError) {
                if (isError) {
                    m_stderrBuffer += text;
                } else {
                    m_stdoutBuffer += text;
                }
            });
            connect(m_console, &ConsoleWidget::commandFinished, this, [this](int exitCode) {
                onProcessFinished(exitCode, QProcess::NormalExit);
            });
            // Заменяем заглушку на реальный виджет консоли
            QWidget *placeholder = m_outputStack->widget(1);
            if (placeholder) {
                m_outputStack->removeWidget(placeholder);
                placeholder->deleteLater();
            }
            m_outputStack->insertWidget(1, m_console);
        } else {
            // Обновляем путь к Python в консоли (если изменился)
            m_console->setPythonPath(python);
        }
        
        // Устанавливаем рабочую директорию для консоли
        m_console->setWorkingDirectory(fi.absolutePath());
        
        // Формируем команду для запуска скрипта
        // Используем cd для смены директории перед запуском
        QString command = QString("cd /d \"%1\" && \"%2\" \"%3\" && echo [EXIT_CODE:0] || echo [EXIT_CODE:%ERRORLEVEL%]")
                          .arg(fi.absolutePath(), python, filePath);
        
        statusBar()->showMessage(tr("Выполняется..."));
        m_console->ensureStarted(); // Убеждаемся, что консоль запущена
        m_console->writeCommand(command);
    } else {
        // Вывод виден - запускаем через QProcess и выводим в QTextBrowser
        m_scriptRunningInConsole = false;
        
        if (m_output) {
            m_output->clear();
        }
        
        m_process = new QProcess(this);
        connect(m_process, &QProcess::started, this, &MainWindow::onProcessStarted);
        connect(m_process, &QProcess::readyReadStandardOutput, this, &MainWindow::onProcessStdout);
        connect(m_process, &QProcess::readyReadStandardError, this, &MainWindow::onProcessStderr);
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &MainWindow::onProcessFinished);

        m_process->setProgram(python);
        
        // Обертываем скрипт в код, который выводит полный traceback для всех ошибок
        QString escapedPath = filePath;
        // Для raw string r'...' экранируем только одинарные кавычки
        escapedPath.replace('\'', "\\'");
        
        QString wrapperScript = QString(
            "import sys\n"
            "import traceback\n"
            "import os\n"
            "\n"
            "# Устанавливаем excepthook для вывода полного traceback\n"
            "def excepthook(exc_type, exc_value, exc_traceback):\n"
            "    if exc_type != SystemExit:\n"
            "        traceback.print_exception(exc_type, exc_value, exc_traceback, file=sys.stderr)\n"
            "    sys.__excepthook__(exc_type, exc_value, exc_traceback)\n"
            "\n"
            "sys.excepthook = excepthook\n"
            "\n"
            "# Путь к файлу\n"
            "script_path = r'%1'\n"
            "\n"
            "# Устанавливаем __file__ для совместимости\n"
            "globals_dict = {\n"
            "    '__name__': '__main__',\n"
            "    '__file__': script_path,\n"
            "    '__builtins__': __builtins__,\n"
            "}\n"
            "\n"
            "try:\n"
            "    # Читаем и выполняем файл (работает с UNC путями)\n"
            "    with open(script_path, 'r', encoding='utf-8') as f:\n"
            "        code = compile(f.read(), script_path, 'exec')\n"
            "        exec(code, globals_dict)\n"
            "except SystemExit:\n"
            "    raise\n"
            "except Exception as e:\n"
            "    # Выводим полный traceback для всех исключений\n"
            "    traceback.print_exc()\n"
            "    sys.exit(1)\n"
        ).arg(escapedPath);
        
        m_process->setArguments(QStringList() << "-c" << wrapperScript);
        m_process->setWorkingDirectory(fi.absolutePath());
        m_process->setProcessChannelMode(QProcess::SeparateChannels);
        
        // Устанавливаем переменные окружения для корректной работы с кириллицей
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("PYTHONIOENCODING", "utf-8");  // Принудительно UTF-8 для ввода-вывода
        env.insert("PYTHONUNBUFFERED", "1");      // Отключаем буферизацию
        m_process->setProcessEnvironment(env);
        
        // Показываем поле ввода когда процесс запущен
        if (m_outputInput && !m_outputModeIsConsole) {
            m_outputInput->setVisible(true);
        }
        
        m_process->start();
    }
}

void MainWindow::runScriptInTerminal() {
    CodeEditor *editor = currentEditor();
    if (!editor) return;
    
    QString filePath = m_editorToPath.value(editor, QString());
    
    // Если файл не сохранен, сохраняем его во временную папку
    if (filePath.isEmpty()) {
        filePath = saveUnsavedToTemp(editor);
        if (filePath.isEmpty()) {
            QMessageBox::critical(this, tr("Ошибка"), tr("Не удалось сохранить файл во временную папку."));
            return;
        }
    } else if (editor->document()->isModified()) {
        // Файл сохранен, но есть несохраненные изменения - сохраняем их
        QFile f(filePath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::critical(this, tr("Ошибка записи"), 
                tr("Не удалось сохранить файл %1: %2").arg(filePath, f.errorString()));
            return;
        }
        QTextStream ts(&f);
        ts.setCodec("UTF-8");
        ts << editor->toPlainText();
        f.close();
        editor->document()->setModified(false);
        updateTabText(editor);
    }
    
    // Для HTML файлов открываем в системном браузере
    if (isHtmlFile(filePath)) {
        if (openInSystemDefault(filePath)) {
            statusBar()->showMessage(tr("HTML открыт в системном браузере"), 3000);
        } else {
            statusBar()->showMessage(tr("Не удалось открыть HTML в системном браузере"), 3000);
        }
        return;
    }
    
    const QString python = detectPythonExecutable();
    if (python.isEmpty()) {
        QMessageBox::critical(this, tr("Python не найден"), tr("Не удалось найти интерпретатор Python."));
        return;
    }
    
    QFileInfo fi(filePath);
    QString workingDir = fi.absolutePath();
    
    // Запускаем в отдельном окне консоли
#ifdef _WIN32
    // На Windows используем CreateProcess для создания дочернего процесса с консолью
    QString command = QString("cd /d \"%1\" && \"%2\" \"%3\" && pause").arg(workingDir, python, filePath);
    
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOW;
    
    PROCESS_INFORMATION pi = {0};
    
    // Формируем командную строку - используем /C чтобы окно закрылось после выполнения команды (включая pause)
    QString cmdLine = QString("cmd.exe /C \"%1\"").arg(command);
    std::wstring wCmdLine = cmdLine.toStdWString();
    std::vector<wchar_t> cmdLineBuf(wCmdLine.length() + 1);
    wcscpy(cmdLineBuf.data(), wCmdLine.c_str());
    
    // Преобразуем рабочую директорию в wchar_t
    std::wstring wWorkingDir = workingDir.toStdWString();
    std::vector<wchar_t> workingDirBuf(wWorkingDir.length() + 1);
    wcscpy(workingDirBuf.data(), wWorkingDir.c_str());
    
    // Создаем процесс с новой консолью
    BOOL success = CreateProcessW(
        NULL,                    // Имя приложения (NULL - берется из командной строки)
        cmdLineBuf.data(),       // Командная строка (должна быть изменяемой)
        NULL,                    // Атрибуты безопасности процесса
        NULL,                    // Атрибуты безопасности потока
        FALSE,                   // Наследование дескрипторов
        CREATE_NEW_CONSOLE,      // Флаги создания (новая консоль)
        NULL,                    // Блок окружения
        workingDirBuf.data(),    // Текущая директория
        &si,                     // Информация о запуске
        &pi                      // Информация о процессе
    );
    
    if (!success) {
        DWORD error = GetLastError();
        QMessageBox::warning(this, tr("Ошибка"), tr("Не удалось запустить консоль. Код ошибки: %1").arg(error));
        return;
    }
    
    // Закрываем handle потока (он нам не нужен)
    CloseHandle(pi.hThread);
    
    // Сохраняем handle процесса для завершения при закрытии IDE
    QProcess *terminalProcess = new QProcess(this);
    terminalProcess->setProperty("processHandle", reinterpret_cast<quintptr>(pi.hProcess));
    m_terminalProcesses.append(terminalProcess);
    
    // Подключаем таймер для проверки завершения процесса
    QTimer *checkTimer = new QTimer(this);
    checkTimer->setSingleShot(false);
    checkTimer->setInterval(1000); // Проверяем каждую секунду
    connect(checkTimer, &QTimer::timeout, this, [this, terminalProcess, pi, checkTimer]() {
        DWORD exitCode;
        if (GetExitCodeProcess(pi.hProcess, &exitCode)) {
            if (exitCode != STILL_ACTIVE) {
                // Процесс завершился
                CloseHandle(pi.hProcess);
                m_terminalProcesses.removeAll(terminalProcess);
                terminalProcess->deleteLater();
                checkTimer->stop();
                checkTimer->deleteLater();
            }
        } else {
            // Ошибка при проверке - возможно процесс уже завершен
            CloseHandle(pi.hProcess);
            m_terminalProcesses.removeAll(terminalProcess);
            terminalProcess->deleteLater();
            checkTimer->stop();
            checkTimer->deleteLater();
        }
    });
    checkTimer->start();
#else
    // На Linux/Mac используем терминал
    QString terminal = qEnvironmentVariable("TERMINAL", "xterm");
    QString command = QString("cd '%1' && '%2' '%3' && read -p 'Press Enter to continue...'").arg(workingDir, python, filePath);
    QStringList arguments;
    arguments << "-e" << "sh" << "-c" << command;
    QProcess *terminalProcess = new QProcess(this);
    terminalProcess->start(terminal, arguments);
    m_terminalProcesses.append(terminalProcess);
    connect(terminalProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, terminalProcess](int, QProcess::ExitStatus) {
                m_terminalProcesses.removeAll(terminalProcess);
                terminalProcess->deleteLater();
            });
#endif
    
    statusBar()->showMessage(tr("Скрипт запущен в отдельном окне консоли"), 2000);
}

void MainWindow::terminateRun() {
    if (TurtleTaskWidget *task = currentTurtleTask()) {
        if (task->isRunning()) {
            task->stop();
            statusBar()->showMessage(tr("Выполнение остановлено"), 2000);
            return;
        }
    }

    // Определяем, какой виджет сейчас виден
    bool isConsoleVisible = (m_outputStack && m_outputStack->currentIndex() == 1);

    if (m_embeddedBrowserProcess && m_embeddedBrowserProcess->state() != QProcess::NotRunning) {
        m_embeddedBrowserProcess->terminate();
        if (!m_embeddedBrowserProcess->waitForFinished(1000)) {
            m_embeddedBrowserProcess->kill();
        }
        m_embeddedBrowserProcess->deleteLater();
        m_embeddedBrowserProcess = nullptr;
        statusBar()->showMessage(tr("Встроенный браузер закрыт"), 2000);
        return;
    }

    if (isConsoleVisible && m_console && m_console->isRunning()) {
        m_console->terminate();
        statusBar()->showMessage(tr("Выполнение остановлено"), 2000);
    } else if (!isConsoleVisible && m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(1000))
            m_process->kill();
        statusBar()->showMessage(tr("Выполнение остановлено"), 2000);
    }
    
    // Сбрасываем флаг отладки
    m_isDebugging = false;
    m_currentBreakpointLine = -1;
    
    // Отменяем регистрацию глобального хоткея F9
#ifdef _WIN32
    if (m_hotkeyRegistered) {
        HWND hwnd = reinterpret_cast<HWND>(winId());
        if (hwnd) {
            UnregisterHotKey(hwnd, HOTKEY_ID);
        }
        m_hotkeyRegistered = false;
    }
#endif
}

void MainWindow::runScriptWithDebug() {
    CodeEditor *editor = currentEditor();
    if (!editor) return;
    
    QString filePath = m_editorToPath.value(editor, QString());
    
    // Если файл не сохранен, сохраняем его во временную папку
    if (filePath.isEmpty()) {
        filePath = saveUnsavedToTemp(editor);
        if (filePath.isEmpty()) {
            QMessageBox::critical(this, tr("Ошибка"), tr("Не удалось сохранить файл во временную папку."));
            return;
        }
    } else if (editor->document()->isModified()) {
        // Файл сохранен, но есть несохраненные изменения - сохраняем их
        QFile f(filePath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::critical(this, tr("Ошибка записи"), 
                tr("Не удалось сохранить файл %1: %2").arg(filePath, f.errorString()));
            return;
        }
        QTextStream ts(&f);
        ts.setCodec("UTF-8");
        ts << editor->toPlainText();
        f.close();
        editor->document()->setModified(false);
        updateTabText(editor);
    }
    
    // Собираем точки останова из редактора
    m_debugBreakpoints = editor->breakpoints();
    if (m_debugBreakpoints.isEmpty()) {
        QMessageBox::information(this, tr("Отладка"), tr("Установите точки останова перед запуском отладки."));
        return;
    }
    
    // Проверяем, запущен ли скрипт
    if (m_process && m_process->state() != QProcess::NotRunning) {
        QMessageBox::information(this, tr("Запуск"), tr("Скрипт уже выполняется."));
        return;
    }
    
    if (isHtmlFile(filePath)) {
        QMessageBox::information(this, tr("Отладка"), tr("Нельзя отлаживать HTML-файлы."));
        return;
    }

    // Очищаем подсветку ошибки во всех редакторах
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        CodeEditor *ed = getEditorFromTabWidget(i);
        if (ed) {
            ed->clearErrorHighlight();
        }
    }
    
    const QString python = detectPythonExecutable();
    if (python.isEmpty()) {
        QMessageBox::critical(this, tr("Python не найден"), tr("Не удалось найти интерпретатор Python."));
        return;
    }
    
    m_runningFilePath = filePath;
    m_stderrBuffer.clear();
    m_stdoutBuffer.clear();
    m_isDebugging = true;
    m_currentBreakpointLine = -1;
    
    QFileInfo fi(filePath);
    
    // Переключаемся на вывод (не консоль)
    if (m_outputStack) {
        m_outputStack->setCurrentIndex(0);
        m_outputModeIsConsole = false;
    }
    
    if (m_output) {
        m_output->clear();
    }
    
    m_process = new QProcess(this);
    connect(m_process, &QProcess::started, this, &MainWindow::onProcessStarted);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &MainWindow::onProcessStdout);
    connect(m_process, &QProcess::readyReadStandardError, this, &MainWindow::onProcessStderr);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onProcessFinished);
    
    m_process->setProgram(python);
    
    // Создаем временный файл для синхронизации
    QString syncFile = QDir::temp().absoluteFilePath("vuzhyk_debug_sync_" + QString::number(QCoreApplication::applicationPid()) + ".txt");
    QFile::remove(syncFile); // Удаляем старый файл, если есть
    
    // Формируем отладочный скрипт
    QString escapedPath = filePath;
    // Для raw string r'...' экранируем только одинарные кавычки
    escapedPath.replace('\'', "\\'");
    
    QString escapedSyncFile = syncFile;
    // Для raw string r'...' экранируем только одинарные кавычки
    escapedSyncFile.replace('\'', "\\'");
    
    // Формируем список точек останова для Python
    QStringList breakpointLines;
    for (int line : m_debugBreakpoints) {
        breakpointLines << QString::number(line + 1); // Python использует 1-based индексацию
    }
    QString breakpointsStr = "{" + breakpointLines.join(", ") + "}";
    
    QString debugScript = QString(
        "import sys\n"
        "import traceback\n"
        "import os\n"
        "import time\n"
        "\n"
        "# Отключаем буферизацию для немедленного вывода\n"
        "sys.stdout.reconfigure(line_buffering=True)\n"
        "sys.stderr.reconfigure(line_buffering=True)\n"
        "\n"
        "# Точки останова\n"
        "breakpoints = %1\n"
        "sync_file = r'%2'\n"
        "current_file = r'%3'\n"
        "\n"
        "# Функция trace для остановки на точках останова\n"
        "def trace_debug(frame, event, arg):\n"
        "    if event == 'line':\n"
        "        filename = frame.f_code.co_filename\n"
        "        lineno = frame.f_lineno\n"
        "        # Нормализуем пути для сравнения (абсолютные пути, одинаковые разделители)\n"
        "        # os уже импортирован в начале скрипта\n"
        "        normalized_filename = os.path.normpath(os.path.abspath(filename))\n"
        "        normalized_current_file = os.path.normpath(os.path.abspath(current_file))\n"
        "        # Проверяем, совпадает ли файл и номер строки с точкой останова\n"
        "        if normalized_filename == normalized_current_file and lineno in breakpoints:\n"
        "            # Принудительно выводим буфер перед остановкой\n"
        "            sys.stdout.flush()\n"
        "            sys.stderr.flush()\n"
        "            # Записываем информацию о точке останова в файл\n"
        "            with open(sync_file, 'w') as f:\n"
        "                f.write(f'BREAKPOINT:{lineno}\\n')\n"
        "            # Ждем, пока файл не будет удален (сигнал продолжения)\n"
        "            # Используем неблокирующий механизм для GUI приложений\n"
        "            import threading\n"
        "            import queue\n"
        "            \n"
        "            # Пытаемся обработать события GUI, если это GUI приложение\n"
        "            try:\n"
        "                # Для tkinter\n"
        "                import tkinter as tk\n"
        "                root = tk._default_root\n"
        "                if root:\n"
        "                    while os.path.exists(sync_file):\n"
        "                        root.update_idletasks()\n"
        "                        time.sleep(0.01)\n"
        "                    # После продолжения добавляем перенос строки для корректного форматирования вывода\n"
        "                    sys.stdout.write('\\n')\n"
        "                    sys.stdout.flush()\n"
        "                    return trace_debug\n"
        "            except:\n"
        "                pass\n"
        "            \n"
        "            try:\n"
        "                # Для PyQt/PySide\n"
        "                from PyQt5.QtWidgets import QApplication\n"
        "                app = QApplication.instance()\n"
        "                if app:\n"
        "                    while os.path.exists(sync_file):\n"
        "                        app.processEvents()\n"
        "                        time.sleep(0.01)\n"
        "            except:\n"
        "                pass\n"
        "            \n"
        "            try:\n"
        "                # Для PySide2\n"
        "                from PySide2.QtWidgets import QApplication\n"
        "                app = QApplication.instance()\n"
        "                if app:\n"
        "                    while os.path.exists(sync_file):\n"
        "                        app.processEvents()\n"
        "                        time.sleep(0.01)\n"
        "                    # После продолжения добавляем перенос строки для корректного форматирования вывода\n"
        "                    sys.stdout.write('\\n')\n"
        "                    sys.stdout.flush()\n"
        "                    return trace_debug\n"
        "            except:\n"
        "                pass\n"
        "            \n"
        "            try:\n"
        "                # Для PySide6\n"
        "                from PySide6.QtWidgets import QApplication\n"
        "                app = QApplication.instance()\n"
        "                if app:\n"
        "                    while os.path.exists(sync_file):\n"
        "                        app.processEvents()\n"
        "                        time.sleep(0.01)\n"
        "                    # После продолжения добавляем перенос строки для корректного форматирования вывода\n"
        "                    sys.stdout.write('\\n')\n"
        "                    sys.stdout.flush()\n"
        "                    return trace_debug\n"
        "            except:\n"
        "                pass\n"
        "            \n"
        "            try:\n"
        "                # Для PyQt6\n"
        "                from PyQt6.QtWidgets import QApplication\n"
        "                app = QApplication.instance()\n"
        "                if app:\n"
        "                    while os.path.exists(sync_file):\n"
        "                        app.processEvents()\n"
        "                        time.sleep(0.01)\n"
        "                    # После продолжения добавляем перенос строки для корректного форматирования вывода\n"
        "                    sys.stdout.write('\\n')\n"
        "                    sys.stdout.flush()\n"
        "                    return trace_debug\n"
        "            except:\n"
        "                pass\n"
        "            \n"
        "            # Если это не GUI приложение, используем обычное ожидание\n"
        "            # Но все равно периодически выводим буфер\n"
        "            while os.path.exists(sync_file):\n"
        "                sys.stdout.flush()\n"
        "                sys.stderr.flush()\n"
        "                time.sleep(0.1)\n"
        "            \n"
        "            # После продолжения (когда файл удален) всегда добавляем перенос строки\n"
        "            # Это гарантирует, что вывод начнется с новой строки после каждого брейкпоинта\n"
        "            sys.stdout.write('\\n')\n"
        "            sys.stdout.flush()\n"
        "    return trace_debug\n"
        "\n"
        "# Устанавливаем excepthook для вывода полного traceback\n"
        "def excepthook(exc_type, exc_value, exc_traceback):\n"
        "    if exc_type != SystemExit:\n"
        "        traceback.print_exception(exc_type, exc_value, exc_traceback, file=sys.stderr)\n"
        "    sys.__excepthook__(exc_type, exc_value, exc_traceback)\n"
        "\n"
        "sys.excepthook = excepthook\n"
        "sys.settrace(trace_debug)\n"
        "\n"
        "# Устанавливаем глобальные переменные для исполняемого скрипта\n"
        "globals_dict = {\n"
        "    '__name__': '__main__',\n"
        "    '__file__': current_file,\n"
        "    '__builtins__': __builtins__,\n"
        "}\n"
        "\n"
        "try:\n"
        "    # Читаем и выполняем файл (работает с UNC путями)\n"
        "    with open(current_file, 'r', encoding='utf-8') as f:\n"
        "        code = compile(f.read(), current_file, 'exec')\n"
        "        exec(code, globals_dict)\n"
        "except SystemExit:\n"
        "    raise\n"
        "except Exception as e:\n"
        "    traceback.print_exc()\n"
        "    sys.exit(1)\n"
        "finally:\n"
        "    # Принудительно выводим оставшийся буфер\n"
        "    sys.stdout.flush()\n"
        "    sys.stderr.flush()\n"
        "    # Удаляем файл синхронизации при завершении\n"
        "    if os.path.exists(sync_file):\n"
        "        os.remove(sync_file)\n"
    ).arg(breakpointsStr, escapedSyncFile, escapedPath);
    
    // Используем флаг -u для отключения буферизации вывода
    m_process->setArguments(QStringList() << "-u" << "-c" << debugScript);
    m_process->setWorkingDirectory(fi.absolutePath());
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    
    // Устанавливаем переменные окружения для корректной работы с кириллицей и отладки
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");  // Принудительно UTF-8 для ввода-вывода (исправляет UnicodeEncodeError)
    env.insert("PYTHONUNBUFFERED", "1");      // Отключаем буферизацию
    m_process->setProcessEnvironment(env);
    
    // Сохраняем путь к файлу синхронизации для continueDebug
    m_debugSyncFile = syncFile;
    
    // Запускаем таймер для проверки файла синхронизации
    if (!m_debugTimer) {
        m_debugTimer = new QTimer(this);
        connect(m_debugTimer, &QTimer::timeout, this, &MainWindow::checkDebugSyncFile);
    }
    m_debugTimer->start(100); // Проверяем каждые 100 мс
    
    // Регистрируем глобальный хоткей F9
#ifdef _WIN32
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd && !m_hotkeyRegistered) {
        // VK_F9 = 0x78 (120)
        if (RegisterHotKey(hwnd, HOTKEY_ID, 0, 0x78)) {
            m_hotkeyRegistered = true;
        }
    }
#endif
    
    // Показываем поле ввода для отладки
    if (m_outputInput && !m_outputModeIsConsole) {
        m_outputInput->setVisible(true);
    }
    
    m_process->start();
    statusBar()->showMessage(tr("Отладка запущена. Нажмите F9 для продолжения."));
}

void MainWindow::continueDebug() {
    if (!m_isDebugging || m_debugSyncFile.isEmpty()) {
        return;
    }
    
    // Удаляем файл синхронизации, чтобы продолжить выполнение
    if (QFile::exists(m_debugSyncFile)) {
        QFile::remove(m_debugSyncFile);
    }
    
    statusBar()->showMessage(tr("Продолжение выполнения..."));
}

void MainWindow::onOutputInputReturnPressed() {
    if (!m_outputInput || !m_process || m_process->state() != QProcess::Running) {
        return;
    }
    
    QString text = m_outputInput->text();
    
    // Показываем введенный текст в выводе в новой строке
    if (m_output) {
        // Перемещаем курсор в конец и добавляем новую строку
        QTextCursor cursor = m_output->textCursor();
        cursor.movePosition(QTextCursor::End);
        // Убеждаемся, что мы на новой строке
        if (!cursor.atBlockStart()) {
            cursor.insertText("\n");
        }
        // Добавляем введенный текст
        cursor.insertText(text);
        // Добавляем перенос строки после введенного текста
        cursor.insertText("\n");
        cursor.movePosition(QTextCursor::End);
        m_output->setTextCursor(cursor);
        // Прокручиваем к концу
        QTextCursor scrollCursor = m_output->textCursor();
        scrollCursor.movePosition(QTextCursor::End);
        m_output->setTextCursor(scrollCursor);
    }
    
    if (text.isEmpty()) {
        // Если пусто, отправляем просто перевод строки
        m_process->write("\n");
    } else {
        // Отправляем текст с переводом строки
        m_process->write((text + "\n").toUtf8());
    }
    
    // Очищаем поле ввода
    m_outputInput->clear();
}

void MainWindow::checkDebugSyncFile() {
    if (!m_isDebugging || m_debugSyncFile.isEmpty()) {
        if (m_debugTimer) {
            m_debugTimer->stop();
        }
        return;
    }
    
    // Проверяем файл синхронизации
    if (QFile::exists(m_debugSyncFile)) {
        QFile file(m_debugSyncFile);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            QString line = in.readLine();
            if (line.startsWith("BREAKPOINT:")) {
                int lineNumber = line.mid(11).toInt() - 1; // Конвертируем в 0-based
                m_currentBreakpointLine = lineNumber;
                
                // Подсвечиваем строку в редакторе
                CodeEditor *editor = currentEditor();
                if (editor) {
                    // QScintilla использует номера строк напрямую (0-based)
                    if (lineNumber >= 0 && lineNumber < editor->lines()) {
                        editor->setCursorPosition(lineNumber, 0);
                        editor->ensureLineVisible(lineNumber);
                    }
                }
                
                statusBar()->showMessage(tr("Остановка на строке %1. Нажмите F9 для продолжения.").arg(lineNumber + 1));
            }
            file.close();
        }
    }
    
    // Проверяем, завершился ли процесс
    if (m_process && m_process->state() == QProcess::NotRunning) {
        m_isDebugging = false;
        m_currentBreakpointLine = -1;
        if (m_debugTimer) {
            m_debugTimer->stop();
        }
        if (QFile::exists(m_debugSyncFile)) {
            QFile::remove(m_debugSyncFile);
        }
        m_debugSyncFile.clear();
        
        // Отменяем регистрацию глобального хоткея
#ifdef _WIN32
        if (m_hotkeyRegistered) {
            HWND hwnd = reinterpret_cast<HWND>(winId());
            if (hwnd) {
                UnregisterHotKey(hwnd, HOTKEY_ID);
            }
            m_hotkeyRegistered = false;
        }
#endif
    }
}

void MainWindow::choosePython() {
    const QString startDir = QFileInfo(configuredPythonPath()).absolutePath();
    QString selectedPath = showFilePicker(
        tr("Исполняемые (*.exe);;Все файлы (*.*)"),
        startDir,
        false,
        false
    );
    
    if (selectedPath.isEmpty()) return;
    QSettings s;
    s.beginGroup(SETTINGS_GROUP);
    s.setValue(SETTINGS_PYTHON_PATH, selectedPath);
    s.endGroup();
    statusBar()->showMessage(tr("Выбран Python: %1").arg(selectedPath), 3000);
}

void MainWindow::openProjectFolder() {
    QString selectedDir = showFilePicker(
        QString(),
        m_fsModel ? m_fsModel->rootPath() : QDir::currentPath(),
        false,
        true
    );
    
    if (selectedDir.isEmpty()) return;
    ensureProjectDock();
    m_fsModel->setRootPath(selectedDir);
    m_fsView->setRootIndex(m_fsModel->index(selectedDir));
    if (m_projectDock)
        m_projectDock->show();
}

void MainWindow::startRepl() {
    ensureReplDock();
    if (m_replProcess && m_replProcess->state() != QProcess::NotRunning) {
        statusBar()->showMessage(tr("REPL уже запущен"), 2000);
        return;
    }
    const QString python = detectPythonExecutable();
    if (python.isEmpty()) {
        QMessageBox::critical(this, tr("Python не найден"), tr("Не удалось найти интерпретатор Python."));
        return;
    }
    m_replOutput->clear();
    m_replHistory.clear();
    m_replHistoryIndex = -1;

    m_replProcess = new QProcess(this);
    connect(m_replProcess, &QProcess::started, this, &MainWindow::onReplStarted);
    connect(m_replProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::onReplStdout);
    connect(m_replProcess, &QProcess::readyReadStandardError, this, &MainWindow::onReplStderr);
    connect(m_replProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onReplFinished);

    m_replProcess->setProgram(python);
    m_replProcess->setArguments(QStringList() << "-i");
    m_replProcess->setProcessChannelMode(QProcess::SeparateChannels);
    m_replProcess->start();
    if (m_replDock) m_replDock->show();
}

void MainWindow::stopRepl() {
    if (m_replProcess && m_replProcess->state() != QProcess::NotRunning) {
        m_replProcess->terminate();
        if (!m_replProcess->waitForFinished(1000))
            m_replProcess->kill();
    }
    if (m_replDock)
        m_replDock->hide();
}

void MainWindow::sendReplInput() {
    if (!m_replProcess || m_replProcess->state() == QProcess::NotRunning) {
        statusBar()->showMessage(tr("REPL не запущен"), 2000);
        return;
    }
    const QString line = m_replInput->toPlainText();
    if (line.isEmpty()) return;
    m_replHistory << line;
    // Ограничиваем историю до 1000 записей для экономии памяти
    const int MAX_HISTORY_SIZE = 1000;
    if (m_replHistory.size() > MAX_HISTORY_SIZE) {
        m_replHistory.removeFirst();
    }
    m_replHistoryIndex = m_replHistory.size();
    m_replProcess->write(line.toUtf8());
    m_replProcess->write("\n");
    m_replInput->clear();
}

void MainWindow::onProcessStarted() {
    statusBar()->showMessage(tr("Выполняется..."));
    
    // Показываем поле ввода когда процесс запущен
    if (m_outputInput && !m_outputModeIsConsole) {
        m_outputInput->setVisible(true);
        m_outputInput->setFocus();
    }
}

void MainWindow::onProcessStdout() {
    QByteArray data = m_process->readAllStandardOutput();
    // Пробуем разные кодировки для правильного отображения
    QString text;
    text = QString::fromUtf8(data);
    if (text.isEmpty() || text.contains(QChar::ReplacementCharacter)) {
        text = QString::fromLocal8Bit(data);
    }
    
    if (!text.isEmpty()) {
        // Ограничиваем размер буфера для экономии памяти
        const int MAX_BUFFER_SIZE = 100000; // Максимум ~100К символов
        if (m_stdoutBuffer.length() > MAX_BUFFER_SIZE) {
            m_stdoutBuffer = m_stdoutBuffer.right(80000); // Оставляем последние 80К
        }
        m_stdoutBuffer += text;
        appendOutput(text, false);
    }
}

void MainWindow::onProcessStderr() {
    QByteArray data = m_process->readAllStandardError();
    // Пробуем разные кодировки для правильного отображения
    QString text;
    text = QString::fromUtf8(data);
    if (text.isEmpty() || text.contains(QChar::ReplacementCharacter)) {
        text = QString::fromLocal8Bit(data);
    }
    
    if (!text.isEmpty()) {
        // Ограничиваем размер буфера для экономии памяти
        const int MAX_BUFFER_SIZE = 100000; // Максимум ~100К символов
        if (m_stderrBuffer.length() > MAX_BUFFER_SIZE) {
            m_stderrBuffer = m_stderrBuffer.right(80000); // Оставляем последние 80К
        }
        m_stderrBuffer += text;
        appendOutput(text, true);
    }
}

void MainWindow::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(status);
    
    // Скрываем поле ввода когда процесс завершен
    if (m_outputInput) {
        m_outputInput->setVisible(false);
        m_outputInput->clear();
    }
    
    // Очищаем отладку, если она была активна
    if (m_isDebugging) {
        m_isDebugging = false;
        m_currentBreakpointLine = -1;
        if (m_debugTimer) {
            m_debugTimer->stop();
        }
        if (QFile::exists(m_debugSyncFile)) {
            QFile::remove(m_debugSyncFile);
        }
        m_debugSyncFile.clear();
        
        // Отменяем регистрацию глобального хоткея
#ifdef _WIN32
        if (m_hotkeyRegistered) {
            HWND hwnd = reinterpret_cast<HWND>(winId());
            if (hwnd) {
                UnregisterHotKey(hwnd, HOTKEY_ID);
            }
            m_hotkeyRegistered = false;
        }
#endif
    }
    
    statusBar()->showMessage(tr("Готов"));
    
    // Выводим сообщение о завершении только если скрипт был запущен через QProcess (не через консоль)
    if (!m_scriptRunningInConsole) {
        appendOutput(QString("\n[Process exited with code %1]\n").arg(exitCode), exitCode != 0);
    }
    
    // Парсим ошибки и подсвечиваем строку с ошибкой
    // Проверяем как stderr, так и stdout (ошибки могут выводиться через print)
    QString allErrors = m_stderrBuffer + m_stdoutBuffer;
    if (!allErrors.isEmpty()) {
        // Проверяем наличие сообщений об ошибках
        bool hasError = allErrors.contains("Error:") || 
                       allErrors.contains("Ошибка:") ||
                       allErrors.contains("FileNotFoundError") ||
                       allErrors.contains("IndexError") ||
                       allErrors.contains("KeyError") ||
                       allErrors.contains("ZeroDivisionError") ||
                       allErrors.contains("TypeError") ||
                       allErrors.contains("ValueError") ||
                       allErrors.contains("division by zero") ||
                       allErrors.contains("Traceback") ||
                       exitCode != 0;
        
        if (hasError) {
            parseErrorAndHighlight(allErrors);
        }
    }
    
    m_stderrBuffer.clear();
    m_stdoutBuffer.clear();
    m_runningFilePath.clear();
    m_scriptRunningInConsole = false; // Сбрасываем флаг
}

void MainWindow::onReplStarted() {
    statusBar()->showMessage(tr("REPL запущен"), 2000);
}

void MainWindow::onReplStdout() {
    const QString text = QString::fromLocal8Bit(m_replProcess->readAllStandardOutput());
    appendReplOutput(text, false);
}

void MainWindow::onReplStderr() {
    const QString text = QString::fromLocal8Bit(m_replProcess->readAllStandardError());
    appendReplOutput(text, true);
}

void MainWindow::onReplFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(status);
    appendReplOutput(QString("\n[REPL exited with code %1]\n").arg(exitCode), exitCode != 0);
}

bool MainWindow::maybeSave() {
    CodeEditor *editor = currentEditor();
    if (!editor || !editor->document()->isModified())
        return true;
    
    // Проверяем, является ли файл пустым, новым и начальным файлом при запуске - если да, не показываем диалог
    QString filePath = m_editorToPath.value(editor, QString());
    bool isEmpty = editor->toPlainText().trimmed().isEmpty();
    bool isNewFile = filePath.isEmpty();
    bool isInitialFile = editor->property("isInitialFile").toBool();
    
    // Если файл пустой, новый и является начальным файлом при запуске, не показываем диалог сохранения
    if (isEmpty && isNewFile && isInitialFile) {
        return true; // Просто продолжаем без сохранения
    }
    
    QString fileName = filePath.isEmpty() ? tr("Новый файл") : QFileInfo(filePath).fileName();
    
    const auto ret = QMessageBox::warning(
        this, tr("Несохранённые изменения"),
        tr("Сохранить изменения в файле \"%1\"?").arg(fileName),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (ret == QMessageBox::Save) {
        if (filePath.isEmpty() || isTempPyrobSolutionPath(filePath)) {
            saveFileAs();
        } else {
            saveToPath(filePath);
        }
        return !editor->document()->isModified();
    }
    if (ret == QMessageBox::Cancel)
        return false;
    return true;
}

bool MainWindow::saveToPath(const QString &path) {
    CodeEditor *editor = currentEditor();
    if (!editor) return false;
    
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, tr("Ошибка записи"), f.errorString());
        return false;
    }
    QTextStream ts(&f);
    ts.setCodec("UTF-8");
    ts << editor->toPlainText();
    f.close();
    editor->document()->setModified(false);
    
    // Обновляем вкладку
    int currentIndex = m_tabWidget->currentIndex();
    m_editorToPath[editor] = path;
    updateTabText(editor);
    
    updateWindowTitle();
    statusBar()->showMessage(tr("Сохранено: %1").arg(QDir::toNativeSeparators(path)), 3000);
    return true;
}

void MainWindow::openFileFromPath(const QString &path) {
    // Проверяем и закрываем пустой стартовый файл (как при открытии через меню)
    CodeEditor *editor = currentEditor();
    if (editor) {
        QString filePath = m_editorToPath.value(editor, QString());
        QString text = editor->toPlainText();
        bool isEmpty = text.trimmed().isEmpty();
        bool isNewFile = filePath.isEmpty();
        bool isInitialFile = editor->property("isInitialFile").toBool();
        
        // Если файл пустой, новый и является начальным файлом при запуске, закрываем его без диалога
        if (isEmpty && isNewFile && isInitialFile) {
            editor->document()->setModified(false);
            int currentIndex = m_tabWidget->currentIndex();
            if (currentIndex >= 0) {
                closeTab(currentIndex);
            }
        } else if (editor->document()->isModified()) {
            // Для непустых или сохраненных файлов показываем диалог только если файл изменен
            if (!maybeSave())
                return;
        }
    }
    
    loadFromPath(path);
}

void MainWindow::loadFromPath(const QString &path) {
    // Удаляем кавычки, если они есть
    QString cleanPath = path.trimmed();
    if (cleanPath.startsWith('"') && cleanPath.endsWith('"')) {
        cleanPath = cleanPath.mid(1, cleanPath.length() - 2);
    }
    
    // Нормализуем путь и проверяем существование
    QString normalizedPath = QDir::toNativeSeparators(QDir::cleanPath(cleanPath));
    
    // Преобразуем относительный путь в абсолютный, если нужно
    QFileInfo fileInfo(normalizedPath);
    if (!fileInfo.isAbsolute()) {
        // Если путь относительный, пытаемся сделать его абсолютным относительно текущей директории
        normalizedPath = QDir::current().absoluteFilePath(normalizedPath);
        fileInfo = QFileInfo(normalizedPath);
    }
    
    // Проверяем существование файла
    if (!fileInfo.exists()) {
        // Показываем более подробное сообщение об ошибке
        QMessageBox::critical(this, tr("Файл не найден"), 
            tr("Файл не существует:\n%1\n\nИсходный путь: %2").arg(normalizedPath, path));
        return;
    }
    
    if (!fileInfo.isFile()) {
        QMessageBox::critical(this, tr("Ошибка"), 
            tr("Указанный путь не является файлом:\n%1").arg(normalizedPath));
        return;
    }
    
    QFile f(normalizedPath);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Ошибка чтения"), 
            tr("Не удалось открыть файл:\n%1\n\n%2").arg(normalizedPath, f.errorString()));
        return;
    }
    QTextStream ts(&f);
    ts.setCodec("UTF-8");
    const QString text = ts.readAll();
    f.close();
    
    // Ограничиваем количество открытых вкладок для экономии памяти
    const int MAX_TABS = 50;
    int codeEditorCount = 0;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        CodeEditor *editor = getEditorFromTabWidget(i);
        if (editor) codeEditorCount++;
    }
    if (codeEditorCount >= MAX_TABS) {
        QMessageBox::warning(this, tr("Превышен лимит"), 
            tr("Достигнут максимум %1 открытых вкладок редактора. Закройте некоторые вкладки перед открытием новых.").arg(MAX_TABS));
        return;
    }
    
    // Создаем новую вкладку с обёрткой для анимации
    QWidget *container = new QWidget(m_tabWidget);
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    CodeEditor *editor = new CodeEditor(container);
    setupEditor(editor);
    editor->setFileExtension(fileInfo.suffix());
    editor->setPlainText(text);
    editor->document()->setModified(false);
    layout->addWidget(editor);
    
    // Убеждаемся, что контейнер и редактор правильно растягиваются
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    editor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    int index = m_tabWidget->addTab(container, fileInfo.fileName());
    m_editorToPath[editor] = normalizedPath;
    // Не переключаемся на вкладку сразу при анимации — иначе контент мелькнёт на месте; анимация переключит в конце
    if (m_isInitializing) {
        m_tabWidget->setCurrentIndex(index);
    } else {
        container->hide(); // Скрываем до старта анимации, чтобы контент не мелькал на месте текущей вкладки
    }
    
    // Анимируем появление вкладки только если это не инициализация
    if (!m_isInitializing) {
        animateTabOpening(container, index);
    }
    
    // Обновляем автодополнение после загрузки содержимого файла
    // Используем QTimer для отложенного обновления, чтобы убедиться, что документ полностью загружен
    QTimer::singleShot(100, this, [this, editor]() {
        // Проверяем, что редактор все еще существует и лексер инициализирован
        if (editor && editor->lexer()) {
            try {
                updateCompletionFromDocument();
            } catch (...) {
                // Игнорируем ошибки при обновлении автокомплита
            }
        }
    });
    
    updateWindowTitle();
    statusBar()->showMessage(tr("Открыто: %1").arg(QDir::toNativeSeparators(normalizedPath)), 3000);
    refreshFileTypeActions();
}

QString MainWindow::embeddedPythonPath() const {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString candidate = QDir(appDir).filePath("Python38/python.exe");
    return QFile::exists(candidate) ? candidate : QString();
}

QString MainWindow::configuredPythonPath() const {
    QSettings s;
    s.beginGroup(SETTINGS_GROUP);
    const QString v = s.value(SETTINGS_PYTHON_PATH).toString();
    s.endGroup();
    return v;
}

QString MainWindow::detectPythonExecutable() const {
    QSettings s;
    s.beginGroup(SETTINGS_GROUP);
    const int type = s.value("interpreterType", 0).toInt(); // 0=встроенный, 1=установленный, 2=системный, 3=пользовательский
    const QString customPath = s.value(SETTINGS_PYTHON_PATH).toString();
    const QString selectedInterpreter = s.value("selectedInterpreter").toString();
    s.endGroup();

    if (type == 3) {
        if (QFile::exists(customPath)) {
            return customPath;
        }
        return {};
    }

    if (type == 1) {
        if (!selectedInterpreter.isEmpty() && QFile::exists(selectedInterpreter)) {
            return selectedInterpreter;
        }

        const QString interpretersDir = QDir(QCoreApplication::applicationDirPath()).filePath("interpreters");
        QDir dir(interpretersDir);
        if (dir.exists()) {
            const QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            if (!subdirs.isEmpty()) {
                const QString pythonExe = dir.filePath(subdirs.first() + "/python.exe");
                if (QFile::exists(pythonExe)) {
                    return pythonExe;
                }
            }
        }
    }

    if (type == 0 || type == 1) {
        const QString embedded = embeddedPythonPath();
        if (!embedded.isEmpty()) {
            return embedded;
        }
    }

    if (type == 2) {
        return QStringLiteral("python.exe");
    }

    return QStringLiteral("python.exe");
}

void MainWindow::onTabCloseRequested(int index) {
    closeTab(index);
}

void MainWindow::onTabChanged(int index) {
    // Обновляем индекс вкладки настроек, если она была перемещена
    if (m_settingsWidget && m_tabWidget) {
        for (int i = 0; i < m_tabWidget->count(); ++i) {
            if (m_tabWidget->widget(i) == m_settingsWidget) {
                m_settingsTabIndex = i;
                break;
            }
        }
    }
    
    // Обновляем индекс вкладки справки, если она была перемещена
    if (m_helpWidget && m_tabWidget) {
        for (int i = 0; i < m_tabWidget->count(); ++i) {
            if (m_tabWidget->widget(i) == m_helpWidget) {
                m_helpTabIndex = i;
                break;
            }
        }
    }
    
    // Обновляем предыдущий индекс
    // (анимация уже запущена из tabBarClicked, если переключение было по клику)
    m_previousTabIndex = index;
    
    // Периодически очищаем кэши Qt для предотвращения накопления памяти
    m_tabSwitchCount++;
    if (m_tabSwitchCount >= 10) { // Каждые 10 переключений
        m_tabSwitchCount = 0;
        
        // Очищаем кэш пиксельных карт
        QPixmapCache::clear();
        
        // Обрабатываем отложенные удаления
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    
    updateWindowTitle();
    updateCompletionFromDocument();
    refreshFileTypeActions();
}

void MainWindow::showTabContextMenu(const QPoint &pos) {
    QTabBar *tabBar = m_tabWidget->tabBar();
    int index = tabBar->tabAt(pos);
    if (index < 0) return;
    
    // Сохраняем индекс вкладки для сохранения
    int clickedTabIndex = index;
    
    // Если кликнули по вкладке настроек, показываем специальное меню
    if (clickedTabIndex == m_settingsTabIndex) {
        QMenu contextMenu(this);
        QAction *actClose = contextMenu.addAction(tr("Закрыть"));
        QAction *selectedAction = contextMenu.exec(tabBar->mapToGlobal(pos));
        if (selectedAction == actClose) {
            if (m_tabWidget->currentIndex() != clickedTabIndex) {
                m_tabWidget->setCurrentIndex(clickedTabIndex);
            }
            closeSettingsTab();
        }
        return;
    }
    
    // Если кликнули по вкладке справки, показываем специальное меню
    if (clickedTabIndex == m_helpTabIndex) {
        QMenu contextMenu(this);
        QAction *actClose = contextMenu.addAction(tr("Закрыть"));
        QAction *selectedAction = contextMenu.exec(tabBar->mapToGlobal(pos));
        if (selectedAction == actClose) {
            if (m_tabWidget->currentIndex() != clickedTabIndex) {
                m_tabWidget->setCurrentIndex(clickedTabIndex);
            }
            closeTab(clickedTabIndex);
        }
        return;
    }
    
    // Если кликнули по вкладке игры, показываем специальное меню
    if (clickedTabIndex == m_snakeGameTabIndex) {
        QMenu contextMenu(this);
        QAction *actClose = contextMenu.addAction(tr("Закрыть"));
        QAction *selectedAction = contextMenu.exec(tabBar->mapToGlobal(pos));
        if (selectedAction == actClose) {
            if (m_tabWidget->currentIndex() != clickedTabIndex) {
                m_tabWidget->setCurrentIndex(clickedTabIndex);
            }
            closeTab(clickedTabIndex);
        }
        return;
    }
    
    QMenu contextMenu(this);
    
    CodeEditor *clickedEditor = getEditorFromTabWidget(clickedTabIndex);
    QAction *actSave = contextMenu.addAction(tr("Сохранить"));
    actSave->setEnabled(clickedEditor != nullptr && clickedEditor->document()->isModified());
    
    contextMenu.addSeparator();
    
    QAction *actClose = contextMenu.addAction(tr("Закрыть"));
    
    QAction *actCloseOthers = contextMenu.addAction(tr("Закрыть все кроме текущего"));
    // Учитываем вкладку настроек и справки при подсчете
    int editableTabsCount = m_tabWidget->count();
    if (m_settingsTabIndex >= 0) editableTabsCount--;
    if (m_helpTabIndex >= 0) editableTabsCount--;
    actCloseOthers->setEnabled(editableTabsCount > 1);
    
    QAction *selectedAction = contextMenu.exec(tabBar->mapToGlobal(pos));
    
    if (selectedAction == actClose) {
        if (m_tabWidget->currentIndex() != clickedTabIndex) {
            m_tabWidget->setCurrentIndex(clickedTabIndex);
        }
        closeTab(clickedTabIndex);
    } else if (selectedAction == actCloseOthers) {
        closeAllTabsExceptCurrent();
    } else if (selectedAction == actSave) {
        // Переключаемся на вкладку, по которой кликнули, и сохраняем
        if (clickedTabIndex != m_tabWidget->currentIndex()) {
            m_tabWidget->setCurrentIndex(clickedTabIndex);
        }
        saveFile();
    }
}

void MainWindow::closeTab(int index) {
    if (!m_tabWidget || index < 0 || index >= m_tabWidget->count())
        return;
    
    QWidget *tabWidget = m_tabWidget->widget(index);
    if (!tabWidget)
        return;
    
    CodeEditor *editor = getEditorFromWidget(tabWidget);
    
    const bool isSettingsTab = (tabWidget == m_settingsWidget);
    const bool isHelpTab = (tabWidget == m_helpWidget);
    const bool isPyrobTab = (tabWidget == m_pyrobEditorWidget);
    const bool isSnakeTab = (tabWidget == m_snakeGameContainer);
    
    int currentIndex = m_tabWidget->currentIndex();
    QPointer<QWidget> widgetToRemove = tabWidget;
    bool deleteWidget = !(isSettingsTab || isHelpTab || isPyrobTab);
    
    bool hasAlternativeTabs = (m_tabWidget->count() > 1);
    bool shouldAnimateClose = (currentIndex == index) && hasAlternativeTabs && !widgetToRemove.isNull();
    int targetIndex = -1;
    if (shouldAnimateClose) {
        if (index > 0) {
            targetIndex = index - 1; // Всегда стараемся перелистнуть влево
        } else if (index + 1 < m_tabWidget->count()) {
            targetIndex = index + 1;
        } else {
            shouldAnimateClose = false;
        }
    }
    
    // Проверяем несохраненные изменения только для редакторов
    if (editor && editor->document()->isModified()) {
        QString filePath = m_editorToPath.value(editor, QString());
        bool isEmpty = editor->toPlainText().trimmed().isEmpty();
        bool isNewFile = filePath.isEmpty();
        bool isInitialFile = editor->property("isInitialFile").toBool();
        
        // Если файл пустой, новый и является начальным файлом при запуске, не показываем диалог - просто закрываем
        if (isEmpty && isNewFile && isInitialFile) {
            editor->document()->setModified(false);
            // Продолжаем закрытие без диалога
        } else {
            QString fileName = filePath.isEmpty() ? tr("Новый файл") : QFileInfo(filePath).fileName();
            
            const auto ret = QMessageBox::warning(
                this, tr("Несохранённые изменения"),
                tr("Сохранить изменения в файле \"%1\" перед закрытием?").arg(fileName),
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                QMessageBox::Save);
            
            if (ret == QMessageBox::Cancel) {
                // Восстанавливаем текущую вкладку
                if (currentIndex == index) {
                    m_tabWidget->setCurrentIndex(index);
                }
                return;
            }
            
            if (ret == QMessageBox::Save) {
                if (filePath.isEmpty()) {
                    // Сохраняем как новый файл
                    QString path = showFilePicker(
                        tr("Python файлы (*.py);;Все файлы (*.*)"),
                        QString(),
                        true,
                        false
                    );
                    
                    if (path.isEmpty()) {
                        // Восстанавливаем текущую вкладку
                        if (currentIndex == index) {
                            m_tabWidget->setCurrentIndex(index);
                        }
                        return;
                    }
                    if (!path.endsWith(".py", Qt::CaseInsensitive))
                        path += ".py";
                    
                    QFile f(path);
                    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                        QTextStream ts(&f);
                        ts.setCodec("UTF-8");
                        ts << editor->toPlainText();
                        f.close();
                    }
                } else {
                    QFile f(filePath);
                    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                        QTextStream ts(&f);
                        ts.setCodec("UTF-8");
                        ts << editor->toPlainText();
                        f.close();
                    }
                }
            }
            // Если выбрали Discard, просто продолжаем закрытие
        }
    }
    
    // Удаляем из карты только редакторы
    if (editor) {
        m_editorToPath.remove(editor);
        // Очищаем кэш автодополнений для этого редактора
        m_fileCompletionsCache.remove(editor);
        m_fileContentHash.remove(editor);
        m_editorCompletionsModels.remove(editor);
    }
    
    // Обновляем индекс вкладки настроек, если закрываемая вкладка была перед ней
    if (m_settingsTabIndex >= 0 && index < m_settingsTabIndex) {
        m_settingsTabIndex--;
    }
    
    // Обновляем индекс вкладки справки, если закрываемая вкладка была перед ней
    if (m_helpTabIndex >= 0 && index < m_helpTabIndex) {
        m_helpTabIndex--;
    }
    
    // Обновляем индекс вкладки pyrob редактора, если закрываемая вкладка была перед ней
    if (m_pyrobEditorTabIndex >= 0 && index < m_pyrobEditorTabIndex) {
        m_pyrobEditorTabIndex--;
    }
    
    // Оптимизация памяти: очищаем документ перед удалением контейнера
    if (editor && editor->document()) {
        editor->setText("");
    }
    
    auto finalizeClose = [this, widgetToRemove, deleteWidget, isSettingsTab, isHelpTab, isPyrobTab, isSnakeTab]() {
        if (!widgetToRemove)
            return;
        
        // Проверяем, что виджет еще существует перед операциями
        if (widgetToRemove.isNull()) {
            return;
        }
        
        // Специальная обработка для pyrob редактора - делаем все до removeTab
        if (isPyrobTab && m_pyrobEditorWidget && m_pyrobEditorWidget == widgetToRemove) {
            m_pyrobEditorTabIndex = -1;
            
            // Сохраняем указатель перед очисткой
            PyrobEditorWidget *widgetToDelete = m_pyrobEditorWidget;
            
            // Очищаем указатель ДО любых операций с виджетом
            m_pyrobEditorWidget = nullptr;
            
            // Удаляем виджет из QTabWidget
            int tabIdx = m_tabWidget->indexOf(widgetToDelete);
            if (tabIdx != -1) {
                m_tabWidget->removeTab(tabIdx);
            }
            
            // Отключаем все связи перед очисткой
            if (widgetToDelete) {
                widgetToDelete->disconnect();
            }
            
            // Удаляем виджет из родителя перед очисткой
            if (widgetToDelete && widgetToDelete->parent()) {
                widgetToDelete->setParent(nullptr);
            }
            
            // НЕ вызываем clearAll() и НЕ вызываем deleteLater() сразу
            // Вместо этого используем QTimer для отложенного удаления,
            // чтобы все события успели обработаться
            if (widgetToDelete) {
                // Используем QTimer для безопасного удаления в следующем цикле событий
                QTimer::singleShot(0, this, [widgetToDelete]() {
                    if (widgetToDelete) {
                        widgetToDelete->deleteLater();
                    }
                });
            }
            
            // Не продолжаем выполнение лямбды, так как виджет уже обработан
            return;
        }
        
        // Обычная обработка для других вкладок
        int tabIdx = m_tabWidget->indexOf(widgetToRemove);
        if (tabIdx != -1) {
            m_tabWidget->removeTab(tabIdx);
        }
        
        // Проверяем еще раз после removeTab
        if (widgetToRemove.isNull()) {
            return;
        } else if (isSettingsTab) {
            m_settingsTabIndex = -1;
            if (!widgetToRemove.isNull()) {
                widgetToRemove->hide();
            }
        } else if (isHelpTab) {
            m_helpTabIndex = -1;
            if (!widgetToRemove.isNull()) {
                widgetToRemove->hide();
            }
        } else if (deleteWidget) {
            if (!widgetToRemove.isNull()) {
                widgetToRemove->deleteLater();
            }
        } else {
            if (!widgetToRemove.isNull()) {
                widgetToRemove->hide();
            }
        }
        if (isSnakeTab) {
            m_snakeGameTabIndex = -1;
            if (m_snakeGameContainer) {
                if (!deleteWidget) {
                    m_snakeGameContainer->deleteLater();
                }
                m_snakeGameContainer = nullptr;
            }
            m_snakeGame = nullptr;
        }
        
        // Не создаем новый файл автоматически при закрытии последней вкладки
        // Новый файл создается только один раз при запуске IDE
        updateWindowTitle();
    };
    
    if (shouldAnimateClose && targetIndex >= 0) {
        animateTabSwitch(index, targetIndex, [finalizeClose]() {
            finalizeClose();
        });
        return;
    }
    
    finalizeClose();
}

void MainWindow::closeAllTabsExceptCurrent() {
    int currentIndex = m_tabWidget->currentIndex();
    if (currentIndex < 0) return;
    
    // Закрываем вкладки справа налево, чтобы индексы не сбивались
    // Не закрываем текущую вкладку, вкладку настроек и справки
    for (int i = m_tabWidget->count() - 1; i >= 0; --i) {
        if (i != currentIndex && i != m_settingsTabIndex && i != m_helpTabIndex) {
            closeTab(i);
        }
    }
}


void MainWindow::appendOutput(const QString &text, bool isError) {
    if (!m_output) return;
    
    auto linkify = [&](const QString &t) {
        // Поиск строк трассировки: File "path", line N
        static QRegExp rx("File \"([^\"]+)\", line (\\d+)");
        QString html = QString(t).toHtmlEscaped();
        int pos = 0;
        while ((pos = rx.indexIn(t, pos)) != -1) {
            const QString path = rx.cap(1);
            const QString line = rx.cap(2);
            const QString orig = QString("File \"%1\", line %2").arg(path, line);
            const QString repl = QString("<a href=\"vuzhyk://open?path=%1&line=%2\">%3</a>")
                                  .arg(QString(path).toHtmlEscaped(), QString(line).toHtmlEscaped(), QString(orig).toHtmlEscaped());
            html.replace(QString(orig).toHtmlEscaped(), repl);
            pos += rx.matchedLength();
        }
        return html;
    };

    if (isError) {
        m_output->insertHtml(QString("<span style='color:#b00020'>%1</span><br/>").arg(linkify(text)));
    } else {
        QString html = QString(text).toHtmlEscaped();
        html.replace("\n", "<br/>");
        m_output->insertHtml(QString("%1").arg(html));
    }
    
    // Оптимизация памяти: ограничиваем размер вывода
    const int MAX_OUTPUT_LENGTH = 500000; // Максимум ~500К символов
    QString currentText = m_output->toPlainText();
    if (currentText.length() > MAX_OUTPUT_LENGTH) {
        // Оставляем последние 400К символов
        QString trimmed = currentText.right(400000);
        m_output->setPlainText(trimmed);
        m_output->moveCursor(QTextCursor::End);
    }
}

void MainWindow::appendReplOutput(const QString &text, bool isError) {
    auto linkify = [&](const QString &t) {
        static QRegExp rx("File \"([^\"]+)\", line (\\d+)");
        QString html = QString(t).toHtmlEscaped();
        int pos = 0;
        while ((pos = rx.indexIn(t, pos)) != -1) {
            const QString path = rx.cap(1);
            const QString line = rx.cap(2);
            const QString orig = QString("File \"%1\", line %2").arg(path, line);
            const QString repl = QString("<a href=\"vuzhyk://open?path=%1&line=%2\">%3</a>")
                                  .arg(QString(path).toHtmlEscaped(), QString(line).toHtmlEscaped(), QString(orig).toHtmlEscaped());
            html.replace(QString(orig).toHtmlEscaped(), repl);
            pos += rx.matchedLength();
        }
        return html;
    };
    if (isError) {
        m_replOutput->insertHtml(QString("<span style='color:#b00020'>%1</span><br/>").arg(linkify(text)));
    } else {
        QString html = QString(text).toHtmlEscaped();
        html.replace("\n", "<br/>");
        m_replOutput->insertHtml(QString("%1").arg(html));
    }
    
    // Оптимизация памяти: ограничиваем размер вывода REPL
    const int MAX_REPL_OUTPUT_LENGTH = 500000; // Максимум ~500К символов
    QString currentText = m_replOutput->toPlainText();
    if (currentText.length() > MAX_REPL_OUTPUT_LENGTH) {
        // Оставляем последние 400К символов
        QString trimmed = currentText.right(400000);
        m_replOutput->setPlainText(trimmed);
        m_replOutput->moveCursor(QTextCursor::End);
    }
}

void MainWindow::onOutputAnchorClicked(const QUrl &url) {
    if (url.scheme() != "vuzhyk") return;
    QUrlQuery q(url);
    const QString path = q.queryItemValue("path");
    const int line = q.queryItemValue("line").toInt();
    openFileAt(path, line);
}

void MainWindow::onReplAnchorClicked(const QUrl &url) {
    onOutputAnchorClicked(url);
}

void MainWindow::onFsDoubleClicked(const QModelIndex &index) {
    if (!m_fsModel) return;
    const QString path = m_fsModel->filePath(index);
    QFileInfo fi(path);
    if (fi.isFile())
        loadFromPath(path);
}

void MainWindow::openFileAt(const QString &path, int line) {
    if (!QFile::exists(path)) return;
    
    // Проверяем, не открыт ли файл уже
    int existingTab = findTabByPath(path);
    if (existingTab >= 0) {
        m_tabWidget->setCurrentIndex(existingTab);
    } else {
        loadFromPath(path);
    }
    
    // Переходим к нужной строке
    CodeEditor *editor = currentEditor();
    if (!editor) return;
    
    // QScintilla использует номера строк напрямую (0-based)
    int targetLine = qMax(0, line - 1);
    if (targetLine < editor->lines()) {
        editor->setCursorPosition(targetLine, 0);
        editor->ensureLineVisible(targetLine);
    }
    editor->setFocus();
}

void MainWindow::updateCompletionFromDocument() {
    CodeEditor *editor = currentEditor();
    if (!editor) {
        return;
    }
    
    // Проверка completer не нужна, так как мы используем QsciAPIs для автокомплита
    // Продолжаем работу даже если completer равен nullptr
    
    // Вычисляем хеш текущего содержимого файла
    const QString currentContent = editor->toPlainText();
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(currentContent.toUtf8());
    QString currentHash = hash.result().toHex();
    
    // Получаем текущую позицию курсора для проверки контекста
    int currentLine, currentIndex;
    editor->getCursorPosition(&currentLine, &currentIndex);
    
    // Проверяем, находится ли курсор после "def " в определении функции
    bool isInDefContext = false;
    if (currentLine >= 0 && currentLine < editor->lines()) {
        QString lineText = editor->text(currentLine);
        // Ищем "def " перед курсором в текущей строке
        int defPos = lineText.lastIndexOf("def ", currentIndex, Qt::CaseSensitive);
        if (defPos >= 0) {
            // Проверяем, что курсор находится после "def "
            int defEndPos = defPos + 4; // позиция после "def "
            if (currentIndex >= defEndPos) {
                // Получаем текст между "def " и курсором
                QString betweenDefAndCursor = lineText.mid(defEndPos, currentIndex - defEndPos);
                // Убираем пробелы для проверки
                QString trimmed = betweenDefAndCursor.trimmed();
                // Если между "def " и курсором только пробелы или начало идентификатора - это контекст def
                // Проверяем, что нет символов, которые не могут быть в имени функции (например, скобки, двоеточие)
                if (trimmed.isEmpty() || (QRegExp("^[a-zA-Z_][a-zA-Z0-9_]*$").exactMatch(trimmed) && !betweenDefAndCursor.contains('(') && !betweenDefAndCursor.contains(':'))) {
                    isInDefContext = true;
                }
            }
        }
    }
    
    // Загружаем задачи pyrob только если курсор находится в контексте "def "
    QStringList pyrobTasks;
    if (isInDefContext) {
        pyrobTasks = loadPyrobTasks();
    }
    
    // Если содержимое не изменилось с последнего раза, используем кэш
    // Включаем контекст def в ключ кэша
    QString cacheKey = currentHash + QString("_def_%1").arg(isInDefContext ? 1 : 0);
    if (m_fileContentHash.contains(editor) && m_fileContentHash[editor] == cacheKey) {
        // Содержимое не изменилось - используем кэш
        QStringList cachedFileWords = m_fileCompletionsCache[editor];
        
        // Объединяем базовые слова, кэшированные слова из файла и задачи pyrob
        QSet<QString> allSet = m_cachedBaseCompletionsSet;
        for (const QString &word : cachedFileWords) {
            allSet.insert(word);
        }
        // Добавляем задачи pyrob только если в контексте def
        if (isInDefContext) {
            for (const QString &task : pyrobTasks) {
                if (!task.isEmpty() && task.length() > 1) {
                    allSet.insert(task);
                }
            }
        }
        
        // Преобразуем в отсортированный список
        QStringList all = QStringList::fromSet(allSet);
        all.sort(Qt::CaseInsensitive);
        
        // Обновляем модель completer для этого редактора (если используется)
        // Проверяем, что модель существует перед использованием
        if (m_editorCompletionsModels.contains(editor)) {
            auto *model = m_editorCompletionsModels.value(editor);
            if (model) {
                model->setStringList(all);
            }
        }
        
        // Обновляем API лексера для встроенного автокомплита QScintilla
        // Проверяем, что редактор валиден и лексер инициализирован перед вызовом
        if (editor) {
            QsciLexer *lexer = editor->lexer();
            if (lexer) {
                // Используем QTimer для отложенного вызова, чтобы убедиться, что лексер полностью инициализирован
                // Используем QPointer для безопасной проверки, что редактор еще существует
                QPointer<CodeEditor> editorPtr = editor;
                // Объединяем базовые слова (включая pyrob API) и задачи pyrob
                QStringList allWords = m_cachedBaseCompletions;
                allWords.append(pyrobTasks);
                QTimer::singleShot(50, this, [editorPtr, allWords]() {
                    if (editorPtr && editorPtr->lexer()) {
                        try {
                            editorPtr->updateAPIs(allWords);
                        } catch (...) {
                            // Игнорируем ошибки при обновлении API
                        }
                    }
                });
            }
        }
        
        return; // Выходим, не пересчитывая слова
    }
    
    // Содержимое изменилось - пересчитываем слова из файла
    const QString text = currentContent;
    const QStringList lines = text.split('\n');
    
    // Извлекаем идентификаторы из текста
    QRegExp idRx("\\b[A-Za-z_][A-Za-z0-9_]*\\b");
    QSet<QString> ids;
    int pos = 0;
    while ((pos = idRx.indexIn(text, pos)) != -1) {
        const QString w = idRx.cap(0);
        if (w.length() > 1) {
            ids.insert(w);
        }
        pos += idRx.matchedLength();
    }
    
    // Импорты: import xxx, from yyy import zzz
    QRegExp importRx("^(?:from\\s+([A-Za-z_][A-Za-z0-9_\\.]*)\\s+import\\s+([A-Za-z_][A-Za-z0-9_]*(?:\\s*,\\s*[A-Za-z_][A-Za-z0-9_]*)*))|(?:import\\s+([A-Za-z_][A-Za-z0-9_]*(?:\\s*,\\s*[A-Za-z_][A-Za-z0-9_]*)*))$");
    importRx.setCaseSensitivity(Qt::CaseInsensitive);
    importRx.setMinimal(true);
    for (const QString &ln : lines) {
        if (importRx.indexIn(ln) >= 0) {
            const QString fromMods = importRx.cap(1);
            const QString imported = importRx.cap(2);
            const QString mods = importRx.cap(3);
            auto addCSV = [&](const QString &csv){ 
                for (const QString &p : csv.split(',', QString::SkipEmptyParts)) {
                    QString trimmed = p.trimmed();
                    if (trimmed.length() > 1 && !trimmed.contains('.')) {
                        ids.insert(trimmed);
                    }
                }
            };
            if (!fromMods.isEmpty()) {
                addCSV(fromMods);
                if (!imported.isEmpty()) addCSV(imported);
            } else if (!mods.isEmpty()) {
                addCSV(mods);
            }
        }
    }
    
    // Фильтруем слова из файла (исключаем префиксы базовых слов)
    // Используем QSet для быстрой проверки вместо линейного поиска
    QStringList fileWords;
    for (const QString &w : ids) {
        if (w.length() >= 3) {
            bool isPrefix = false;
            // Проверяем, является ли слово префиксом какого-либо базового слова
            // Используем QSet для быстрой проверки (O(1) вместо O(n))
            for (const QString &baseWord : m_cachedBaseCompletionsSet) {
                if (baseWord.length() > w.length() && baseWord.startsWith(w, Qt::CaseInsensitive)) {
                    isPrefix = true;
                    break;
                }
            }
            if (!isPrefix) {
                fileWords.append(w);
            }
        }
    }
    
    // Добавляем задачи pyrob в автокомплит только если курсор в контексте "def "
    QStringList pyrobTasksForAPI;
    if (isInDefContext) {
        for (const QString &task : pyrobTasks) {
            if (!task.isEmpty() && task.length() > 1) {
                fileWords.append(task);
                pyrobTasksForAPI.append(task); // Также добавляем в API для QScintilla автокомплита
            }
        }
    }
    
    // Обновляем кэш для этого редактора
    m_fileCompletionsCache[editor] = fileWords;
    // Сохраняем хеш содержимого файла
    m_fileContentHash[editor] = cacheKey;
    
    // Обновляем API лексера для встроенного автокомплита QScintilla
    // Всегда добавляем базовые слова (включая pyrob API) и задачи pyrob
    if (editor) {
        QsciLexer *lexer = editor->lexer();
        if (lexer) {
            // Используем QTimer для отложенного вызова, чтобы убедиться, что лексер полностью инициализирован
            // Используем QPointer для безопасной проверки, что редактор еще существует
            QPointer<CodeEditor> editorPtr = editor;
            // Объединяем базовые слова (включая pyrob API) и задачи pyrob
            QStringList allWords = m_cachedBaseCompletions;
            allWords.append(pyrobTasksForAPI);
            QTimer::singleShot(50, this, [editorPtr, allWords]() {
                if (editorPtr && editorPtr->lexer()) {
                    try {
                        editorPtr->updateAPIs(allWords);
                    } catch (...) {
                        // Игнорируем ошибки при обновлении API
                    }
                }
            });
        }
    }
    
    // Объединяем базовые слова (из кэша) и слова из файла
    // Используем QSet напрямую для избежания промежуточных списков
    QSet<QString> allSet = m_cachedBaseCompletionsSet;
    for (const QString &word : fileWords) {
        allSet.insert(word);
    }
    
    // Преобразуем в отсортированный список
    QStringList all = QStringList::fromSet(allSet);
    all.sort(Qt::CaseInsensitive);
    
    // Обновляем модель completer для этого редактора (на случай, если используется QCompleter)
    auto *model = m_editorCompletionsModels.value(editor);
    if (model) {
        model->setStringList(all);
    }
}

QStringList MainWindow::loadPythonCompletions() {
    QStringList result;
    
    // Пытаемся получить путь к Python
    const QString python = detectPythonExecutable();
    if (python.isEmpty() || !QFile::exists(python)) {
        return result; // Возвращаем пустой список, будет использован fallback
    }
    
    // Создаем временный Python скрипт для получения списка команд
    QString script = 
        "import keyword\n"
        "import sys\n"
        "import pkgutil\n"
        "\n"
        "result = []\n"
        "\n"
        "# Ключевые слова Python\n"
        "result.extend(keyword.kwlist)\n"
        "\n"
        "# Встроенные функции и объекты\n"
        "builtins_list = dir(__builtins__)\n"
        "for item in builtins_list:\n"
        "    if not item.startswith('_'):\n"
        "        result.append(item)\n"
        "    elif item in ['__debug__', '__import__']:\n"
        "        result.append(item)\n"
        "\n"
        "# Константы\n"
        "result.extend(['Ellipsis', 'NotImplemented', '__debug__'])\n"
        "\n"
        "# Исключения из __builtins__\n"
        "exceptions = [name for name in dir(__builtins__) if isinstance(getattr(__builtins__, name, None), type) and issubclass(getattr(__builtins__, name, None), BaseException) if hasattr(__builtins__, name)]\n"
        "try:\n"
        "    exceptions = [name for name in dir(__builtins__) if name[0].isupper()]\n"
        "    for exc in exceptions:\n"
        "        obj = getattr(__builtins__, exc, None)\n"
        "        if isinstance(obj, type) and issubclass(obj, BaseException):\n"
        "            result.append(exc)\n"
        "except:\n"
        "    pass\n"
        "\n"
        "# Модули стандартной библиотеки\n"
        "try:\n"
        "    for importer, modname, ispkg in pkgutil.iter_modules():\n"
        "        if not modname.startswith('_') and '.' not in modname:\n"
        "            result.append(modname)\n"
        "except:\n"
        "    # Если не удалось загрузить модули, используем популярные\n"
        "    stdlib_modules = ['sys', 'os', 'json', 'datetime', 'time', 'random', 'math', 're', 'collections',\n"
        "                     'itertools', 'functools', 'operator', 'pathlib', 'urllib', 'socket', 'threading',\n"
        "                     'multiprocessing', 'subprocess', 'sqlite3', 'csv', 'pickle', 'hashlib', 'base64']\n"
        "    result.extend(stdlib_modules)\n"
        "\n"
        "# Pyrob API\n"
        "try:\n"
        "    import pyrob.api\n"
        "    pyrob_items = dir(pyrob.api)\n"
        "    for item in pyrob_items:\n"
        "        if not item.startswith('_'):\n"
        "            result.append(item)\n"
        "    # Также добавляем из pyrob.core (для rob.*)\n"
        "    try:\n"
        "        import pyrob.core\n"
        "        core_items = dir(pyrob.core)\n"
        "        for item in core_items:\n"
        "            if not item.startswith('_'):\n"
        "                result.append(item)\n"
        "    except:\n"
        "        pass\n"
        "    # Также добавляем task и run_tasks из pyrob\n"
        "    try:\n"
        "        import pyrob\n"
        "        if hasattr(pyrob, 'task'):\n"
        "            result.append('task')\n"
        "        if hasattr(pyrob, 'run_tasks'):\n"
        "            result.append('run_tasks')\n"
        "    except:\n"
        "        pass\n"
        "except ImportError:\n"
        "    # Pyrob не установлен, используем расширенный базовый список\n"
        "    pyrob_api = ['move_left', 'move_right', 'move_up', 'move_down', 'wall_is_above', 'wall_is_beneath',\n"
        "                 'wall_is_on_the_left', 'wall_is_on_the_right', 'fill_cell', 'cell_is_filled', 'mov',\n"
        "                 'is_parking_point', 'goto', 'put_wall', 'set_field_size', 'set_cell_type',\n"
        "                 'set_parking_cell', 'CELL_FILLED', 'CELL_TO_BE_FILLED', 'CELL_EMPTY',\n"
        "                 'task', 'run_tasks']\n"
        "    result.extend(pyrob_api)\n"
        "\n"
        "# Turtle API (стандартная библиотека)\n"
        "try:\n"
        "    import turtle\n"
        "    turtle_items = dir(turtle)\n"
        "    for item in turtle_items:\n"
        "        if not item.startswith('_'):\n"
        "            result.append(item)\n"
        "except ImportError:\n"
        "    # Turtle не установлен, используем базовый список\n"
        "    turtle_api = ['forward', 'fd', 'backward', 'back', 'bk', 'right', 'rt', 'left', 'lt',\n"
        "                  'goto', 'setpos', 'setposition', 'setx', 'sety', 'setheading', 'seth', 'home',\n"
        "                  'circle', 'dot', 'stamp', 'clearstamp', 'clearstamps', 'undo', 'speed',\n"
        "                  'position', 'pos', 'towards', 'xcor', 'ycor', 'heading', 'distance',\n"
        "                  'degrees', 'radians', 'pendown', 'pd', 'down', 'penup', 'pu', 'up',\n"
        "                  'pensize', 'width', 'pen', 'isdown', 'color', 'pencolor', 'fillcolor',\n"
        "                  'filling', 'begin_fill', 'end_fill', 'reset', 'clear', 'write',\n"
        "                  'showturtle', 'st', 'hideturtle', 'ht', 'isvisible', 'shape', 'resizemode',\n"
        "                  'shapesize', 'turtlesize', 'shearfactor', 'settiltangle', 'tiltangle', 'tilt',\n"
        "                  'shapetransform', 'get_shapepoly', 'onclick', 'onrelease', 'ondrag',\n"
        "                  'begin_poly', 'end_poly', 'get_poly', 'clone', 'getturtle', 'getscreen',\n"
        "                  'setundobuffer', 'undobufferentries', 'tracer', 'window_width', 'window_height',\n"
        "                  'bgcolor', 'title', 'setup', 'bye', 'exitonclick', 'done', 'mainloop',\n"
        "                  'mode', 'colormode', 'getcanvas', 'getshapes', 'register_shape', 'addshape',\n"
        "                  'listen', 'onkey', 'onkeypress', 'onkeyrelease', 'ontimer', 'textinput', 'numinput',\n"
        "                  'Turtle', 'Screen', 'RawTurtle', 'TurtleScreen', 'ScrolledCanvas', 'Shape', 'Vec2D']\n"
        "    result.extend(turtle_api)\n"
        "\n"
        "# TurtleXY API (сторонняя библиотека)\n"
        "try:\n"
        "    import turtlexy\n"
        "    turtlexy_items = dir(turtlexy)\n"
        "    for item in turtlexy_items:\n"
        "        if not item.startswith('_'):\n"
        "            result.append(item)\n"
        "except ImportError:\n"
        "    # TurtleXY не установлен, используем базовый список\n"
        "    turtlexy_api = ['forward', 'backward', 'left', 'right', 'goto', 'home',\n"
        "                    'penup', 'pendown', 'pensize', 'color', 'bgcolor',\n"
        "                    'clear', 'reset', 'write', 'circle', 'dot',\n"
        "                    'position', 'heading', 'distance', 'towards',\n"
        "                    'showturtle', 'hideturtle', 'shape', 'speed',\n"
        "                    'begin_fill', 'end_fill', 'filling', 'done']\n"
        "    result.extend(turtlexy_api)\n"
        "\n"
        "# Удаляем дубликаты и сортируем\n"
        "result = sorted(set(result))\n"
        "\n"
        "# Выводим результат, разделенный запятыми\n"
        "print(','.join(result))\n";
    
    // Запускаем Python скрипт
    QProcess process;
    process.setProgram(python);
    process.setArguments(QStringList() << "-c" << script);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    
    // Устанавливаем UTF-8 для корректной работы с кириллицей
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");
    process.setProcessEnvironment(env);
    
    process.start();
    if (!process.waitForFinished(5000)) { // Таймаут 5 секунд
        process.kill();
        return result; // Возвращаем пустой список при ошибке
    }
    
    // Читаем вывод
    QByteArray output = process.readAllStandardOutput();
    QString outputStr = QString::fromUtf8(output).trimmed();
    
    if (!outputStr.isEmpty()) {
        result = outputStr.split(',', QString::SkipEmptyParts);
        // Очищаем от лишних пробелов
        for (QString &item : result) {
            item = item.trimmed();
        }
        // Удаляем пустые элементы
        result.removeAll(QString());
    }
    
    return result;
}

QStringList MainWindow::loadPyrobTasks() {
    QStringList result;
    
    // Пытаемся получить путь к Python
    const QString python = detectPythonExecutable();
    if (python.isEmpty() || !QFile::exists(python)) {
        return result; // Возвращаем пустой список, если Python не найден
    }
    
    // Создаем Python скрипт для получения списка задач из папки pyrob/tasks
    QString script =
        "import os\n"
        "import sys\n"
        "\n"
        "tasks = []\n"
        "\n"
        "# Пытаемся импортировать pyrob и найти папку tasks\n"
        "try:\n"
        "    import pyrob\n"
        "    pyrob_path = pyrob.__file__\n"
        "    pyrob_dir = os.path.dirname(pyrob_path)\n"
        "    tasks_dir = os.path.join(pyrob_dir, 'tasks')\n"
        "except ImportError:\n"
        "    # Если pyrob не найден через импорт, пробуем стандартные пути\n"
        "    python_dir = os.path.dirname(sys.executable)\n"
        "    # Пробуем путь для встроенного Python\n"
        "    tasks_dir = os.path.join(python_dir, 'Lib', 'site-packages', 'pyrob', 'tasks')\n"
        "    if not os.path.exists(tasks_dir):\n"
        "        # Пробуем путь относительно executable\n"
        "        app_dir = os.path.dirname(os.path.dirname(python_dir))\n"
        "        tasks_dir = os.path.join(app_dir, 'Python38', 'Lib', 'site-packages', 'pyrob', 'tasks')\n"
        "\n"
        "# Получаем список .py файлов из папки tasks\n"
        "if os.path.exists(tasks_dir):\n"
        "    for filename in os.listdir(tasks_dir):\n"
        "        if filename.endswith('.py') and filename != '__init__.py':\n"
        "            # Убираем расширение .py\n"
        "            task_name = filename[:-3]\n"
        "            tasks.append(task_name)\n"
        "\n"
        "# Выводим результат, разделенный запятыми\n"
        "print(','.join(sorted(tasks)))\n";
    
    // Запускаем Python скрипт
    QProcess process;
    process.setProgram(python);
    process.setArguments(QStringList() << "-c" << script);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    
    // Устанавливаем UTF-8 для корректной работы с кириллицей
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");
    process.setProcessEnvironment(env);
    
    process.start();
    if (!process.waitForFinished(5000)) { // Таймаут 5 секунд
        process.kill();
        return result; // Возвращаем пустой список при ошибке
    }
    
    // Читаем вывод
    QByteArray output = process.readAllStandardOutput();
    QString outputStr = QString::fromUtf8(output).trimmed();
    
    if (!outputStr.isEmpty()) {
        result = outputStr.split(',', QString::SkipEmptyParts);
        // Очищаем от лишних пробелов
        for (QString &item : result) {
            item = item.trimmed();
        }
        // Удаляем пустые элементы
        result.removeAll(QString());
    }
    
    return result;
}

void MainWindow::setupEditorCompletions(CodeEditor *editor, QStringListModel *baseModel) {
    // Создаем модель только для слов из файла (пока пустую)
    // Базовые слова будут добавляться динамически при обновлении
    auto *model = new QStringListModel(QStringList(), this);
    auto *completer = new QCompleter(model, this);
    editor->setCompleter(completer);
    
    // Сохраняем ссылку на модель для этого редактора
    m_editorCompletionsModels[editor] = model;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_agentDock && event->type() == QEvent::Resize) {
        saveAgentDockWidth();
        return false;
    }

    // Перехватываем клики по вкладкам для анимации и закрытия
    if (obj == m_tabWidget->tabBar() && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        
        // Обработка закрытия вкладки средней кнопкой мыши
        if (mouseEvent->button() == Qt::MiddleButton) {
            QSettings settings;
            bool closeWithMiddleButton = settings.value("interface/closeTabWithMiddleButton", false).toBool();
            if (closeWithMiddleButton) {
                int clickedIndex = m_tabWidget->tabBar()->tabAt(mouseEvent->pos());
                if (clickedIndex >= 0 && clickedIndex < m_tabWidget->count()) {
                    closeTab(clickedIndex);
                    return true; // Событие обработано
                }
            }
        }
        
        // Обработка левой кнопки мыши для анимации
        if (mouseEvent->button() == Qt::LeftButton) {
            int clickedIndex = m_tabWidget->tabBar()->tabAt(mouseEvent->pos());
            if (clickedIndex >= 0 && clickedIndex < m_tabWidget->count()) {
                int currentIndex = m_tabWidget->currentIndex();
                if (clickedIndex != currentIndex && currentIndex >= 0) {
                    // Полностью поглощаем событие, чтобы предотвратить стандартное переключение
                    // Блокируем сигналы для предотвращения автоматического переключения
                    m_tabWidget->blockSignals(true);
                    // Запускаем анимацию (она сама переключит вкладку в нужный момент)
                    animateTabSwitch(currentIndex, clickedIndex);
                    return true; // Событие обработано, не передаём дальше
                }
            }
        }
    }
    
    // Остальная логика eventFilter
    if (obj == m_replInput && event->type() == QEvent::KeyPress) {
        return handleReplKeyPress(static_cast<QKeyEvent*>(event));
    }
    
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    // Обработка F9 для продолжения отладки
    if (event->key() == Qt::Key_F9 && m_isDebugging) {
        continueDebug();
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

bool MainWindow::handleReplKeyPress(QKeyEvent *ke) {
    const bool ctrlEnter = (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
                           && (ke->modifiers() & Qt::ControlModifier);
    const bool shiftEnter = (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
                            && (ke->modifiers() & Qt::ShiftModifier);

    if (ctrlEnter) {
        sendReplInput();
        return true;
    }
    if (shiftEnter) {
        // Вставляем перевод строки
        m_replInput->insertPlainText("\n");
        return true;
    }

    // Навигация по истории: Up/Down, когда курсор на первой/последней строке
    if (ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down) {
        QTextCursor c = m_replInput->textCursor();
        const int currentBlock = c.blockNumber();
        const int totalBlocks = m_replInput->document()->blockCount();
        const bool atFirstLine = (currentBlock == 0) && (c.positionInBlock() == 0);
        const bool atLastLine = (currentBlock == totalBlocks - 1) && (c.atBlockEnd());

        if ((ke->key() == Qt::Key_Up && atFirstLine) || (ke->key() == Qt::Key_Down && atLastLine)) {
            if (m_replHistory.isEmpty()) return true;
            if (ke->key() == Qt::Key_Up) {
                if (m_replHistoryIndex <= 0) m_replHistoryIndex = m_replHistory.size();
                m_replHistoryIndex = qMax(0, m_replHistoryIndex - 1);
            } else {
                if (m_replHistoryIndex >= m_replHistory.size()) m_replHistoryIndex = -1;
                m_replHistoryIndex = qMin(m_replHistory.size(), m_replHistoryIndex + 1);
            }
            if (m_replHistoryIndex >= 0 && m_replHistoryIndex < m_replHistory.size()) {
                m_replInput->setPlainText(m_replHistory[m_replHistoryIndex]);
                QTextCursor end = m_replInput->textCursor();
                end.movePosition(QTextCursor::End);
                m_replInput->setTextCursor(end);
            } else {
                m_replInput->clear();
            }
            return true;
        }
    }
    return false;
}


void MainWindow::ensureReplDock() {
    if (m_replDock)
        return;
    m_replDock = new QDockWidget(tr("Python REPL"), this);
    m_replDock->setObjectName("ReplDock");
    QWidget *replWidget = new QWidget(m_replDock);
    auto *replLayout = new QVBoxLayout(replWidget);
    replLayout->setContentsMargins(4,4,4,4);
    m_replOutput = new QTextBrowser(replWidget);
    m_replOutput->setOpenLinks(false);
    connect(m_replOutput, &QTextBrowser::anchorClicked, this, &MainWindow::onReplAnchorClicked);
    m_replInput = new QTextEdit(replWidget);
    m_replInput->setPlaceholderText(tr("Введите код. Ctrl+Enter — выполнить, Shift+Enter — новая строка"));
    m_replInput->installEventFilter(this);
    replLayout->addWidget(m_replOutput);
    replLayout->addWidget(m_replInput);
    replWidget->setLayout(replLayout);
    m_replDock->setWidget(replWidget);
    addDockWidget(Qt::BottomDockWidgetArea, m_replDock);
    m_replDock->hide();
}

void MainWindow::ensureProjectDock() {
    if (m_projectDock)
        return;
    m_projectDock = new QDockWidget(tr("Проект"), this);
    m_projectDock->setObjectName("ProjectDock");
    m_fsModel = new QFileSystemModel(m_projectDock);
    m_fsModel->setNameFilters({"*.py", "*.txt", "*.md"});
    m_fsModel->setNameFilterDisables(false);
    m_fsView = new QTreeView(m_projectDock);
    m_fsView->setModel(m_fsModel);
    m_fsView->setHeaderHidden(true);
    connect(m_fsView, &QTreeView::doubleClicked, this, &MainWindow::onFsDoubleClicked);
    m_projectDock->setWidget(m_fsView);
    addDockWidget(Qt::LeftDockWidgetArea, m_projectDock);
    m_projectDock->hide();
}

void MainWindow::ensureAgentDock()
{
    if (m_agentDock) {
        return;
    }

    QSettings settings;
    m_agentDockWidth = settings.value(QStringLiteral("agent/panelWidth"), 260).toInt();

    m_agentDock = new QDockWidget(tr("Агент"), this);
    m_agentDock->setObjectName(QStringLiteral("AgentDock"));
    m_agentDock->setAllowedAreas(Qt::RightDockWidgetArea);
    m_agentDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    m_agentDock->installEventFilter(this);

    m_agentPanel = new AgentPanel(m_agentDock);
    m_agentPanel->setTheme(m_currentTheme);
    m_agentPanel->setContextProvider([this]() { return agentEditorContext(); });
    m_agentPanel->setCodeApplier([this](const QString &response, QString *statusMessage) {
        return applyAgentResponseToEditor(response, statusMessage);
    });
    m_agentDock->setWidget(m_agentPanel);
    m_agentDockWidth = qBound(agentDockMinWidth(), m_agentDockWidth, 800);
    m_agentDock->setMinimumWidth(agentDockMinWidth());

    // Угол уже назначен RightDock в setupUi; закрепляем агент справа на всю высоту.
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, m_agentDock);
    applyAgentDockWidth();
    m_agentDock->hide();

    connect(m_agentDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (m_actToggleAgent) {
            QSignalBlocker blocker(m_actToggleAgent);
            m_actToggleAgent->setChecked(visible);
        }
        // При закрытии агента нижняя панель снова на всю ширину (поведение Qt по умолчанию).
        if (!visible) {
            setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
            return;
        }
        QTimer::singleShot(0, this, [this]() { applyAgentDockWidth(); });
    });
}

void MainWindow::applyAgentDockWidth()
{
    if (!m_agentDock || !m_agentDock->isVisible()) {
        return;
    }
    const int minW = agentDockMinWidth();
    m_agentDock->setMinimumWidth(minW);
    const int dockWidth = qBound(minW, m_agentDockWidth, qMax(minW, this->width() - 200));
    m_applyingAgentDockWidth = true;
    resizeDocks({m_agentDock}, {dockWidth}, Qt::Horizontal);
    m_applyingAgentDockWidth = false;
}

int MainWindow::agentDockMinWidth() const
{
    if (m_agentPanel) {
        return qMax(m_agentPanel->minimumSizeHint().width(), m_agentPanel->minimumWidth());
    }
    return 160;
}

void MainWindow::saveAgentDockWidth()
{
    if (!m_agentDock || !m_agentDock->isVisible() || m_applyingAgentDockWidth) {
        return;
    }
    const int width = m_agentDock->width();
    if (width < agentDockMinWidth()) {
        return;
    }
    m_agentDockWidth = width;
    QSettings settings;
    settings.setValue(QStringLiteral("agent/panelWidth"), m_agentDockWidth);
}

void MainWindow::toggleAgentPanel()
{
    if (!isAgentFeatureAllowed()) {
        if (m_actToggleAgent) {
            QSignalBlocker blocker(m_actToggleAgent);
            m_actToggleAgent->setChecked(false);
        }
        if (m_agentDock) {
            m_agentDock->hide();
        }
        return;
    }

    ensureAgentDock();
    if (!m_agentDock) {
        return;
    }

    const bool show = m_actToggleAgent ? m_actToggleAgent->isChecked() : !m_agentDock->isVisible();
    m_agentDock->setVisible(show);
    if (show && m_agentPanel) {
        m_agentPanel->reloadSettings();
        m_agentPanel->refreshModels();
    }
}

bool MainWindow::isAgentFeatureAllowed() const
{
    if (!m_librarySyncManager) {
        return true;
    }
    return m_librarySyncManager->isStudentAgentAllowed();
}

void MainWindow::updateAgentAvailability()
{
    const bool allowed = isAgentFeatureAllowed();
    if (m_actToggleAgent) {
        m_actToggleAgent->setVisible(allowed);
        if (!allowed) {
            QSignalBlocker blocker(m_actToggleAgent);
            m_actToggleAgent->setChecked(false);
        }
    }
    if (!allowed && m_agentDock) {
        m_agentDock->hide();
    }
    if (m_settingsWidget) {
        m_settingsWidget->setAgentFeatureVisible(allowed);
    }
}

QString MainWindow::agentEditorContext() const
{
    CodeEditor *editor = currentEditor();
    if (!editor) {
        return QStringLiteral(
            "No code editor tab is active.\n"
            "If the user asks to create a new program, emit replace_file and the IDE will open a new tab.");
    }

    QString path = m_editorToPath.value(editor);
    if (path.isEmpty()) {
        path = QStringLiteral("(unsaved file)");
    }

    return AgentEdit::buildNumberedFileContext(path, editor->toPlainText());
}

bool MainWindow::applyAgentResponseToEditor(const QString &assistantResponse, QString *statusMessage)
{
    const QVector<AgentEdit::Operation> ops = AgentEdit::parseOperations(assistantResponse);
    if (ops.isEmpty()) {
        if (statusMessage) {
            *statusMessage = tr("Готово. Блоков правки не найдено (нужен <<<VUZHYK_EDIT ... <<<END_EDIT).");
        }
        return false;
    }

    CodeEditor *editor = currentEditor();
    if (!editor) {
        const int MAX_TABS = 50;
        int codeEditorCount = 0;
        for (int i = 0; i < m_tabWidget->count(); ++i) {
            if (getEditorFromTabWidget(i)) {
                ++codeEditorCount;
            }
        }
        if (codeEditorCount >= MAX_TABS) {
            if (statusMessage) {
                *statusMessage = tr("Не удалось применить правки: слишком много вкладок.");
            }
            return false;
        }

        QWidget *container = new QWidget(m_tabWidget);
        auto *layout = new QVBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        CodeEditor *newEditor = new CodeEditor(container);
        setupEditor(newEditor);
        layout->addWidget(newEditor);
        container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        newEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        const int index = m_tabWidget->addTab(container, tr("Новый файл"));
        m_editorToPath[newEditor] = QString();
        m_tabWidget->setCurrentIndex(index);
        editor = newEditor;
    }

    QString error;
    const QString newText = AgentEdit::applyOperations(editor->toPlainText(), ops, &error);
    if (!error.isEmpty()) {
        if (statusMessage) {
            *statusMessage = tr("Ошибка применения правок: %1").arg(error);
        }
        return false;
    }

    editor->setPlainText(newText);
    editor->setModified(true);

    QString language;
    for (const AgentEdit::Operation &op : ops) {
        if (!op.language.isEmpty()) {
            language = op.language;
            break;
        }
    }
    QString ext = QStringLiteral("py");
    if (language == QLatin1String("html") || language == QLatin1String("htm")) {
        ext = QStringLiteral("html");
    } else if (language == QLatin1String("javascript") || language == QLatin1String("js")) {
        ext = QStringLiteral("js");
    } else if (language == QLatin1String("css")) {
        ext = QStringLiteral("css");
    } else if (language == QLatin1String("json")) {
        ext = QStringLiteral("json");
    } else if (!language.isEmpty()
               && language != QLatin1String("python")
               && language != QLatin1String("py")) {
        ext = language;
    }
    editor->setFileExtension(ext);
    updateTabText(editor);

    int replaceFile = 0, replaceLines = 0, insertAfter = 0, deleteLines = 0;
    for (const AgentEdit::Operation &op : ops) {
        switch (op.type) {
        case AgentEdit::Type::ReplaceEntireFile: ++replaceFile; break;
        case AgentEdit::Type::ReplaceLines: ++replaceLines; break;
        case AgentEdit::Type::InsertAfter: ++insertAfter; break;
        case AgentEdit::Type::DeleteLines: ++deleteLines; break;
        }
    }

    QStringList parts;
    if (replaceFile) parts << tr("файл целиком: %1").arg(replaceFile);
    if (replaceLines) parts << tr("строки: %1").arg(replaceLines);
    if (insertAfter) parts << tr("вставки: %1").arg(insertAfter);
    if (deleteLines) parts << tr("удаления: %1").arg(deleteLines);

    if (statusMessage) {
        *statusMessage = tr("Готово. Применено к вкладке (%1).").arg(parts.join(QStringLiteral(", ")));
    }
    statusBar()->showMessage(tr("Агент применил правки к редактору."), 3000);
    return true;
}

void MainWindow::toggleRepl(bool enabled) {
    if (enabled) {
        startRepl();
    } else {
        stopRepl();
    }
}

void MainWindow::toggleTheme() {
    QString newTheme = (m_currentTheme == "light") ? "dark" : "light";
    QString newThemeName = (newTheme == "dark") ? tr("тёмную") : tr("светлую");
    
    // Проверяем наличие несохраненных изменений
    // Проверяем наличие несохраненных изменений во всех вкладках
    bool hasUnsavedChanges = false;
    if (m_tabWidget) {
        for (int i = 0; i < m_tabWidget->count(); ++i) {
            CodeEditor *editor = getEditorFromTabWidget(i);
            if (editor && editor->document()->isModified()) {
                hasUnsavedChanges = true;
                break;
            }
        }
    }
    QString warningText = tr("Внимание!\n\n")
                         + tr("Для переключения на %1 тему приложение будет перезапущено.\n\n").arg(newThemeName);
    if (hasUnsavedChanges) {
        warningText += tr("⚠ У вас есть несохранённые изменения!\n")
                      + tr("Пожалуйста, сохраните их перед перезапуском, иначе изменения будут потеряны.\n\n");
    }
    warningText += tr("Продолжить перезапуск?");
    
    // Показываем диалог подтверждения
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Переключение темы"));
    msgBox.setText(warningText);
    msgBox.setIcon(QMessageBox::Warning);
    
    // Добавляем кнопки
    QPushButton *restartButton = msgBox.addButton(tr("Перезапуск"), QMessageBox::YesRole);
    QPushButton *cancelButton = msgBox.addButton(tr("Отмена"), QMessageBox::NoRole);
    msgBox.setDefaultButton(cancelButton);
    
    msgBox.exec();
    
    // Если пользователь нажал "Отмена", ничего не делаем
    if (msgBox.clickedButton() == cancelButton) {
        return;
    }
    
    // Если пользователь нажал "Перезапуск", продолжаем
    // Сохраняем новую тему в настройки
    QSettings settings;
    settings.setValue("theme", newTheme);
    settings.sync(); // Принудительно синхронизируем настройки
    
    // Перезапускаем приложение с новой темой
    QString appPath = QApplication::applicationFilePath();
    QStringList arguments;
    arguments << "--theme" << newTheme;
    
    QProcess::startDetached(appPath, arguments);
    
    // Закрываем текущее окно
    close();
}

void MainWindow::applyTheme(const QString &theme) {
    m_currentTheme = theme;
    // Используем light-theme иконки и перекрашиваем их в зависимости от темы
    QString iconBasePath = ":/icons/icons/light-theme/";
    QString iconColor = (theme == "dark") ? "#e0e0e0" : "#000000";

    // Сначала очищаем все иконки, чтобы гарантировать обновление
    if (m_actNew) {
        m_actNew->setIcon(QIcon());
        m_actNew->setIcon(createIconFromResource(iconBasePath + "file-new.svg", iconColor));
    }
    if (m_actOpen) {
        m_actOpen->setIcon(QIcon());
        m_actOpen->setIcon(createIconFromResource(iconBasePath + "file-open.svg", iconColor));
    }
    if (m_actSave) {
        m_actSave->setIcon(QIcon());
        m_actSave->setIcon(createIconFromResource(iconBasePath + "file-save.svg", iconColor));
    }
    if (m_actSaveAll) {
        m_actSaveAll->setIcon(QIcon());
        m_actSaveAll->setIcon(createIconFromResource(iconBasePath + "file-save-all.svg", iconColor));
    }
    if (m_actCut) {
        m_actCut->setIcon(QIcon());
        m_actCut->setIcon(createIconFromResource(iconBasePath + "cut.svg", iconColor));
    }
    if (m_actCopy) {
        m_actCopy->setIcon(QIcon());
        m_actCopy->setIcon(createIconFromResource(iconBasePath + "copy.svg", iconColor));
    }
    if (m_actPaste) {
        m_actPaste->setIcon(QIcon());
        m_actPaste->setIcon(createIconFromResource(iconBasePath + "paste-clipboard.svg", iconColor));
    }
    if (m_actUndo) {
        m_actUndo->setIcon(QIcon());
        m_actUndo->setIcon(createIconFromResource(iconBasePath + "undo.svg", iconColor));
    }
    if (m_actRedo) {
        m_actRedo->setIcon(QIcon());
        m_actRedo->setIcon(createIconFromResource(iconBasePath + "redo.svg", iconColor));
    }
    if (m_actRun) {
        m_actRun->setIcon(QIcon());
        m_actRun->setIcon(createIconFromResource(iconBasePath + "play.svg", iconColor));
    }
    if (m_actTerminate) {
        m_actTerminate->setIcon(QIcon());
        m_actTerminate->setIcon(createIconFromResource(iconBasePath + "stop.svg", iconColor));
    }
    if (m_actDebug) {
        m_actDebug->setIcon(createIconFromResource(iconBasePath + "bug.svg", iconColor));
    }
    if (m_actDebugNext) {
        m_actDebugNext->setIcon(createIconFromResource(iconBasePath + "next-bug.svg", iconColor));
    }
    if (m_actRunInTerminal) {
        m_actRunInTerminal->setIcon(createIconFromResource(iconBasePath + "run-in-terminal.svg", iconColor));
    }

    // Принудительно обновляем тулбар и все кнопки
    if (m_toolBar) {
        QList<QAction*> actions = m_toolBar->actions();
        for (QAction* action : actions) {
            if (action->isSeparator()) continue;
            // Получаем виджет кнопки
            QWidget* widget = m_toolBar->widgetForAction(action);
            if (widget) {
                // Временно скрываем и показываем для принудительного обновления
                widget->hide();
                widget->show();
                widget->update();
                widget->repaint();
            }
        }
        // Обновляем весь тулбар
        m_toolBar->update();
        m_toolBar->repaint();
    }

    // Применяем стили интерфейса
    if (theme == "dark") {
        // Темная тема
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(35, 35, 35));
        darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ToolTipBase, QColor(25, 25, 25));
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);
        
        qApp->setPalette(darkPalette);
        
        // Дополнительные стили через stylesheet
        qApp->setStyleSheet(
            "QMainWindow { background-color: #353535; }"
            "QTextEdit, QPlainTextEdit { "
                "background-color: #232323; "
                "color: #ffffff; "
                "border: 1px solid #555555; "
            "}"
            "QTextBrowser { "
                "background-color: #232323; "
                "color: #ffffff; "
                "border: 1px solid #555555; "
            "}"
            "QMenuBar { "
                "background-color: #353535; "
                "color: #ffffff; "
            "}"
            "QMenuBar::item:selected { "
                "background-color: #555555; "
            "}"
            "QMenu { "
                "background-color: #353535; "
                "color: #ffffff; "
                "border: 1px solid #555555; "
            "}"
            "QMenu::item:selected { "
                "background-color: #555555; "
            "}"
            "QToolBar { "
                "background-color: #353535; "
                "border: none; "
            "}"
            "QToolButton { "
                "background-color: transparent; "
                "color: #ffffff; "
            "}"
            "QToolButton:hover { "
                "background-color: #555555; "
            "}"
            "QDockWidget { "
                "background-color: #353535; "
                "color: #ffffff; "
            "}"
            "QDockWidget::title { "
                "background-color: #2a2a2a; "
                "color: #ffffff; "
            "}"
            "QTreeView { "
                "background-color: #232323; "
                "color: #ffffff; "
                "border: 1px solid #555555; "
            "}"
            "QStatusBar { "
                "background-color: #2a2a2a; "
                "color: #ffffff; "
            "}"
            "QTabWidget::pane { "
                "background-color: #232323; "
                "border: 1px solid #555555; "
            "}"
            "QTabBar::tab { "
                "background-color: #2a2a2a; "
                "color: #ffffff; "
                "border: 1px solid #555555; "
                "padding: 5px 15px; "
                "margin-right: 2px; "
            "}"
            "QTabBar::tab:selected { "
                "background-color: #353535; "
                "border-bottom-color: #232323; "
            "}"
            "QTabBar::tab:hover:!selected { "
                "background-color: #3a3a3a; "
            "}"
            "QMessageBox { "
                "background-color: #353535; "
                "color: #ffffff; "
            "}"
            "QMessageBox QLabel { "
                "background-color: #353535; "
                "color: #ffffff; "
            "}"
            "QMessageBox QPushButton { "
                "background-color: #555555; "
                "color: #ffffff; "
                "border: 1px solid #777777; "
                "padding: 5px 15px; "
                "min-width: 80px; "
            "}"
            "QMessageBox QPushButton:hover { "
                "background-color: #666666; "
                "border: 1px solid #888888; "
            "}"
            "QMessageBox QPushButton:pressed { "
                "background-color: #444444; "
            "}"
            "QScrollBar:vertical { "
                "background-color: #2a2a2a; "
                "width: 12px; "
                "border: none; "
            "}"
            "QScrollBar::handle:vertical { "
                "background-color: #555555; "
                "min-height: 20px; "
                "border-radius: 6px; "
                "margin: 2px; "
            "}"
            "QScrollBar::handle:vertical:hover { "
                "background-color: #666666; "
            "}"
            "QScrollBar::handle:vertical:pressed { "
                "background-color: #777777; "
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
                "height: 0px; "
            "}"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { "
                "background: none; "
            "}"
            "QScrollBar:horizontal { "
                "background-color: #2a2a2a; "
                "height: 12px; "
                "border: none; "
            "}"
            "QScrollBar::handle:horizontal { "
                "background-color: #555555; "
                "min-width: 20px; "
                "border-radius: 6px; "
                "margin: 2px; "
            "}"
            "QScrollBar::handle:horizontal:hover { "
                "background-color: #666666; "
            "}"
            "QScrollBar::handle:horizontal:pressed { "
                "background-color: #777777; "
            "}"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { "
                "width: 0px; "
            "}"
            "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { "
                "background: none; "
            "}"
        );
    } else {
        // Светлая тема (стандартная)
        qApp->setPalette(qApp->style()->standardPalette());
        qApp->setStyleSheet(
            "QScrollBar:vertical { "
                "background-color: #f0f0f0; "
                "width: 12px; "
                "border: none; "
            "}"
            "QScrollBar::handle:vertical { "
                "background-color: #c0c0c0; "
                "min-height: 20px; "
                "border-radius: 6px; "
                "margin: 2px; "
            "}"
            "QScrollBar::handle:vertical:hover { "
                "background-color: #a0a0a0; "
            "}"
            "QScrollBar::handle:vertical:pressed { "
                "background-color: #808080; "
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
                "height: 0px; "
            "}"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { "
                "background: none; "
            "}"
            "QScrollBar:horizontal { "
                "background-color: #f0f0f0; "
                "height: 12px; "
                "border: none; "
            "}"
            "QScrollBar::handle:horizontal { "
                "background-color: #c0c0c0; "
                "min-width: 20px; "
                "border-radius: 6px; "
                "margin: 2px; "
            "}"
            "QScrollBar::handle:horizontal:hover { "
                "background-color: #a0a0a0; "
            "}"
            "QScrollBar::handle:horizontal:pressed { "
                "background-color: #808080; "
            "}"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { "
                "width: 0px; "
            "}"
            "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { "
                "background: none; "
            "}"
        );
    }
    
    // Обновляем иконки управления окном в TitleBar
    if (m_titleBar) {
        m_titleBar->updateIconsForTheme(theme);
    }
    
    // Обновляем подсветку синтаксиса во всех редакторах
    if (m_tabWidget) {
        for (int i = 0; i < m_tabWidget->count(); ++i) {
            CodeEditor *editor = getEditorFromTabWidget(i);
            if (editor) {
                // Обновляем тему редактора (панель номеров и текущая строка)
                editor->setTheme(theme);
            }
            if (TurtleTaskWidget *task = turtleTaskFromWidget(m_tabWidget->widget(i))) {
                task->setTheme(theme);
            }
        }
    }
    
    // Обновляем тему справки
    if (m_helpWidget) {
        m_helpWidget->setTheme(theme);
    }
    
    // Обновляем тему игры
    if (m_snakeGame) {
        m_snakeGame->setTheme(theme);
    }
    
    // Обновляем тему редактора задач pyrob
    if (m_pyrobEditorWidget) {
        m_pyrobEditorWidget->setTheme(theme);
    }

    if (m_settingsWidget) {
        m_settingsWidget->setTheme(theme);
    }

    if (m_agentPanel) {
        m_agentPanel->setTheme(theme);
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // Проверяем наличие несохраненных изменений во всех вкладках
    if (m_tabWidget) {
        for (int i = 0; i < m_tabWidget->count(); ++i) {
            CodeEditor *editor = getEditorFromTabWidget(i);
            if (editor && editor->document()->isModified()) {
                // Переключаемся на вкладку с изменениями
                m_tabWidget->setCurrentIndex(i);
                if (!maybeSave()) {
                    event->ignore();
                    return;
                }
            }
        }
    }
    
    // Завершаем все процессы терминалов
#ifdef _WIN32
    for (QProcess *terminalProcess : m_terminalProcesses) {
        if (terminalProcess) {
            QVariant handleVar = terminalProcess->property("processHandle");
            if (handleVar.isValid()) {
                quintptr handlePtr = handleVar.value<quintptr>();
                if (handlePtr) {
                    HANDLE hProcess = reinterpret_cast<HANDLE>(handlePtr);
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                }
            }
        }
    }
#else
    for (QProcess *terminalProcess : m_terminalProcesses) {
        if (terminalProcess && terminalProcess->state() != QProcess::NotRunning) {
            terminalProcess->terminate();
            if (!terminalProcess->waitForFinished(1000)) {
                terminalProcess->kill();
            }
        }
    }
#endif
    m_terminalProcesses.clear();
    
    // Отменяем регистрацию глобального хоткея Shift+F5
#ifdef _WIN32
    if (m_terminateHotkeyRegistered) {
        HWND hwnd = reinterpret_cast<HWND>(winId());
        if (hwnd) {
            UnregisterHotKey(hwnd, HOTKEY_TERMINATE_ID);
        }
        m_terminateHotkeyRegistered = false;
    }
#endif
    
    // Сохраняем состояние окна перед закрытием
    saveWindowState();
    
    event->accept();
}

void MainWindow::saveWindowState() {
    QSettings settings;
    // Сохраняем состояние окна (геометрию и положение всех тулбаров и dock widgets)
    QByteArray state = saveState();
    settings.setValue("windowState", state);
    
    // Также сохраняем геометрию окна отдельно
    settings.setValue("geometry", saveGeometry());
    if (m_agentDock && m_agentDock->isVisible() && m_agentDock->width() >= agentDockMinWidth()) {
        settings.setValue(QStringLiteral("agent/panelWidth"), m_agentDock->width());
    }
    settings.sync();
}

void MainWindow::restoreWindowState() {
    QSettings settings;
    
    // Восстанавливаем геометрию окна
    QByteArray geometry = settings.value("geometry").toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    
    // Восстанавливаем состояние окна (положение тулбаров и dock widgets)
    QByteArray state = settings.value("windowState").toByteArray();
    if (!state.isEmpty()) {
        restoreState(state);
    }
}

void MainWindow::openSettings() {
    // Если вкладка настроек уже открыта, просто переключаемся на неё
    if (m_settingsTabIndex >= 0 && m_settingsTabIndex < m_tabWidget->count()) {
        m_tabWidget->setCurrentIndex(m_settingsTabIndex);
        if (m_settingsWidget) {
            m_settingsWidget->resetSyncTabAccessOnOpen();
        }
        return;
    }
    
    // Создаем виджет настроек, если его еще нет
    if (!m_settingsWidget) {
        m_settingsWidget = new SettingsWidget(this);
        m_settingsWidget->setTheme(m_currentTheme);
        connect(m_settingsWidget, &SettingsWidget::fontSizeChanged, this, &MainWindow::applyFontSizeToAllEditors);
        connect(m_settingsWidget, &SettingsWidget::interpreterChanged, this, [this]() {
            // Обновляем статус при изменении интерпретатора
            const QString python = detectPythonExecutable();
            statusBar()->showMessage(tr("Интерпретатор изменен: %1").arg(python), 3000);
            // Обновляем путь к Python в консоли
            if (m_console) {
                m_console->setPythonPath(python);
            }
        });
        connect(m_settingsWidget, &SettingsWidget::showStatusMessage, this,
                [this](const QString &message, int timeoutMs) {
                    statusBar()->showMessage(message, timeoutMs);
                });
        connect(m_settingsWidget, &SettingsWidget::closeRequested, this, &MainWindow::closeSettingsTab);
        connect(m_settingsWidget, &SettingsWidget::shortcutChanged, this, &MainWindow::applyShortcut);
        connect(m_settingsWidget, &SettingsWidget::showCloseButtonOnTabsChanged, this, [this](bool show) {
            if (m_tabWidget) {
                m_tabWidget->setTabsClosable(show);
            }
        });
        connect(m_settingsWidget, &SettingsWidget::syncSettingsChanged, this, [this]() {
            if (m_librarySyncManager) {
                m_librarySyncManager->reloadSettings();
            }
            LibrarySyncServerControl::applyFromSettings();
            updateSyncCountdownDisplay();
            updateAgentAvailability();
        });
        connect(m_settingsWidget, &SettingsWidget::syncCheckConnectionRequested, this, [this]() {
            if (!m_librarySyncManager) {
                return;
            }
            m_librarySyncManager->reloadSettings();
            QString message;
            if (m_librarySyncManager->checkTeacherConnection(&message)) {
                QMessageBox::information(this, tr("Синхронизация"), message);
            } else {
                QMessageBox::warning(this, tr("Синхронизация"), message);
            }
        });
        connect(m_settingsWidget, &SettingsWidget::syncForceRequested, this, [this]() {
            if (m_librarySyncManager) {
                m_librarySyncManager->reloadSettings();
                m_librarySyncManager->triggerForceSyncNow();
            }
        });
        connect(m_settingsWidget, &SettingsWidget::agentSettingsChanged, this, [this]() {
            if (m_agentPanel) {
                m_agentPanel->reloadSettings();
            }
        });
        updateAgentAvailability();
    }
    
    if (m_extensionManager && m_settingsWidget) {
        m_settingsWidget->setExtensionManager(m_extensionManager);
    }

    // Добавляем вкладку настроек (не переключаемся сразу — анимация переключит в конце)
    m_settingsTabIndex = m_tabWidget->addTab(m_settingsWidget, tr("Настройки"));
    m_settingsWidget->resetSyncTabAccessOnOpen();
    m_settingsWidget->hide(); // Скрываем до старта анимации, чтобы контент не мелькал
    animateTabOpening(m_settingsWidget, m_settingsTabIndex);
}

void MainWindow::applyFontSizeToAllEditors(int size) {
    if (!m_tabWidget) return;
    
    // Применяем размер шрифта ко всем редакторам
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        CodeEditor *editor = getEditorFromTabWidget(i);
        if (editor) {
            QFont font = editor->font();
            font.setPointSize(size);
            editor->setFont(font);
            
            // Обновляем шрифт и высоту элементов в popup автодополнения
            QCompleter *comp = editor->completer();
            if (comp && comp->popup()) {
                QAbstractItemView *popup = comp->popup();
                popup->setFont(editor->font());
                int lineHeight = editor->fontMetrics().height();
                popup->setStyleSheet(QString("QAbstractItemView::item { height: %1px; }").arg(lineHeight));
            }
        }
    }
    
    // Обновляем значение в виджете настроек, если он открыт
    if (m_settingsWidget && m_settingsWidget->m_fontSizeSpinBox) {
        m_settingsWidget->m_fontSizeSpinBox->blockSignals(true);
        m_settingsWidget->m_fontSizeSpinBox->setValue(size);
        m_settingsWidget->m_fontSizeSpinBox->blockSignals(false);
    }
}

void MainWindow::closeSettingsTab() {
    if (m_settingsTabIndex < 0 || m_settingsTabIndex >= m_tabWidget->count()) {
        return;
    }
    
    closeTab(m_settingsTabIndex);
}

void MainWindow::openHelp() {
    // Если вкладка справки уже открыта, просто переключаемся на неё
    if (m_helpTabIndex >= 0 && m_helpTabIndex < m_tabWidget->count()) {
        m_tabWidget->setCurrentIndex(m_helpTabIndex);
        return;
    }
    
    // Создаем виджет справки, если его еще нет
    if (!m_helpWidget) {
        m_helpWidget = new HelpWidget(this);
        m_helpWidget->setTheme(m_currentTheme);
    }
    
    // Добавляем вкладку справки (не переключаемся сразу — анимация переключит в конце)
    m_helpTabIndex = m_tabWidget->addTab(m_helpWidget, tr("Справка"));
    m_helpWidget->hide(); // Скрываем до старта анимации, чтобы контент не мелькал
    animateTabOpening(m_helpWidget, m_helpTabIndex);
}

void MainWindow::openPythonDocumentation()
{
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QString docPath = appDir.filePath("Python38/Doc/python3810.chm");

    if (!QFile::exists(docPath)) {
        statusBar()->showMessage(tr("Документация Python не найдена"), 4000);
        return;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(docPath))) {
        statusBar()->showMessage(tr("Не удалось открыть документацию"), 4000);
    }
}

void MainWindow::openTextbook()
{
    // Учебник и SumatraPDF лежат в папке tools/pdf (копируется при сборке)
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QString pdfDirPath = appDir.filePath("tools/pdf");
    const QDir pdfDir(pdfDirPath);

    if (!pdfDir.exists()) {
        statusBar()->showMessage(tr("Папка tools/pdf не найдена: %1").arg(pdfDirPath), 5000);
        return;
    }

    const QString pdfPath = pdfDir.filePath("Informatika_6kl_Kotov.pdf");
    QFileInfo pdfInfo(pdfPath);
    if (!pdfInfo.exists() || !pdfInfo.isFile()) {
        statusBar()->showMessage(tr("PDF файл учебника не найден: %1").arg(pdfInfo.absoluteFilePath()), 5000);
        return;
    }

    const QString sumatraName = "SumatraPDF-3.5.2-64.exe";
    const QString sumatraPath = pdfDir.filePath(sumatraName);
    if (!QFile::exists(sumatraPath)) {
        statusBar()->showMessage(tr("%1 не найден в папке tools/pdf.").arg(sumatraName), 5000);
        return;
    }

    if (!QProcess::startDetached(sumatraPath, { pdfInfo.absoluteFilePath() })) {
        statusBar()->showMessage(tr("Не удалось открыть PDF в SumatraPDF"), 5000);
        return;
    }

    statusBar()->showMessage(tr("Учебник открыт в SumatraPDF"), 3000);
}

void MainWindow::openTextbook7()
{
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QString pdfDirPath = appDir.filePath("tools/pdf");
    const QDir pdfDir(pdfDirPath);

    if (!pdfDir.exists()) {
        statusBar()->showMessage(tr("Папка tools/pdf не найдена: %1").arg(pdfDirPath), 5000);
        return;
    }

    const QString pdfPath = pdfDir.filePath("Informatika_7kl_Kotov.pdf");
    QFileInfo pdfInfo(pdfPath);
    if (!pdfInfo.exists() || !pdfInfo.isFile()) {
        statusBar()->showMessage(tr("PDF файл учебника не найден: %1").arg(pdfInfo.absoluteFilePath()), 5000);
        return;
    }

    const QString sumatraName = "SumatraPDF-3.5.2-64.exe";
    const QString sumatraPath = pdfDir.filePath(sumatraName);
    if (!QFile::exists(sumatraPath)) {
        statusBar()->showMessage(tr("%1 не найден в папке tools/pdf.").arg(sumatraName), 5000);
        return;
    }

    if (!QProcess::startDetached(sumatraPath, { pdfInfo.absoluteFilePath() })) {
        statusBar()->showMessage(tr("Не удалось открыть PDF в SumatraPDF"), 5000);
        return;
    }

    statusBar()->showMessage(tr("Учебник 7 класс открыт в SumatraPDF"), 3000);
}

void MainWindow::openPyrobEditor() {
    // Если вкладка редактора уже открыта, просто переключаемся на неё
    if (m_pyrobEditorTabIndex >= 0 && m_pyrobEditorTabIndex < m_tabWidget->count()) {
        QWidget *widget = m_tabWidget->widget(m_pyrobEditorTabIndex);
        if (widget == m_pyrobEditorWidget && m_pyrobEditorWidget) {
            m_tabWidget->setCurrentIndex(m_pyrobEditorTabIndex);
            return;
        }
    }
    
    // Убеждаемся, что старый виджет удален
    if (m_pyrobEditorWidget) {
        // Если виджет все еще существует, удаляем его
        if (m_tabWidget->indexOf(m_pyrobEditorWidget) != -1) {
            int idx = m_tabWidget->indexOf(m_pyrobEditorWidget);
            m_tabWidget->removeTab(idx);
        }
        // Отключаем все связи
        m_pyrobEditorWidget->disconnect();
        // Удаляем виджет из родителя
        if (m_pyrobEditorWidget->parent()) {
            m_pyrobEditorWidget->setParent(nullptr);
        }
        // Удаляем через deleteLater для безопасного удаления
        PyrobEditorWidget *oldWidget = m_pyrobEditorWidget;
        m_pyrobEditorWidget = nullptr;
        oldWidget->deleteLater();
        // Обрабатываем события для освобождения памяти
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    
    // Создаем новый виджет редактора
    m_pyrobEditorWidget = new PyrobEditorWidget(this);
    m_pyrobEditorWidget->setTheme(m_currentTheme);
    
    // Добавляем вкладку редактора (не переключаемся сразу — анимация переключит в конце)
    m_pyrobEditorTabIndex = m_tabWidget->addTab(m_pyrobEditorWidget, tr("Редактор задач pyrob"));
    m_pyrobEditorWidget->hide(); // Скрываем до старта анимации, чтобы контент не мелькал
    animateTabOpening(m_pyrobEditorWidget, m_pyrobEditorTabIndex);
}

void MainWindow::openSnakeGame() {
    // Если вкладка игры уже открыта, просто переключаемся на неё
    if (m_snakeGameTabIndex >= 0 && m_snakeGameTabIndex < m_tabWidget->count()) {
        m_tabWidget->setCurrentIndex(m_snakeGameTabIndex);
        return;
    }

    // Создаем контейнер и виджет игры, если их еще нет
    if (!m_snakeGameContainer) {
        m_snakeGameContainer = new QWidget(m_tabWidget);
        QVBoxLayout *layout = new QVBoxLayout(m_snakeGameContainer);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        if (!m_snakeGame) {
            m_snakeGame = new SnakeGame(m_snakeGameContainer);
            m_snakeGame->setTheme(m_currentTheme);
        } else {
            m_snakeGame->setParent(m_snakeGameContainer);
        }

        layout->addWidget(m_snakeGame);
        m_snakeGameContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_snakeGame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    // Добавляем вкладку игры (не переключаемся сразу — анимация переключит в конце)
    m_snakeGameTabIndex = m_tabWidget->addTab(m_snakeGameContainer, tr("🐍 Змейка"));
    m_snakeGameContainer->hide(); // Скрываем до старта анимации, чтобы контент не мелькал
    animateTabOpening(m_snakeGameContainer, m_snakeGameTabIndex);
}

void MainWindow::animateTabOpening(QWidget *widget, int tabIndex) {
    // Если анимации отключены, показываем виджет (он мог быть скрыт при addTab) и переключаемся на вкладку
    if (!m_animationsEnabled) {
        if (widget) widget->show();
        if (m_tabWidget && tabIndex >= 0 && tabIndex < m_tabWidget->count()) {
            m_tabWidget->setCurrentIndex(tabIndex);
        }
        return;
    }
    
    if (!widget || !m_tabWidget) return;
    
    // ВАЖНО: Останавливаем и удаляем все старые анимации ПЕРЕД созданием новых
    if (!m_activeAnimations.isEmpty()) {
        for (QPropertyAnimation *anim : m_activeAnimations) {
            if (anim) {
                anim->stop();
                anim->disconnect();
                anim->deleteLater();
            }
        }
        m_activeAnimations.clear();
        // Обрабатываем события для немедленного удаления
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    
    // Получаем текущую активную вкладку (если есть)
    int currentIndex = m_tabWidget->currentIndex();
    QWidget *currentWidget = nullptr;
    
    if (currentIndex >= 0 && currentIndex < m_tabWidget->count() && currentIndex != tabIndex) {
        currentWidget = m_tabWidget->widget(currentIndex);
    }
    
    // Используем QPointer для безопасного доступа к виджетам в lambda
    QPointer<QWidget> widgetPtr = widget;
    QPointer<QWidget> currentWidgetPtr = currentWidget;
    QPointer<QTabWidget> tabWidgetPtr = m_tabWidget;
    
    // Ждём, пока виджет получит корректную геометрию после добавления в QTabWidget
    QTimer::singleShot(50, this, [this, widgetPtr, currentWidgetPtr, tabWidgetPtr, tabIndex, currentIndex]() {
        if (!widgetPtr || !tabWidgetPtr) return;
        
        // Получаем геометрию области вкладок
        QRect tabAreaRect = tabWidgetPtr->rect();
        QTabBar *tabBar = tabWidgetPtr->tabBar();
        if (tabBar && tabBar->isVisible()) {
            tabAreaRect.setTop(tabAreaRect.top() + tabBar->height());
        }
        
        int width = tabAreaRect.width();
        int height = tabAreaRect.height();
        
        if (width <= 0 || height <= 0) {
            // Если геометрия ещё не установлена, повторяем попытку
            QTimer::singleShot(50, this, [this, widgetPtr, tabIndex]() {
                if (widgetPtr) {
                    animateTabOpening(widgetPtr, tabIndex);
                }
            });
            return;
        }
        
        // Получаем QStackedWidget
        QStackedWidget *stackWidget = nullptr;
        QList<QStackedWidget*> stacks = tabWidgetPtr->findChildren<QStackedWidget*>(QString(), Qt::FindDirectChildrenOnly);
        if (!stacks.isEmpty()) {
            stackWidget = stacks.first();
        }
        
        // Получаем родительский виджет для временного размещения виджетов
        QWidget *parentForAnimation = stackWidget ? static_cast<QWidget*>(stackWidget) : tabWidgetPtr;
        
        // Получаем РЕАЛЬНУЮ текущую геометрию currentWidget (если есть)
        QRect currentStartRect;
        if (currentWidgetPtr) {
            currentStartRect = currentWidgetPtr->geometry();
            // Если геометрия пустая, используем стандартную область
            if (currentStartRect.isEmpty() || currentStartRect.width() <= 0) {
                QPoint currentPos = currentWidgetPtr->mapTo(parentForAnimation, QPoint(0, 0));
                currentStartRect = QRect(currentPos, QSize(width, height));
            }
        }
        
        // Вычисляем позиции для анимации перелистывания вправо
        // Новая вкладка начинает справа и приезжает в позицию текущей
        QRect newEndRect = currentStartRect.isEmpty() ? QRect(0, 0, width, height) : currentStartRect;
        QRect newStartRect = newEndRect;
        newStartRect.moveLeft(newEndRect.left() + width); // Начальная позиция справа
        
        // Текущая вкладка уезжает влево
        QRect currentEndRect = currentStartRect;
        currentEndRect.moveLeft(currentStartRect.left() - width); // Уезжает влево
        
        // Сохраняем оригинальных родителей
        QWidget *newOriginalParent = widgetPtr->parentWidget();
        QWidget *currentOriginalParent = currentWidgetPtr ? currentWidgetPtr->parentWidget() : nullptr;
        
        // Отключаем обновления
        setUpdatesEnabled(false);
        tabWidgetPtr->setUpdatesEnabled(false);
        if (stackWidget) {
            stackWidget->setUpdatesEnabled(false);
        }
        widgetPtr->setUpdatesEnabled(false);
        if (currentWidgetPtr) {
            currentWidgetPtr->setUpdatesEnabled(false);
        }
        
        // ВРЕМЕННО перемещаем виджеты на уровень parentForAnimation для анимации
        widgetPtr->setParent(parentForAnimation);
        if (currentWidgetPtr) {
            currentWidgetPtr->setParent(parentForAnimation);
        }
        
        // Устанавливаем начальные позиции
        widgetPtr->setGeometry(newStartRect);
        if (currentWidgetPtr) {
            currentWidgetPtr->setGeometry(currentStartRect);
        }
        
        // Показываем оба виджета
        widgetPtr->show();
        if (currentWidgetPtr) {
            currentWidgetPtr->show();
        }
        widgetPtr->raise();
        if (currentWidgetPtr) {
            currentWidgetPtr->raise();
        }
        
        // Включаем обновления
        setUpdatesEnabled(true);
        tabWidgetPtr->setUpdatesEnabled(true);
        if (stackWidget) {
            stackWidget->setUpdatesEnabled(true);
        }
        widgetPtr->setUpdatesEnabled(true);
        if (currentWidgetPtr) {
            currentWidgetPtr->setUpdatesEnabled(true);
        }
        
        // Принудительно обновляем виджеты для корректной отрисовки содержимого
        widgetPtr->update();
        if (currentWidgetPtr) {
            currentWidgetPtr->update();
        }
        
        // Запускаем анимацию перелистывания вправо
        QPropertyAnimation *newAnimation = new QPropertyAnimation(widgetPtr, "geometry", widgetPtr);
        newAnimation->setDuration(250);
        newAnimation->setEasingCurve(QEasingCurve::InOutCubic);
        newAnimation->setStartValue(newStartRect);
        newAnimation->setEndValue(newEndRect);
        
        QPropertyAnimation *currentAnimation = nullptr;
        if (currentWidgetPtr) {
            currentAnimation = new QPropertyAnimation(currentWidgetPtr, "geometry", currentWidgetPtr);
            currentAnimation->setDuration(250);
            currentAnimation->setEasingCurve(QEasingCurve::InOutCubic);
            currentAnimation->setStartValue(currentStartRect);
            currentAnimation->setEndValue(currentEndRect);
        }
        
        // Используем QPointer для безопасного доступа в lambda
        QPointer<QStackedWidget> stackWidgetPtr = stackWidget;
        
        // После завершения анимации возвращаем виджеты обратно
        connect(newAnimation, &QPropertyAnimation::finished, [this, widgetPtr, currentWidgetPtr, tabWidgetPtr, stackWidgetPtr, newEndRect, tabIndex, newOriginalParent, currentOriginalParent]() {
            // Проверяем, что виджеты ещё существуют
            if (!widgetPtr || !tabWidgetPtr) return;
            
            // Возвращаем виджеты обратно к их оригинальным родителям
            if (newOriginalParent) {
                widgetPtr->setParent(newOriginalParent);
            }
            if (currentWidgetPtr && currentOriginalParent) {
                currentWidgetPtr->setParent(currentOriginalParent);
            }
            
            // Явно переключаем вкладку - это обновит и QTabBar (визуальное выделение) и QStackedWidget
            if (tabWidgetPtr->currentIndex() != tabIndex) {
                tabWidgetPtr->setCurrentIndex(tabIndex);
            }
            
            // Принудительно обновляем QTabBar для корректного визуального отображения
            QTabBar *tabBar = tabWidgetPtr->tabBar();
            if (tabBar) {
                tabBar->setCurrentIndex(tabIndex);
                tabBar->update();
            }
            
            // Скрываем старую вкладку
            if (currentWidgetPtr) {
                int currentIdx = tabWidgetPtr->indexOf(currentWidgetPtr);
                if (currentIdx >= 0 && currentIdx != tabWidgetPtr->currentIndex()) {
                    currentWidgetPtr->hide();
                }
            }
            
            // Новая вкладка в правильной позиции
            widgetPtr->setGeometry(newEndRect);
        });
        
        // Сохраняем ссылки на новые анимации
        m_activeAnimations.append(newAnimation);
        if (currentAnimation) {
            m_activeAnimations.append(currentAnimation);
        }
        
        // Автоматически удаляем анимации из списка при завершении
        connect(newAnimation, &QPropertyAnimation::destroyed, this, [this, newAnimation]() {
            m_activeAnimations.removeAll(newAnimation);
        });
        if (currentAnimation) {
            connect(currentAnimation, &QPropertyAnimation::destroyed, this, [this, currentAnimation]() {
                m_activeAnimations.removeAll(currentAnimation);
            });
        }
        
        // Запускаем анимации одновременно
        newAnimation->start(QAbstractAnimation::DeleteWhenStopped);
        if (currentAnimation) {
            currentAnimation->start(QAbstractAnimation::DeleteWhenStopped);
        }
    });
}

void MainWindow::animateTabSwitch(int fromIndex, int toIndex, std::function<void()> onFinished) {
    // Если анимации отключены, просто переключаем вкладку
    if (!m_animationsEnabled) {
        if (m_tabWidget && toIndex >= 0 && toIndex < m_tabWidget->count()) {
            m_tabWidget->blockSignals(false); // Разблокируем сигналы
            m_tabWidget->setCurrentIndex(toIndex);
            if (onFinished) {
                onFinished();
            }
        }
        return;
    }
    
    // ВАЖНО: Останавливаем и удаляем все старые анимации ПЕРЕД созданием новых
    if (!m_activeAnimations.isEmpty()) {
        for (QPropertyAnimation *anim : m_activeAnimations) {
            if (anim) {
                anim->stop();
                anim->disconnect();
                anim->deleteLater();
            }
        }
        m_activeAnimations.clear();
        // Обрабатываем события для немедленного удаления
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    
    if (!m_tabWidget || fromIndex < 0 || toIndex < 0 || 
        fromIndex >= m_tabWidget->count() || toIndex >= m_tabWidget->count()) {
        // Разблокируем сигналы в случае ошибки
        if (m_tabWidget) {
            m_tabWidget->blockSignals(false);
            if (toIndex >= 0 && toIndex < m_tabWidget->count()) {
                m_tabWidget->setCurrentIndex(toIndex);
            }
        }
        return;
    }
    
    // Получаем виджеты вкладок (это контейнеры)
    QWidget *fromWidget = m_tabWidget->widget(fromIndex);
    QWidget *toWidget = m_tabWidget->widget(toIndex);
    
    if (!fromWidget || !toWidget) {
        if (m_tabWidget) {
            m_tabWidget->blockSignals(false);
            m_tabWidget->setCurrentIndex(toIndex);
        }
        return;
    }
    
    // Определяем направление перехода
    bool movingRight = toIndex > fromIndex;
    
    // Получаем геометрию области вкладок ДО переключения (пока fromWidget видим)
    QRect tabAreaRect = m_tabWidget->rect();
    QTabBar *tabBar = m_tabWidget->tabBar();
    if (tabBar && tabBar->isVisible()) {
        tabAreaRect.setTop(tabAreaRect.top() + tabBar->height());
    }
    
    int width = tabAreaRect.width();
    int height = tabAreaRect.height();
    
    if (width <= 0 || height <= 0) {
        if (m_tabWidget) {
            m_tabWidget->blockSignals(false);
            m_tabWidget->setCurrentIndex(toIndex);
        }
        return;
    }
    
    // Получаем родительский виджет (stack widget внутри QTabWidget)
    QWidget *parent = fromWidget->parentWidget();
    if (!parent || parent == m_tabWidget) {
        // Ищем QStackedWidget внутри QTabWidget
        QList<QStackedWidget*> stacks = m_tabWidget->findChildren<QStackedWidget*>(QString(), Qt::FindDirectChildrenOnly);
        if (!stacks.isEmpty()) {
            parent = stacks.first();
        } else {
            parent = m_tabWidget;
        }
    }
    
    // Получаем текущую позицию fromWidget (она видима и в правильной позиции)
    QRect fromCurrentRect = fromWidget->geometry();
    if (fromCurrentRect.isEmpty() || fromCurrentRect.width() <= 0 || fromCurrentRect.height() <= 0) {
        // Если геометрия некорректна, используем область вкладок
        QPoint fromPos = fromWidget->mapTo(parent, QPoint(0, 0));
        fromCurrentRect = QRect(fromPos, QSize(width, height));
    }
    
    // Вычисляем позиции для анимации относительно родителя
    QRect fromStartRect = fromCurrentRect;
    QRect fromEndRect = fromStartRect;
    
    QRect toStartRect = fromStartRect;
    QRect toEndRect = fromStartRect;
    
    if (movingRight) {
        // Переход вправо: текущая уезжает влево, новая выезжает справа
        fromEndRect.moveLeft(fromStartRect.left() - width);
        toStartRect.moveLeft(fromStartRect.left() + width);
    } else {
        // Переход влево: текущая уезжает вправо, новая выезжает слева
        fromEndRect.moveLeft(fromStartRect.left() + width);
        toStartRect.moveLeft(fromStartRect.left() - width);
    }
    
    // Получаем QStackedWidget внутри QTabWidget
    QStackedWidget *stackWidget = nullptr;
    QList<QStackedWidget*> stacks = m_tabWidget->findChildren<QStackedWidget*>(QString(), Qt::FindDirectChildrenOnly);
    if (!stacks.isEmpty()) {
        stackWidget = stacks.first();
    }
    
    // Обновляем tab bar
    m_tabWidget->tabBar()->setCurrentIndex(toIndex);
    
    // Получаем родительский виджет для временного размещения виджетов
    QWidget *parentForAnimation = stackWidget ? static_cast<QWidget*>(stackWidget) : m_tabWidget;
    
    // Сохраняем оригинальных родителей
    QWidget *fromOriginalParent = fromWidget->parentWidget();
    QWidget *toOriginalParent = toWidget->parentWidget();
    
    // Отключаем обновления
    setUpdatesEnabled(false);
    m_tabWidget->setUpdatesEnabled(false);
    if (stackWidget) {
        stackWidget->setUpdatesEnabled(false);
    }
    fromWidget->setUpdatesEnabled(false);
    toWidget->setUpdatesEnabled(false);
    
    // ВРЕМЕННО перемещаем виджеты на уровень parentForAnimation для анимации
    fromWidget->setParent(parentForAnimation);
    toWidget->setParent(parentForAnimation);
    
    // Устанавливаем начальные позиции
    fromWidget->setGeometry(fromStartRect);
    toWidget->setGeometry(toStartRect);
    
    // Показываем оба виджета
    fromWidget->show();
    toWidget->show();
    fromWidget->raise();
    toWidget->raise();
    
    // Включаем обновления
    setUpdatesEnabled(true);
    m_tabWidget->setUpdatesEnabled(true);
    if (stackWidget) {
        stackWidget->setUpdatesEnabled(true);
    }
    fromWidget->setUpdatesEnabled(true);
    toWidget->setUpdatesEnabled(true);
    
    // Используем QPointer для безопасного доступа к виджетам в lambda
    QPointer<QWidget> fromWidgetPtr = fromWidget;
    QPointer<QWidget> toWidgetPtr = toWidget;
    QPointer<QTabWidget> tabWidgetPtr = m_tabWidget;
    QPointer<QStackedWidget> stackWidgetPtr = stackWidget;
    
    // Запускаем анимацию БЕЗ вызова setCurrentIndex
    QPropertyAnimation *fromAnimation = new QPropertyAnimation(fromWidgetPtr, "geometry", fromWidgetPtr);
    fromAnimation->setDuration(250);
    fromAnimation->setEasingCurve(QEasingCurve::InOutCubic);
    fromAnimation->setStartValue(fromStartRect);
    fromAnimation->setEndValue(fromEndRect);
    
    QPropertyAnimation *toAnimation = new QPropertyAnimation(toWidgetPtr, "geometry", toWidgetPtr);
    toAnimation->setDuration(250);
    toAnimation->setEasingCurve(QEasingCurve::InOutCubic);
    toAnimation->setStartValue(toStartRect);
    toAnimation->setEndValue(toEndRect);
    
    // После завершения анимации возвращаем виджеты обратно и переключаем вкладку
    connect(toAnimation, &QPropertyAnimation::finished, [this, fromWidgetPtr, toWidgetPtr, tabWidgetPtr, stackWidgetPtr, toEndRect, toIndex, fromOriginalParent, toOriginalParent, onFinished]() {
        // Проверяем, что виджеты ещё существуют
        if (!fromWidgetPtr || !toWidgetPtr || !tabWidgetPtr) return;
        
        // Возвращаем виджеты обратно к их оригинальным родителям
        if (fromOriginalParent) {
            fromWidgetPtr->setParent(fromOriginalParent);
        }
        if (toOriginalParent) {
            toWidgetPtr->setParent(toOriginalParent);
        }
        
        // ТЕПЕРЬ переключаем вкладку (после того, как виджеты вернулись)
        if (stackWidgetPtr) {
            stackWidgetPtr->blockSignals(true);
            stackWidgetPtr->setCurrentIndex(toIndex);
            stackWidgetPtr->blockSignals(false);
        } else {
            tabWidgetPtr->setCurrentIndex(toIndex);
        }
        
        // Разблокируем сигналы
        tabWidgetPtr->blockSignals(false);
        
        // Убеждаемся, что текущий индекс правильный
        if (tabWidgetPtr->currentIndex() != toIndex) {
            tabWidgetPtr->setCurrentIndex(toIndex);
        }
        
        // Скрываем старую вкладку
        int fromIdx = tabWidgetPtr->indexOf(fromWidgetPtr);
        if (fromIdx >= 0 && fromIdx != tabWidgetPtr->currentIndex()) {
            fromWidgetPtr->hide();
        }
        
        // Новая вкладка в правильной позиции
        toWidgetPtr->setGeometry(toEndRect);
        
        // Вызываем обработчик переключения
        onTabChanged(toIndex);
        if (onFinished) {
            onFinished();
        }
    });
    
    // Сохраняем ссылки на новые анимации
    m_activeAnimations.append(fromAnimation);
    m_activeAnimations.append(toAnimation);
    
    // Автоматически удаляем анимации из списка при завершении
    connect(fromAnimation, &QPropertyAnimation::destroyed, this, [this, fromAnimation]() {
        m_activeAnimations.removeAll(fromAnimation);
    });
    connect(toAnimation, &QPropertyAnimation::destroyed, this, [this, toAnimation]() {
        m_activeAnimations.removeAll(toAnimation);
    });
    
    // Запускаем анимации одновременно
    fromAnimation->start(QAbstractAnimation::DeleteWhenStopped);
    toAnimation->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::parseErrorAndHighlight(const QString &errorText) {
    if (m_runningFilePath.isEmpty()) {
        return;
    }
    
    
    // Ищем паттерн: File "path", line N
    // Может быть несколько вариантов:
    //   File "path", line N, in <module>
    //   File "path", line N, in function
    QRegularExpression rx(R"(File\s+["']([^"']+)["'],\s+line\s+(\d+))");
    QRegularExpressionMatchIterator it = rx.globalMatch(errorText);
    
    QList<int> errorLines;
    QFileInfo runningFile(m_runningFilePath);
    QString runningFileName = runningFile.fileName();
    QString runningAbsolutePath = runningFile.absoluteFilePath();
    
    // Собираем все строки, которые соответствуют выполняемому файлу
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString filePath = match.captured(1);
        int lineNumber = match.captured(2).toInt();
        
        // Нормализуем пути для сравнения
        QFileInfo errorFile(filePath);
        QString errorAbsolutePath = errorFile.absoluteFilePath();
        QString errorFileName = errorFile.fileName();
        
        // Сравниваем абсолютные пути или имена файлов
        if (QFileInfo(runningAbsolutePath) == QFileInfo(errorAbsolutePath) ||
            runningFileName == errorFileName) {
            int lineIndex = lineNumber - 1; // Нумерация строк в QTextDocument начинается с 0
            if (!errorLines.contains(lineIndex)) {
                errorLines.append(lineIndex);
            }
        }
    }
    
    // Если не нашли строки в traceback, пытаемся найти их по типу ошибки и анализу кода
    if (errorLines.isEmpty()) {
        CodeEditor *editor = currentEditor();
        if (editor) {
            QString editorPath = m_editorToPath.value(editor, QString());
            QFileInfo editorFile(editorPath);
            
            // Проверяем, что это тот же файл
            if (editorFile.absoluteFilePath() == runningAbsolutePath ||
                editorFile.fileName() == runningFileName) {
                
                QString code = editor->toPlainText();
                QStringList lines = code.split('\n');
                
                // Анализируем ошибки и ищем соответствующие строки в коде
                // FileNotFoundError - ищем open() или похожие операции с файлами
                if (errorText.contains("FileNotFoundError") || errorText.contains("No such file or directory")) {
                    // Сначала ищем строки с open() внутри функций
                    for (int i = 0; i < lines.size(); ++i) {
                        QString line = lines[i].trimmed();
                        // Ищем строки с open() или read_config_file() и т.д.
                        if (line.contains("open(") || line.contains("read_config_file(") || 
                            line.contains("with open(")) {
                            // Проверяем, что это не комментарий
                            if (!line.startsWith("#")) {
                                errorLines.append(i);
                            }
                        }
                    }
                    // Если не нашли, ищем вызовы функций, которые могут вызывать open()
                    if (errorLines.isEmpty()) {
                        for (int i = 0; i < lines.size(); ++i) {
                            QString line = lines[i].trimmed();
                            // Ищем вызовы функций, которые могут работать с файлами
                            if ((line.contains("read_config_file(") || line.contains("config_file(") ||
                                 line.contains("read_file(") || line.contains("load_file(")) &&
                                !line.startsWith("#")) {
                                errorLines.append(i);
                            }
                        }
                    }
                }
                
                // IndexError - ищем обращения к спискам/массивам
                if (errorText.contains("IndexError") || errorText.contains("list index out of range") ||
                    errorText.contains("index out of range")) {
                    for (int i = 0; i < lines.size(); ++i) {
                        QString line = lines[i].trimmed();
                        if (line.startsWith("#")) continue;
                        
                        // Ищем строки с обращением к элементам списка более точно
                        // lines[0], lines[1], lines[2] и т.д.
                        QRegularExpression indexRx(R"(\w+\[\s*\d+\s*\])");
                        if (indexRx.match(line).hasMatch()) {
                            if (!errorLines.contains(i)) {
                                errorLines.append(i);
                            }
                        }
                        // Также ищем прямые обращения к lines[0], lines[1], lines[2]
                        if (line.contains("lines[0]") || line.contains("lines[1]") || 
                            line.contains("lines[2]")) {
                            if (!errorLines.contains(i)) {
                                errorLines.append(i);
                            }
                        }
                    }
                }
                
                // KeyError - ищем обращения к словарям
                if (errorText.contains("KeyError")) {
                    for (int i = 0; i < lines.size(); ++i) {
                        QString line = lines[i].trimmed();
                        if (!line.startsWith("#") && line.contains("['") && line.contains("']")) {
                            errorLines.append(i);
                        }
                    }
                }
                
                // ZeroDivisionError - ищем деление
                if (errorText.contains("ZeroDivisionError") || errorText.contains("division by zero")) {
                    for (int i = 0; i < lines.size(); ++i) {
                        QString line = lines[i].trimmed();
                        if (line.startsWith("#")) continue;
                        
                        // Ищем строки с делением, особенно деление на len()
                        if ((line.contains(" / ") || line.contains("/")) && 
                            (line.contains("len(") || line.contains("/ 0") || line.contains("/0"))) {
                            if (!errorLines.contains(i)) {
                                errorLines.append(i);
                            }
                        }
                        // Также ищем паттерн sum(...) / len(...)
                        if (line.contains("sum(") && line.contains("/") && line.contains("len(")) {
                            if (!errorLines.contains(i)) {
                                errorLines.append(i);
                            }
                        }
                    }
                }
                
                // TypeError - ищем операции с неправильными типами
                if (errorText.contains("TypeError")) {
                    for (int i = 0; i < lines.size(); ++i) {
                        QString line = lines[i].trimmed();
                        if (line.startsWith("#")) continue;
                        
                        // Ищем строки с sum(), которые могут вызвать TypeError
                        if (line.contains("sum(") && !line.contains("int(") && !line.contains("float(")) {
                            if (!errorLines.contains(i)) {
                                errorLines.append(i);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Подсвечиваем все строки с ошибками в текущем редакторе
    if (!errorLines.isEmpty()) {
        CodeEditor *editor = currentEditor();
        if (editor) {
            QString editorPath = m_editorToPath.value(editor, QString());
            QFileInfo editorFile(editorPath);
            
            // Проверяем, что это тот же файл
            if (editorFile.absoluteFilePath() == runningAbsolutePath ||
                editorFile.fileName() == runningFileName) {
                editor->highlightErrorLines(errorLines);
            }
        }
    }
}

void MainWindow::toggleOutputMode() {
    if (!m_outputStack || !m_outputDock) return;
    
    m_outputModeIsConsole = !m_outputModeIsConsole;
    
    if (m_outputModeIsConsole) {
        // Переключаемся на консоль - создаем виджет при первом открытии (ленивая инициализация)
        if (!m_console) {
            m_console = new ConsoleWidget();
            QString pythonPath = detectPythonExecutable();
            m_console->setPythonPath(pythonPath);
            
            // Подключаем сигналы для парсинга ошибок из консоли
            connect(m_console, &ConsoleWidget::outputReceived, this, [this](const QString &text, bool isError) {
                if (isError) {
                    m_stderrBuffer += text;
                } else {
                    m_stdoutBuffer += text;
                }
            });
            
            connect(m_console, &ConsoleWidget::commandFinished, this, [this](int exitCode) {
                onProcessFinished(exitCode, QProcess::NormalExit);
            });
            
            // Заменяем заглушку на реальный виджет консоли
            QWidget *placeholder = m_outputStack->widget(1);
            if (placeholder) {
                m_outputStack->removeWidget(placeholder);
                placeholder->deleteLater();
            }
            m_outputStack->insertWidget(1, m_console);
        }
        
        // Запускаем консоль при первом открытии
        m_console->ensureStarted();
        m_outputStack->setCurrentIndex(1);
        QWidget *titleWidget = m_outputDock->titleBarWidget();
        if (titleWidget) {
            QLabel *titleLabel = titleWidget->findChild<QLabel *>();
            QPushButton *toggleButton = titleWidget->findChild<QPushButton *>();
            if (titleLabel) titleLabel->setText(tr("Консоль"));
            if (toggleButton) toggleButton->setText(tr("Вывод"));
        }
        m_outputDock->setWindowTitle(tr("Консоль"));
    } else {
        // Переключаемся на вывод
        m_outputStack->setCurrentIndex(0);
        QWidget *titleWidget = m_outputDock->titleBarWidget();
        if (titleWidget) {
            QLabel *titleLabel = titleWidget->findChild<QLabel *>();
            QPushButton *toggleButton = titleWidget->findChild<QPushButton *>();
            if (titleLabel) titleLabel->setText(tr("Вывод"));
            if (toggleButton) toggleButton->setText(tr("Консоль"));
        }
        m_outputDock->setWindowTitle(tr("Вывод"));
    }
}

void MainWindow::applySnapOnRelease() {
#ifdef _WIN32
    if (m_isMaximized || isFullScreen()) {
        return;
    }
    
    // Получаем геометрию экрана, на котором находится окно
    QScreen *screen = QApplication::screenAt(geometry().center());
    if (!screen) {
        screen = QApplication::primaryScreen();
    }
    if (!screen) {
        return;
    }
    
    QRect screenGeometry = screen->availableGeometry();
    QPoint cursorPos = QCursor::pos();
    
    const int snapThreshold = 20; // Порог в пикселях для активации snap
    
    // Проверяем, находится ли курсор у края экрана в момент отпускания
    bool nearLeftEdge = cursorPos.x() <= screenGeometry.left() + snapThreshold;
    bool nearRightEdge = cursorPos.x() >= screenGeometry.right() - snapThreshold;
    bool nearTopEdge = cursorPos.y() <= screenGeometry.top() + snapThreshold;
    
    // Сохраняем нормальную геометрию перед применением snap
    QRect currentGeometry = geometry();
    bool isCurrentlyFullScreen = (currentGeometry == screenGeometry);
    
    // Применяем snap-эффект в зависимости от края
    if (nearTopEdge) {
        // Перед snap на весь экран ВСЕГДА сохраняем текущую геометрию, если окно еще не развернуто на весь экран
        if (!isCurrentlyFullScreen) {
            m_normalGeometry = currentGeometry;
        }
        // Максимизируем окно (настоящая максимизация, а не просто изменение размера)
        showMaximized();
        m_isMaximized = true;
        m_isSnapped = false; // При максимизации snap не используется
    }
    else if (nearLeftEdge) {
        // Перед snap на левую половину ВСЕГДА сохраняем текущую геометрию, если она не равна половине экрана
        QRect leftHalf = screenGeometry;
        leftHalf.setWidth(screenGeometry.width() / 2);
        const int tolerance = 2;
        bool isAlreadyLeftHalf = (qAbs(currentGeometry.left() - leftHalf.left()) <= tolerance &&
                                 qAbs(currentGeometry.width() - leftHalf.width()) <= tolerance);
        // Сохраняем геометрию, если она не равна левой половине экрана
        if (!isAlreadyLeftHalf) {
            m_normalGeometry = currentGeometry;
        }
        // Разворачиваем на левую половину экрана
        setGeometry(leftHalf);
        m_isSnapped = true;
    }
    else if (nearRightEdge) {
        // Перед snap на правую половину ВСЕГДА сохраняем текущую геометрию, если она не равна половине экрана
        QRect rightHalf = screenGeometry;
        rightHalf.setLeft(screenGeometry.left() + screenGeometry.width() / 2);
        const int tolerance = 2;
        bool isAlreadyRightHalf = (qAbs(currentGeometry.left() - rightHalf.left()) <= tolerance &&
                                  qAbs(currentGeometry.width() - rightHalf.width()) <= tolerance);
        // Сохраняем геометрию, если она не равна правой половине экрана
        if (!isAlreadyRightHalf) {
            m_normalGeometry = currentGeometry;
        }
        // Разворачиваем на правую половину экрана (НЕ на весь экран!)
        setGeometry(rightHalf);
        m_isSnapped = true;
    }
    else {
        // Курсор не у края - сбрасываем snap-состояние, если оно было
        if (m_isSnapped) {
            m_isSnapped = false;
            // Восстанавливаем нормальную геометрию, если она была сохранена
            if (m_normalGeometry.isValid() && !m_normalGeometry.isEmpty()) {
                setGeometry(m_normalGeometry);
                m_normalGeometry = QRect();
            }
        }
    }
#endif
}

void MainWindow::setupLocalServer()
{
    // Создаем локальный сервер для единого экземпляра приложения
    QString serverName = "VuzhykIDE_SingleInstance";
    m_localServer = new QLocalServer(this);
    
    // Удаляем старый сервер, если он существует
    QLocalServer::removeServer(serverName);
    
    // Пытаемся запустить сервер
    if (m_localServer->listen(serverName)) {
        connect(m_localServer, &QLocalServer::newConnection, this, &MainWindow::onNewConnection);
    }
}

void MainWindow::onNewConnection()
{
    // Обрабатываем новое подключение от другого экземпляра
    QLocalSocket *socket = m_localServer->nextPendingConnection();
    if (!socket) return;
    
    connect(socket, &QLocalSocket::readyRead, this, &MainWindow::readSocketData);
    connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
}

void MainWindow::readSocketData()
{
    // Читаем данные из сокета (пути к файлам для открытия)
    QLocalSocket *socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket) return;
    
    QByteArray data = socket->readAll();
    QStringList filePaths = QString::fromUtf8(data).split('\n', QString::SkipEmptyParts);
    
    // Открываем файлы
    for (const QString &filePath : filePaths) {
        QString cleanPath = filePath.trimmed();
        if (!cleanPath.isEmpty()) {
            openFileFromPath(cleanPath);
        }
    }
    
    // Активируем окно
    raise();
    activateWindow();
    show();
    setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
}

void MainWindow::updateSyncCountdownDisplay()
{
    if (!m_syncCountdownLabel || !m_librarySyncManager) {
        return;
    }

    const bool shouldShow = m_librarySyncManager->showSyncTimer()
        && m_librarySyncManager->isSyncEnabled()
        && m_librarySyncManager->isStudentMode()
        && !m_librarySyncManager->isSyncInProgress();

    if (!shouldShow) {
        m_syncCountdownLabel->setVisible(false);
        return;
    }

    const int totalSeconds = m_librarySyncManager->secondsUntilNextSync();
    if (totalSeconds < 0) {
        m_syncCountdownLabel->setVisible(false);
        return;
    }

    const int minutes = totalSeconds / 60;
    const int seconds = totalSeconds % 60;
    m_syncCountdownLabel->setText(tr("Синхронизация через %1:%2")
                                      .arg(minutes, 2, 10, QChar('0'))
                                      .arg(seconds, 2, 10, QChar('0')));
    m_syncCountdownLabel->setVisible(true);
}

void MainWindow::setupLibrarySync()
{
    m_librarySyncManager = new LibrarySyncManager(this);
    LibrarySyncServerControl::applyFromSettings();

    m_syncCountdownTimer = new QTimer(this);
    m_syncCountdownTimer->setInterval(1000);
    connect(m_syncCountdownTimer, &QTimer::timeout, this, &MainWindow::updateSyncCountdownDisplay);
    m_syncCountdownTimer->start();

    connect(m_librarySyncManager, &LibrarySyncManager::syncStarted, this, [this]() {
        if (m_syncCountdownLabel) {
            m_syncCountdownLabel->setVisible(false);
        }
        if (m_syncProgressBar) {
            m_syncProgressBar->setValue(0);
            m_syncProgressBar->setVisible(true);
        }
        if (m_syncProgressLabel) {
            m_syncProgressLabel->setText(tr("Синхронизация"));
            m_syncProgressLabel->setVisible(true);
        }
    });

    connect(m_librarySyncManager, &LibrarySyncManager::syncProgress, this,
            [this](int current, int total, const QString &packageName) {
                if (m_syncProgressBar) {
                    m_syncProgressBar->setMaximum(total);
                    m_syncProgressBar->setValue(current);
                    m_syncProgressBar->setFormat(tr("%v / %m"));
                    m_syncProgressBar->setVisible(true);
                }
                if (m_syncProgressLabel) {
                    m_syncProgressLabel->setText(tr("Синхронизация: %1").arg(packageName));
                    m_syncProgressLabel->setVisible(true);
                }
            });

    connect(m_librarySyncManager, &LibrarySyncManager::syncFinished, this,
            [this](bool success, const QString &message) {
                if (m_syncProgressBar) {
                    m_syncProgressBar->setVisible(false);
                }
                if (m_syncProgressLabel) {
                    m_syncProgressLabel->setVisible(false);
                }
                statusBar()->showMessage(message, success ? 5000 : 8000);
                updateSyncCountdownDisplay();
            });

    connect(m_librarySyncManager, &LibrarySyncManager::studentAgentAccessChanged,
            this, &MainWindow::updateAgentAvailability);

    updateSyncCountdownDisplay();
    updateAgentAvailability();
}

