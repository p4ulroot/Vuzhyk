#include "DemoRobotIdeWindow.h"
#include "DemoRobotTask.h"
#include "DemoRobotCanvas.h"
#include "DemoCursor.h"
#include "DemoIcons.h"
#include "DemoScale.h"
#include "DemoRecorder.h"

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPalette>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStatusBar>
#include <QStringList>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QEasingCurve>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

DemoRobotIdeWindow::DemoRobotIdeWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setWindowTitle(QStringLiteral("Vuzhyk"));
    resize(1440, 900);
    buildChrome();
    buildPatternCells();

    m_cursor = new DemoCursor(this);
    m_cursorAnim = new QPropertyAnimation(m_cursor, "hotPos", this);
    m_cursorAnim->setDuration(700);
    m_cursorAnim->setEasingCurve(QEasingCurve::InOutCubic);
    connect(m_cursorAnim, &QPropertyAnimation::finished, this, &DemoRobotIdeWindow::onCursorArrived);

    m_recorder = new DemoRecorder(this);
}

DemoRobotIdeWindow::~DemoRobotIdeWindow()
{
    if (m_recorder)
        m_recorder->stop();
}

void DemoRobotIdeWindow::buildChrome()
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

    m_titleBar = new QWidget(this);
    m_titleBar->setObjectName(QStringLiteral("CustomTitleBar"));
    m_titleBar->setFixedHeight(ds(26));
    m_titleBar->installEventFilter(this);
    m_titleBar->setStyleSheet(QStringLiteral("QWidget#CustomTitleBar { background: #353535; }"));
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
        "QMenuBar{background:transparent;color:#ffffff;border:none;padding:0;margin:0;min-height:%1px;}"
        "QMenuBar::item{color:#ffffff;padding:0 %2px;margin:0;border:0;min-height:%1px;}"
        "QMenuBar::item:selected{background:#555555;}")
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
        QString::fromUtf8("Выбрать Python..."), QString::fromUtf8("Переключить тему"),
        QString::fromUtf8("Редактор задач pyrob") });
    addDummyMenu(QString::fromUtf8("Задачи"), { QString::fromUtf8("Задачник черепаха") });
    addDummyMenu(QString::fromUtf8("Помощь"), { QString::fromUtf8("Справка") });
    m_menuBar->addAction(QString::fromUtf8("Агент"));
    titleLayout->addWidget(m_menuBar, 1);

    auto makeWinBtn = [this, titleLayout](const QString &res) {
        auto *btn = new QToolButton(m_titleBar);
        btn->setIcon(demoThemedIcon(res, QStringLiteral("#e0e0e0"), dsSize(21, 21)));
        btn->setIconSize(dsSize(21, 21));
        btn->setFixedSize(dsSize(36, 26));
        btn->setAutoRaise(true);
        btn->setStyleSheet(QStringLiteral(
            "QToolButton{background:transparent;border:none;padding:0;}"
            "QToolButton:hover{background:rgba(255,255,255,0.10);}"));
        titleLayout->addWidget(btn, 0);
        return btn;
    };
    QToolButton *minBtn = makeWinBtn(QStringLiteral(":/icons/icons/window-controls/window-minimize.svg"));
    QToolButton *maxBtn = makeWinBtn(QStringLiteral(":/icons/icons/window-controls/window-maximize.svg"));
    QToolButton *closeBtn = makeWinBtn(QStringLiteral(":/icons/icons/window-controls/window-close.svg"));
    connect(minBtn, &QToolButton::clicked, this, &QWidget::showMinimized);
    connect(maxBtn, &QToolButton::clicked, this, [this, maxBtn]() {
        if (isMaximized()) {
            showNormal();
            maxBtn->setIcon(demoThemedIcon(
                QStringLiteral(":/icons/icons/window-controls/window-maximize.svg"),
                QStringLiteral("#e0e0e0"), dsSize(21, 21)));
        } else {
            showMaximized();
            maxBtn->setIcon(demoThemedIcon(
                QStringLiteral(":/icons/icons/window-controls/window-restore.svg"),
                QStringLiteral("#e0e0e0"), dsSize(21, 21)));
        }
    });
    connect(closeBtn, &QToolButton::clicked, this, &QWidget::close);
    setMenuWidget(m_titleBar);

    QToolBar *bar = addToolBar(QString::fromUtf8("Основное"));
    bar->setMovable(false);
    bar->setIconSize(dsSize(16, 16));
    const QString icons = QStringLiteral(":/icons/icons/light-theme/");
    auto addTb = [&](const QString &file, const QString &tip) {
        QAction *a = bar->addAction(
            demoThemedIcon(icons + file, QStringLiteral("#e0e0e0"), dsSize(16, 16)), tip);
        a->setEnabled(true);
        return a;
    };
    addTb(QStringLiteral("file-new.svg"), QString::fromUtf8("Новый"));
    addTb(QStringLiteral("file-open.svg"), QString::fromUtf8("Открыть"));
    addTb(QStringLiteral("file-save.svg"), QString::fromUtf8("Сохранить"));
    addTb(QStringLiteral("file-save-all.svg"), QString::fromUtf8("Сохранить все"));
    bar->addSeparator();
    addTb(QStringLiteral("cut.svg"), QString::fromUtf8("Вырезать"));
    addTb(QStringLiteral("copy.svg"), QString::fromUtf8("Копировать"));
    addTb(QStringLiteral("paste-clipboard.svg"), QString::fromUtf8("Вставить"));
    bar->addSeparator();
    addTb(QStringLiteral("undo.svg"), QString::fromUtf8("Отменить"));
    addTb(QStringLiteral("redo.svg"), QString::fromUtf8("Восстановить"));
    bar->addSeparator();
    addTb(QStringLiteral("play.svg"), QString::fromUtf8("Запустить"));
    addTb(QStringLiteral("stop.svg"), QString::fromUtf8("Завершить"));
    addTb(QStringLiteral("bug.svg"), QString::fromUtf8("Отладка"));
    addTb(QStringLiteral("next-bug.svg"), QString::fromUtf8("Шаг отладки"));
    addTb(QStringLiteral("run-in-terminal.svg"), QString::fromUtf8("В браузере"));

    m_tabs = new QTabWidget(this);
    m_tabs->setTabsClosable(false);
    m_tabs->setMovable(false);
    m_task = new DemoRobotTask(m_tabs);
    m_tabs->addTab(m_task, QString::fromUtf8("Редактор задач pyrob"));
    setCentralWidget(m_tabs);
    statusBar()->setSizeGripEnabled(false);
    statusBar()->setStyleSheet(QStringLiteral(
        "QStatusBar { background: #2a2a2a; color: #ffffff; border-top: 1px solid #555555; }"
        "QStatusBar::item { border: none; }"));
    statusBar()->showMessage(QString::fromUtf8("Готов"));
    statusBar()->show();

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
}

