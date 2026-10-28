#include "DemoIdeWindow.h"
#include "DemoTurtleTask.h"
#include "DemoTurtleCanvas.h"
#include "DemoCursor.h"
#include "DemoIcons.h"
#include "DemoScale.h"
#include "DemoRecorder.h"

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QStringList>
#include <QTextCursor>
#include <QSizePolicy>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QEasingCurve>
#include <QtMath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

DemoIdeWindow::DemoIdeWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setWindowTitle(QStringLiteral("Vuzhyk"));
    resize(1440, 900);
    buildChrome();

    m_cursor = new DemoCursor(this);
    m_typeTimer = new QTimer(this);
    m_typeTimer->setInterval(32);
    connect(m_typeTimer, &QTimer::timeout, this, &DemoIdeWindow::typeNext);

    m_cursorAnim = new QPropertyAnimation(m_cursor, "hotPos", this);
    m_cursorAnim->setDuration(900);
    m_cursorAnim->setEasingCurve(QEasingCurve::InOutCubic);
    connect(m_cursorAnim, &QPropertyAnimation::finished, this, &DemoIdeWindow::onCursorArrived);

    buildHouseScenes();
    prepareScene(0);

    if (m_task && m_task->canvas()) {
        connect(m_task->canvas(), &DemoTurtleCanvas::playbackFinished,
                this, &DemoIdeWindow::onPlaybackFinished);
    }

    m_recorder = new DemoRecorder(this);
}

DemoIdeWindow::~DemoIdeWindow()
{
    if (m_recorder)
        m_recorder->stop();
}

void DemoIdeWindow::buildChrome()
{
    m_titleBar = new QWidget(this);
    m_titleBar->setObjectName(QStringLiteral("CustomTitleBar"));
    m_titleBar->setFixedHeight(ds(26));
    m_titleBar->installEventFilter(this);
    auto *titleLayout = new QHBoxLayout(m_titleBar);
    titleLayout->setContentsMargins(ds(6), 0, 0, 0);
    titleLayout->setSpacing(1);

    auto *iconBtn = new QPushButton(m_titleBar);
    iconBtn->setIcon(QIcon(QStringLiteral(":/icons/icons/logo/vuzhyk.ico")));
    iconBtn->setIconSize(dsSize(20, 20));
    iconBtn->setFixedSize(dsSize(22, 22));
    iconBtn->setFlat(true);
    iconBtn->setStyleSheet(QStringLiteral(
        "QPushButton { border: none; background: transparent; }"));
    titleLayout->addWidget(iconBtn, 0);

    m_menuBar = new QMenuBar(m_titleBar);
    m_menuBar->setNativeMenuBar(false);
    m_menuBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_menuBar->setFixedHeight(ds(26));
    QFont mf = m_menuBar->font();
    if (mf.pointSizeF() > 0)
        mf.setPointSizeF(mf.pointSizeF() * 1.2);
    m_menuBar->setFont(mf);
    m_menuBar->setStyleSheet(QStringLiteral(
        "QMenuBar{background:transparent;border:none;padding:0;margin:0;min-height:%1px;}"
        "QMenuBar::item{padding:0 %2px;margin:0;border:0;min-height:%1px;}"
        "QMenuBar::item:selected{background:rgba(0,0,0,0.06);}")
        .arg(ds(26)).arg(ds(12)));

    auto addDummyMenu = [this](const QString &title, const QStringList &items) {
        QMenu *menu = m_menuBar->addMenu(title);
        for (const QString &item : items)
            menu->addAction(item);
    };
    addDummyMenu(QString::fromUtf8("Файл"), {
        QString::fromUtf8("Новый"), QString::fromUtf8("Открыть..."),
        QString::fromUtf8("Сохранить"), QString::fromUtf8("Выход") });
    addDummyMenu(QString::fromUtf8("Запуск"), { QString::fromUtf8("Запустить скрипт") });
    addDummyMenu(QString::fromUtf8("Инструменты"), {
        QString::fromUtf8("Выбрать Python..."), QString::fromUtf8("Переключить тему") });
    addDummyMenu(QString::fromUtf8("Задачи"), { QString::fromUtf8("Задачник черепаха") });
    addDummyMenu(QString::fromUtf8("Помощь"), { QString::fromUtf8("Справка") });
    m_menuBar->addAction(QString::fromUtf8("Агент"));
    titleLayout->addWidget(m_menuBar, 1);

    auto makeWinBtn = [this, titleLayout](const QString &res) {
        auto *btn = new QToolButton(m_titleBar);
        btn->setIcon(demoThemedIcon(res, QStringLiteral("#333333"), dsSize(21, 21)));
        btn->setIconSize(dsSize(21, 21));
        btn->setFixedSize(dsSize(36, 26));
        btn->setAutoRaise(true);
        btn->setStyleSheet(QStringLiteral(
            "QToolButton{background:transparent;border:none;padding:0;}"
            "QToolButton:hover{background:rgba(0,0,0,0.08);}"));
        titleLayout->addWidget(btn, 0);
        return btn;
    };
    m_minBtn = makeWinBtn(QStringLiteral(":/icons/icons/window-controls/window-minimize.svg"));
    m_maxBtn = makeWinBtn(QStringLiteral(":/icons/icons/window-controls/window-maximize.svg"));
    m_closeBtn = makeWinBtn(QStringLiteral(":/icons/icons/window-controls/window-close.svg"));
    connect(m_minBtn, &QToolButton::clicked, this, &QWidget::showMinimized);
    connect(m_maxBtn, &QToolButton::clicked, this, [this]() {
        if (isMaximized()) {
            showNormal();
            m_maxBtn->setIcon(demoThemedIcon(
                QStringLiteral(":/icons/icons/window-controls/window-maximize.svg"),
                m_winIconColor, dsSize(21, 21)));
        } else {
            showMaximized();
            m_maxBtn->setIcon(demoThemedIcon(
                QStringLiteral(":/icons/icons/window-controls/window-restore.svg"),
                m_winIconColor, dsSize(21, 21)));
        }
    });
    connect(m_closeBtn, &QToolButton::clicked, this, &QWidget::close);
    setMenuWidget(m_titleBar);

    m_toolBar = addToolBar(QString::fromUtf8("Основное"));
    m_toolBar->setMovable(false);
    m_toolBar->setIconSize(dsSize(16, 16));
    const QString icons = QStringLiteral(":/icons/icons/light-theme/");
    auto addTb = [this, icons](const QString &file, const QString &tip) {
        QAction *a = m_toolBar->addAction(
            demoThemedIcon(icons + file, QStringLiteral("#000000"), dsSize(16, 16)), tip);
        a->setEnabled(true);
        return a;
    };
    addTb(QStringLiteral("file-new.svg"), QString::fromUtf8("Новый"));
    addTb(QStringLiteral("file-open.svg"), QString::fromUtf8("Открыть"));
    addTb(QStringLiteral("file-save.svg"), QString::fromUtf8("Сохранить"));
    addTb(QStringLiteral("file-save-all.svg"), QString::fromUtf8("Сохранить все"));
    m_toolBar->addSeparator();
    addTb(QStringLiteral("cut.svg"), QString::fromUtf8("Вырезать"));
    addTb(QStringLiteral("copy.svg"), QString::fromUtf8("Копировать"));
    addTb(QStringLiteral("paste-clipboard.svg"), QString::fromUtf8("Вставить"));
    m_toolBar->addSeparator();
    addTb(QStringLiteral("undo.svg"), QString::fromUtf8("Отменить"));
    addTb(QStringLiteral("redo.svg"), QString::fromUtf8("Восстановить"));
    m_toolBar->addSeparator();
    addTb(QStringLiteral("play.svg"), QString::fromUtf8("Запустить"));
    addTb(QStringLiteral("stop.svg"), QString::fromUtf8("Завершить"));
    addTb(QStringLiteral("bug.svg"), QString::fromUtf8("Отладка"));
    addTb(QStringLiteral("next-bug.svg"), QString::fromUtf8("Шаг отладки"));
    addTb(QStringLiteral("run-in-terminal.svg"), QString::fromUtf8("В браузере"));

    m_tabs = new QTabWidget(this);
    m_tabs->setTabsClosable(false);
    m_tabs->setMovable(false);
    m_task = new DemoTurtleTask(m_tabs);
    m_tabs->addTab(m_task, QString::fromUtf8("Задачник черепаха"));
    setCentralWidget(m_tabs);
    statusBar()->hide();

    qApp->setStyleSheet(qApp->styleSheet() + QStringLiteral(
        "QScrollBar:vertical { background-color: #f0f0f0; width: %1px; border: none; }"
        "QScrollBar::handle:vertical { background-color: #c0c0c0; min-height: %2px;"
        " border-radius: %3px; margin: 2px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QMainWindow { background: #f3f3f3; }"
        "QTabWidget::pane { border: 1px solid #d0d0d0; background: #ffffff; }"
        "QTabBar::tab { background: #e8e8e8; padding: %4px %5px; margin-right: 2px;"
        " border: 1px solid #d0d0d0; }"
        "QTabBar::tab:selected { background: #ffffff; border-bottom-color: #ffffff; }"
        "QStatusBar { background: #f0f0f0; }"
        "QToolBar { background: #f3f3f3; border: none; spacing: %6px; padding: %6px; }"
        "QDockWidget::title { background: #ececec; padding: %4px; }"
    ).arg(ds(12)).arg(ds(20)).arg(ds(6)).arg(ds(5)).arg(ds(15)).arg(ds(2)));
}