bool DemoRobotIdeWindow::eventFilter(QObject *watched, QEvent *event)
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

void DemoRobotIdeWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_cursor)
        m_cursor->raise();
}

void DemoRobotIdeWindow::buildPatternCells()
{
    m_circle.clear();
    m_square.clear();
    m_walls.clear();
    const int cx = 7;
    const int cy = 7;
    const double R = 6.0;
    QVector<QPoint> ring;
    for (int i = 0; i < 15; ++i) {
        for (int j = 0; j < 15; ++j) {
            const double d = std::hypot(double(i - cx), double(j - cy));
            if (qRound(d) == qRound(R))
                ring.append(QPoint(i, j));
        }
    }
    std::sort(ring.begin(), ring.end(), [cx, cy](const QPoint &a, const QPoint &b) {
        auto key = [cx, cy](const QPoint &p) {
            double ang = std::atan2(double(p.x() - cx), double(p.y() - cy));
            ang += M_PI / 2.0;
            if (ang < 0)
                ang += 2.0 * M_PI;
            return ang;
        };
        return key(a) < key(b);
    });
    m_circle = ring;

    for (int i = 5; i <= 9; ++i) {
        for (int j = 5; j <= 9; ++j)
            m_square.append(QPoint(i, j));
    }
    for (int i = 5; i <= 9; ++i)
        m_walls.append({ i, 5, int(DemoRobotCanvas::Left) });
    for (int i = 5; i <= 9; ++i)
        m_walls.append({ i, 9, int(DemoRobotCanvas::Right) });
}