void DemoIdeWindow::applyDarkTheme()
{
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

    m_winIconColor = QStringLiteral("#e0e0e0");
    if (m_titleBar)
        m_titleBar->setStyleSheet(QStringLiteral("QWidget#CustomTitleBar { background: #353535; }"));
    if (m_menuBar) {
        m_menuBar->setStyleSheet(QStringLiteral(
            "QMenuBar{background:transparent;color:#ffffff;border:none;padding:0;margin:0;min-height:%1px;}"
            "QMenuBar::item{color:#ffffff;padding:0 %2px;margin:0;border:0;min-height:%1px;}"
            "QMenuBar::item:selected{background:#555555;}")
            .arg(ds(26)).arg(ds(12)));
    }
    auto restyleWinBtn = [this](QToolButton *btn, const QString &res) {
        if (!btn)
            return;
        btn->setIcon(demoThemedIcon(res, m_winIconColor, dsSize(21, 21)));
        btn->setStyleSheet(QStringLiteral(
            "QToolButton{background:transparent;border:none;padding:0;}"
            "QToolButton:hover{background:rgba(255,255,255,0.10);}"));
    };
    restyleWinBtn(m_minBtn, QStringLiteral(":/icons/icons/window-controls/window-minimize.svg"));
    restyleWinBtn(m_maxBtn, isMaximized()
        ? QStringLiteral(":/icons/icons/window-controls/window-restore.svg")
        : QStringLiteral(":/icons/icons/window-controls/window-maximize.svg"));
    restyleWinBtn(m_closeBtn, QStringLiteral(":/icons/icons/window-controls/window-close.svg"));

    if (m_toolBar) {
        const QString icons = QStringLiteral(":/icons/icons/light-theme/");
        static const QString files[] = {
            QStringLiteral("file-new.svg"), QStringLiteral("file-open.svg"),
            QStringLiteral("file-save.svg"), QStringLiteral("file-save-all.svg"),
            QStringLiteral("cut.svg"), QStringLiteral("copy.svg"),
            QStringLiteral("paste-clipboard.svg"), QStringLiteral("undo.svg"),
            QStringLiteral("redo.svg"), QStringLiteral("play.svg"),
            QStringLiteral("stop.svg"), QStringLiteral("bug.svg"),
            QStringLiteral("next-bug.svg"), QStringLiteral("run-in-terminal.svg")
        };
        int i = 0;
        const int n = int(sizeof(files) / sizeof(files[0]));
        for (QAction *a : m_toolBar->actions()) {
            if (!a || a->isSeparator() || i >= n)
                continue;
            a->setIcon(demoThemedIcon(icons + files[i++], m_winIconColor, dsSize(16, 16)));
        }
    }

    qApp->setStyleSheet(QStringLiteral(
        "QMainWindow { background-color: #353535; }"
        "QMenuBar { background-color: #353535; color: #ffffff; }"
        "QMenuBar::item:selected { background-color: #555555; }"
        "QMenu { background-color: #353535; color: #ffffff; border: 1px solid #555555; }"
        "QMenu::item:selected { background-color: #555555; }"
        "QToolBar { background-color: #353535; border: none; spacing: %1px; padding: %1px; }"
        "QToolButton { background-color: transparent; color: #ffffff; }"
        "QToolButton:hover { background-color: #555555; }"
        "QTabWidget::pane { background-color: #232323; border: 1px solid #555555; }"
        "QTabBar::tab { background-color: #2a2a2a; color: #ffffff;"
        " border: 1px solid #555555; padding: %2px %3px; margin-right: 2px; }"
        "QTabBar::tab:selected { background-color: #353535; border-bottom-color: #232323; }"
        "QTabBar::tab:hover:!selected { background-color: #3a3a3a; }"
        "QStatusBar { background-color: #2a2a2a; color: #ffffff; }"
        "QSplitter::handle { background: #555555; }"
        "QScrollBar:vertical { background-color: #2a2a2a; width: %4px; border: none; }"
        "QScrollBar::handle:vertical { background-color: #555555; min-height: %5px;"
        " border-radius: %6px; margin: 2px; }"
        "QScrollBar::handle:vertical:hover { background-color: #666666; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
        "QScrollBar:horizontal { background-color: #2a2a2a; height: %4px; border: none; }"
        "QScrollBar::handle:horizontal { background-color: #555555; min-width: %5px;"
        " border-radius: %6px; margin: 2px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }"
    ).arg(ds(2)).arg(ds(5)).arg(ds(15)).arg(ds(12)).arg(ds(20)).arg(ds(6)));

    if (m_task)
        m_task->setTheme(QStringLiteral("dark"));
}

bool DemoIdeWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_titleBar && event) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragging = true;
                m_dragOffset = me->globalPos() - frameGeometry().topLeft();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_dragging) {
            auto *me = static_cast<QMouseEvent *>(event);
            move(me->globalPos() - m_dragOffset);
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_dragging = false;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void DemoIdeWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_cursor)
        m_cursor->raise();
}

void DemoIdeWindow::buildHouseScenes()
{
    Scene house;
    house.taskNumber = 6;
    house.instruction = QString::fromUtf8(
        "Нарисуй домик по образцу: квадрат — это стены, сверху — треугольная крыша. "
        "Синяя линия на экране показывает, каким должен получиться рисунок.");
    house.script = QString::fromUtf8(
        "from turtle import *\n"
        "\n"
        "for i in range(4):\n"
        "    forward(100)\n"
        "    left(90)\n"
        "\n"
        "left(90)\n"
        "forward(100)\n"
        "right(45)\n"
        "forward(71)\n"
        "right(90)\n"
        "forward(71)\n");
    house.path = housePath();
    m_scenes.append(house);

    Scene corner;
    corner.taskNumber = 4;
    corner.instruction = QString::fromUtf8(
        "Черепашка умеет поворачивать. Добавь поворот налево на 90 градусов "
        "и ещё один шаг вперёд на 70, чтобы получился уголок как на образце.");
    corner.script = QString::fromUtf8(
        "from turtle import *\n"
        "forward(70)\n"
        "left(90)\n"
        "forward(70)\n");
    corner.path = pathFromSpec(QStringLiteral("F70 L90 F70"));
    m_scenes.append(corner);

    Scene line;
    line.taskNumber = 1;
    line.instruction = QString::fromUtf8(
        "Перед тобой готовая программа. Она перемещает черепашку на 100 шагов вперёд. "
        "Синяя линия на экране — это образец результата. Выполни программу и посмотри, "
        "как черепашка рисует свою линию поверх образца.");
    line.script = QString::fromUtf8(
        "from turtle import *\n"
        "forward(100)\n");
    line.path = pathFromSpec(QStringLiteral("F100"));
    m_scenes.append(line);
}

void DemoIdeWindow::prepareScene(int index)
{
    if (index < 0 || index >= m_scenes.size() || !m_task)
        return;
    const Scene &scene = m_scenes.at(index);
    m_sceneIndex = index;
    m_script = scene.script;
    m_typeIndex = 0;
    m_task->setCatalogSelected(scene.taskNumber);
    m_task->setInstruction(scene.instruction, QStringLiteral("hint"));
    if (m_task->editor()) {
        m_task->editor()->setPlainText(scene.prefill);
        m_task->editor()->moveCursor(QTextCursor::End);
    }
    if (m_task->canvas()) {
        m_task->canvas()->resetStudent();
        m_task->canvas()->setSamplePath(scene.path);
    }
}