void DemoRobotIdeWindow::beginPatternDemo()
{
    repaint();
    if (m_recorder)
        m_recorder->start(this, QStringLiteral("DemoRobotTask"));
    QTimer::singleShot(900, this, [this]() {
        moveCursorTo(toolHotPos(DemoRobotTask::PaintToBeFilled), AfterSelectTeal);
    });
}

QPoint DemoRobotIdeWindow::cellHotPos(int i, int j) const
{
    if (!m_task || !m_task->canvas())
        return QPoint(width() / 2, height() / 2);
    const QPoint local = m_task->canvas()->cellCenterLocal(i, j);
    return m_task->canvas()->mapTo(const_cast<DemoRobotIdeWindow *>(this), local);
}

QPoint DemoRobotIdeWindow::wallHotPos(int i, int j, int side) const
{
    if (!m_task || !m_task->canvas())
        return QPoint(width() / 2, height() / 2);
    const QPoint local = m_task->canvas()->wallClickLocal(
        i, j, static_cast<DemoRobotCanvas::WallSide>(side));
    return m_task->canvas()->mapTo(const_cast<DemoRobotIdeWindow *>(this), local);
}

QPoint DemoRobotIdeWindow::toolHotPos(int toolId) const
{
    QToolButton *btn = m_task ? m_task->toolButton(toolId) : nullptr;
    if (!btn)
        return QPoint(ds(40), height() / 2);
    return btn->mapTo(const_cast<DemoRobotIdeWindow *>(this), btn->rect().center());
}

void DemoRobotIdeWindow::moveCursorTo(const QPoint &end, AfterMove after)
{
    if (!m_cursor || !m_cursorAnim)
        return;
    QPoint start = m_cursor->hotPos();
    if (!m_cursor->isVisible() || start.isNull())
        start = QPoint(width() / 3, height() / 3);
    m_afterMove = after;
    m_cursor->setPressed(false);
    m_cursor->setHotPos(start);
    m_cursor->show();
    m_cursor->raise();
    m_ignoreCursorFinish = true;
    m_cursorAnim->stop();
    m_ignoreCursorFinish = false;
    m_cursorAnim->setDuration(after == AfterBeginCircle || after == AfterBeginSquare
        || after == AfterBeginWalls ? 550 : 700);
    m_cursorAnim->setStartValue(start);
    m_cursorAnim->setEndValue(end);
    m_cursorAnim->start();
}

void DemoRobotIdeWindow::clickTool(int id)
{
    if (!m_cursor || !m_task)
        return;
    QToolButton *btn = m_task->toolButton(id);
    m_cursor->setPressed(true);
    if (btn)
        btn->setDown(true);
    QTimer::singleShot(120, this, [this, btn, id]() {
        if (btn)
            btn->setDown(false);
        if (m_task)
            m_task->setToolChecked(id);
        if (m_cursor)
            m_cursor->setPressed(false);
    });
}