void DemoIdeWindow::beginHouseDemo(const QString &recordPrefix, bool prefillFirstTask)
{
    m_drawDurationMs = 0;
    m_afterOkMs = 1800;
    m_startDelayMs = 1100;
    m_afterTypeMs = 500;
    m_cursorMs = 900;
    m_clickMs = 140;
    m_hideCursorMs = 350;
    m_afterSelectMs = 800;
    m_typeChunk = 1;
    m_typeIntervalMs = 0;
    m_endHoldMs = 0;
    if (prefillFirstTask && !m_scenes.isEmpty()) {
        Scene &house = m_scenes[0];
        house.prefill = QString::fromUtf8(
            "from turtle import *\n"
            "\n"
            "for i in range(4):\n"
            "    forward(100)\n"
            "    left(90)\n"
            "\n"
            "left(90)\n"
            "forward(100)\n"
            "right(45)\n"
            "forward(71)\n");
        house.script = QString::fromUtf8(
            "right(90)\n"
            "forward(71)\n");
    }
    prepareScene(0);
    if (m_recorder)
        m_recorder->start(this, recordPrefix);
    QTimer::singleShot(m_startDelayMs, this, [this]() { m_typeTimer->start(); });
}

void DemoIdeWindow::beginSimpleTasksDemo()
{
    applyDarkTheme();
    m_drawDurationMs = 1250;
    m_afterOkMs = 250;
    m_startDelayMs = 400;
    m_afterTypeMs = 120;
    m_cursorMs = 560;
    m_clickMs = 80;
    m_hideCursorMs = 50;
    m_afterSelectMs = 120;
    m_typeChunk = 1;
    m_typeIntervalMs = 20;
    m_endHoldMs = 180;
    buildSimpleScenes();
    prepareScene(0);
    if (m_recorder)
        m_recorder->start(this, QStringLiteral("DemoTurtleTasks"));
    m_typeTimer->setInterval(m_typeIntervalMs);
    QTimer::singleShot(m_startDelayMs, this, [this]() { m_typeTimer->start(); });
}

void DemoIdeWindow::buildSimpleScenes()
{
    m_scenes.clear();

    Scene square;
    square.taskNumber = 2;
    square.instruction = QString::fromUtf8(
        "Нарисуй квадрат со стороной 75. Поверни налево на 90 градусов после каждой стороны. "
        "Синяя линия на экране — образец.");
    square.prefill = QString::fromUtf8(
        "from turtle import *\n"
        "\n"
        "for i in range(4):\n");
    square.script = QString::fromUtf8(
        "    forward(75)\n"
        "    left(90)\n");
    square.path = pathFromSpec(QStringLiteral("F75 L90 F75 L90 F75 L90 F75 L90"),
                               QPointF(-37.5, -37.5));
    m_scenes.append(square);

    Scene triangle;
    triangle.taskNumber = 3;
    triangle.instruction = QString::fromUtf8(
        "Нарисуй равносторонний треугольник: три стороны по 100 шагов, "
        "после каждой стороны поворот налево на 120 градусов.");
    triangle.prefill = QString::fromUtf8(
        "from turtle import *\n"
        "\n"
        "for i in range(3):\n");
    triangle.script = QString::fromUtf8(
        "    forward(100)\n"
        "    left(120)\n");
    triangle.path = pathFromSpec(QStringLiteral("F100 L120 F100 L120 F100 L120"),
                                 QPointF(-50.0, -28.9));
    m_scenes.append(triangle);

    Scene rect;
    rect.taskNumber = 5;
    rect.instruction = QString::fromUtf8(
        "Нарисуй прямоугольник: длинные стороны 120 шагов, короткие — 40. "
        "После каждой стороны поворачивай налево на 90 градусов.");
    rect.prefill = QString::fromUtf8(
        "from turtle import *\n"
        "\n"
        "for i in range(2):\n"
        "    forward(120)\n"
        "    left(90)\n");
    rect.script = QString::fromUtf8(
        "    forward(40)\n"
        "    left(90)\n");
    rect.path = pathFromSpec(QStringLiteral("F120 L90 F40 L90 F120 L90 F40 L90"),
                             QPointF(-60.0, -20.0));
    m_scenes.append(rect);

    Scene hex;
    hex.taskNumber = 6;
    hex.instruction = QString::fromUtf8(
        "Нарисуй правильный шестиугольник: шесть сторон по 50 шагов, "
        "после каждой стороны поворот налево на 60 градусов.");
    hex.prefill = QString::fromUtf8(
        "from turtle import *\n"
        "\n"
        "for i in range(6):\n");
    hex.script = QString::fromUtf8(
        "    forward(50)\n"
        "    left(60)\n");
    hex.path = pathFromSpec(QStringLiteral("F50 L60 F50 L60 F50 L60 F50 L60 F50 L60 F50 L60"),
                            QPointF(-25.0, -43.3));
    m_scenes.append(hex);
}

void DemoIdeWindow::typeNext()
{
    if (m_typeIndex >= m_script.size()) {
        m_typeTimer->stop();
        QTimer::singleShot(m_afterTypeMs, this, [this]() {
            if (m_task)
                moveCursorTo(m_task->runButton(), CursorRun);
        });
        return;
    }
    const int n = qBound(1, m_typeChunk, m_script.size() - m_typeIndex);
    const QString chunk = m_script.mid(m_typeIndex, n);
    m_typeIndex += n;
    if (m_task)
        m_task->appendTyped(chunk);
    if (m_typeIntervalMs > 0)
        m_typeTimer->setInterval(m_typeIntervalMs);
    else if (chunk.contains(QLatin1Char('\n')))
        m_typeTimer->setInterval(90);
    else if (chunk.contains(QLatin1Char(' ')))
        m_typeTimer->setInterval(22);
    else
        m_typeTimer->setInterval(28 + (m_typeIndex % 5));
}

QPoint DemoIdeWindow::cursorHotFor(QWidget *widget) const
{
    if (!widget)
        return QPoint(width() / 2, height() / 2);
    const QPoint local = widget->rect().center();
    return widget->mapTo(const_cast<DemoIdeWindow *>(this), local);
}

QPoint DemoIdeWindow::appearFromEditor() const
{
    QPlainTextEdit *editor = m_task ? m_task->editor() : nullptr;
    if (!editor)
        return QPoint(width() / 3, height() / 3);

    QTextCursor tc = editor->textCursor();
    tc.movePosition(QTextCursor::End);
    QPoint local = editor->viewport()->mapTo(editor, editor->cursorRect(tc).center());

    const int vx[] = { 42, -110, 168, -36, 88, 12 };
    const int vy[] = { -12, 48, 28, 86, 18, -32 };
    const int i = qAbs(m_sceneIndex) % 6;
    local += QPoint(ds(vx[i]), ds(vy[i]));
    local.setX(qBound(ds(18), local.x(), qMax(ds(20), editor->width() - ds(20))));
    local.setY(qBound(ds(18), local.y(), qMax(ds(20), editor->height() - ds(20))));
    return editor->mapTo(const_cast<DemoIdeWindow *>(this), local);
}

void DemoIdeWindow::moveCursorTo(QWidget *widget, CursorTarget target)
{
    if (!m_cursor || !widget || !m_cursorAnim)
        return;
    QPoint start = m_cursor->hotPos();
    if (!m_cursor->isVisible()) {
        if (target == CursorRun)
            start = appearFromEditor();
        else if (start.isNull())
            start = appearFromEditor();
    }
    const QPoint end = cursorHotFor(widget);
    m_cursorTarget = target;
    m_cursor->setPressed(false);
    m_cursor->setHotPos(start);
    m_cursor->show();
    m_cursor->raise();
    m_ignoreCursorFinish = true;
    m_cursorAnim->stop();
    m_ignoreCursorFinish = false;
    m_cursorAnim->setStartValue(start);
    m_cursorAnim->setEndValue(end);
    m_cursorAnim->setDuration(m_cursorMs);
    m_cursorAnim->start();
}

void DemoIdeWindow::onCursorArrived()
{
    if (m_ignoreCursorFinish)
        return;
    if (m_cursorTarget == CursorRun)
        clickRun();
    else if (m_cursorTarget == CursorTask)
        clickTaskButton();
    m_cursorTarget = CursorNone;
}