void DemoRobotIdeWindow::onCursorArrived()
{
    if (m_ignoreCursorFinish)
        return;
    const AfterMove after = m_afterMove;
    m_afterMove = AfterNone;
    switch (after) {
    case AfterSelectTeal:
        clickTool(DemoRobotTask::PaintToBeFilled);
        QTimer::singleShot(280, this, [this]() {
            if (m_circle.isEmpty())
                return;
            moveCursorTo(cellHotPos(m_circle.first().x(), m_circle.first().y()), AfterBeginCircle);
        });
        break;
    case AfterBeginCircle:
        m_paintList = &m_circle;
        m_paintIndex = 0;
        if (m_cursor)
            m_cursor->setPressed(true);
        paintNext();
        break;
    case AfterBeginSquare:
        m_paintList = &m_square;
        m_paintIndex = 0;
        if (m_cursor)
            m_cursor->setPressed(true);
        paintNext();
        break;
    case AfterSelectWall:
        clickTool(DemoRobotTask::ToggleWall);
        QTimer::singleShot(280, this, [this]() {
            if (m_walls.isEmpty())
                return;
            const WallMark &w = m_walls.first();
            moveCursorTo(wallHotPos(w.i, w.j, w.side), AfterBeginWalls);
        });
        break;
    case AfterBeginWalls:
        m_wallIndex = 0;
        if (m_cursor)
            m_cursor->setPressed(true);
        wallNext();
        break;
    case AfterSelectStart:
        clickTool(DemoRobotTask::SetStart);
        QTimer::singleShot(280, this, [this]() {
            moveCursorTo(cellHotPos(0, 7), AfterPlaceStart);
        });
        break;
    case AfterPlaceStart:
        if (m_cursor)
            m_cursor->setPressed(true);
        if (m_task && m_task->canvas())
            m_task->canvas()->setStart(0, 7);
        QTimer::singleShot(140, this, [this]() {
            if (m_cursor)
                m_cursor->setPressed(false);
            QTimer::singleShot(250, this, [this]() {
                moveCursorTo(toolHotPos(DemoRobotTask::SetParking), AfterSelectPark);
            });
        });
        break;
    case AfterSelectPark:
        clickTool(DemoRobotTask::SetParking);
        QTimer::singleShot(280, this, [this]() {
            moveCursorTo(cellHotPos(14, 7), AfterPlacePark);
        });
        break;
    case AfterPlacePark:
        if (m_cursor)
            m_cursor->setPressed(true);
        if (m_task && m_task->canvas())
            m_task->canvas()->setParking(14, 7);
        QTimer::singleShot(140, this, [this]() {
            if (m_cursor)
                m_cursor->setPressed(false);
            QTimer::singleShot(400, this, [this]() {
                if (m_cursor)
                    m_cursor->hide();
                finishDemo();
            });
        });
        break;
    default:
        break;
    }
}

void DemoRobotIdeWindow::paintNext()
{
    if (!m_paintList || !m_task || !m_task->canvas())
        return;
    if (m_paintIndex >= m_paintList->size()) {
        if (m_cursor)
            m_cursor->setPressed(false);
        const bool wasCircle = (m_paintList == &m_circle);
        m_paintList = nullptr;
        QTimer::singleShot(220, this, [this, wasCircle]() {
            if (wasCircle) {
                if (m_square.isEmpty())
                    return;
                moveCursorTo(cellHotPos(m_square.first().x(), m_square.first().y()), AfterBeginSquare);
            } else {
                moveCursorTo(toolHotPos(DemoRobotTask::ToggleWall), AfterSelectWall);
            }
        });
        return;
    }
    const QPoint cell = m_paintList->at(m_paintIndex++);
    const auto type = DemoRobotCanvas::ToBeFilled;
    m_task->canvas()->setCell(cell.x(), cell.y(), type);
    if (m_cursor)
        m_cursor->setHotPos(cellHotPos(cell.x(), cell.y()));
    QTimer::singleShot(38, this, [this]() { paintNext(); });
}

void DemoRobotIdeWindow::wallNext()
{
    if (!m_task || !m_task->canvas())
        return;
    if (m_wallIndex >= m_walls.size()) {
        if (m_cursor)
            m_cursor->setPressed(false);
        QTimer::singleShot(220, this, [this]() {
            moveCursorTo(toolHotPos(DemoRobotTask::SetStart), AfterSelectStart);
        });
        return;
    }
    const WallMark w = m_walls.at(m_wallIndex++);
    m_task->canvas()->setWall(w.i, w.j, static_cast<DemoRobotCanvas::WallSide>(w.side), true);
    if (m_cursor)
        m_cursor->setHotPos(wallHotPos(w.i, w.j, w.side));
    QTimer::singleShot(70, this, [this]() { wallNext(); });
}

void DemoRobotIdeWindow::finishDemo()
{
    QTimer::singleShot(1800, this, [this]() {
        if (m_recorder)
            m_recorder->stop();
    });
}