void DemoIdeWindow::clickRun()
{
    if (!m_cursor || !m_task || !m_task->runButton())
        return;
    m_cursor->setPressed(true);
    m_task->runButton()->setDown(true);
    QTimer::singleShot(m_clickMs, this, [this]() {
        if (m_task && m_task->runButton())
            m_task->runButton()->setDown(false);
        if (m_cursor)
            m_cursor->setPressed(false);
        startDrawing();
        QTimer::singleShot(m_hideCursorMs, this, [this]() {
            if (m_cursor)
                m_cursor->hide();
        });
    });
}

void DemoIdeWindow::clickTaskButton()
{
    if (!m_task || m_sceneIndex < 0 || m_sceneIndex >= m_scenes.size())
        return;
    QPushButton *btn = m_task->catalogButton(m_scenes.at(m_sceneIndex).taskNumber);
    if (!btn)
        return;
    m_cursor->setPressed(true);
    btn->setDown(true);
    QTimer::singleShot(m_clickMs, this, [this, btn]() {
        if (btn)
            btn->setDown(false);
        if (m_cursor)
            m_cursor->setPressed(false);
        prepareScene(m_sceneIndex);
        QTimer::singleShot(m_hideCursorMs, this, [this]() {
            if (m_cursor)
                m_cursor->hide();
        });
        QTimer::singleShot(m_afterSelectMs, this, [this]() {
            if (m_typeTimer)
                m_typeTimer->start();
        });
    });
}

void DemoIdeWindow::startDrawing()
{
    if (!m_task || !m_task->canvas())
        return;
    if (m_sceneIndex < 0 || m_sceneIndex >= m_scenes.size())
        return;
    m_task->canvas()->resetStudent();
    m_task->canvas()->playPath(m_scenes.at(m_sceneIndex).path, m_drawDurationMs);
}

void DemoIdeWindow::onPlaybackFinished()
{
    if (m_task) {
        m_task->setInstruction(
            QString::fromUtf8("Верно: рисунок совпадает с образцом."),
            QStringLiteral("ok"));
    }
    QTimer::singleShot(m_afterOkMs, this, &DemoIdeWindow::advanceDemo);
}

void DemoIdeWindow::advanceDemo()
{
    if (m_sceneIndex + 1 >= m_scenes.size()) {
        const int hold = m_endHoldMs;
        QTimer::singleShot(hold, this, [this]() {
            if (m_recorder)
                m_recorder->stop();
        });
        return;
    }
    m_sceneIndex++;
    if (!m_task)
        return;
    QPushButton *btn = m_task->catalogButton(m_scenes.at(m_sceneIndex).taskNumber);
    moveCursorTo(btn, CursorTask);
}

QVector<QPointF> DemoIdeWindow::pathFromSpec(const QString &spec, const QPointF &origin) const
{
    QVector<QPointF> pts;
    QPointF p = origin;
    double heading = 0.0;
    pts.append(p);
    const QStringList toks = spec.split(QLatin1Char(' '), QString::SkipEmptyParts);
    for (const QString &tok : toks) {
        if (tok.size() < 2)
            continue;
        const QChar cmd = tok.at(0).toUpper();
        const double value = tok.mid(1).toDouble();
        if (cmd == QLatin1Char('F') || cmd == QLatin1Char('B')) {
            const double dist = (cmd == QLatin1Char('B')) ? -value : value;
            const double rad = heading * M_PI / 180.0;
            p += QPointF(dist * qCos(rad), dist * qSin(rad));
            pts.append(p);
        } else if (cmd == QLatin1Char('L')) {
            heading += value;
        } else if (cmd == QLatin1Char('R')) {
            heading -= value;
        }
    }
    return pts;
}

QVector<QPointF> DemoIdeWindow::housePath() const
{
    QVector<QPointF> pts;
    QPointF p(-50.0, -50.0);
    double heading = 0.0;
    pts.append(p);
    auto forward = [&](double dist) {
        const double rad = heading * M_PI / 180.0;
        p += QPointF(dist * qCos(rad), dist * qSin(rad));
        pts.append(p);
    };
    auto left = [&](double deg) { heading += deg; };
    auto right = [&](double deg) { heading -= deg; };

    for (int i = 0; i < 4; ++i) {
        forward(100);
        left(90);
    }
    left(90);
    forward(100);
    right(45);
    forward(71);
    right(90);
    forward(71);
    return pts;
}
