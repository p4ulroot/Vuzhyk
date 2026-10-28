#include "DemoHelpWindow.h"
#include "HelpWidget.h"
#include "DemoCursor.h"
#include "DemoIcons.h"
#include "DemoScale.h"
#include "DemoRecorder.h"

#include <QAbstractAnimation>
#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStatusBar>
#include <QStringList>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVariantAnimation>
#include <QEasingCurve>

DemoHelpWindow::DemoHelpWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setWindowTitle(QStringLiteral("Vuzhyk"));
    resize(1440, 900);
    buildChrome();
    prepareGrade6();

    m_cursor = new DemoCursor(this);
    m_cursorAnim = new QPropertyAnimation(m_cursor, "hotPos", this);
    m_cursorAnim->setDuration(520);
    m_cursorAnim->setEasingCurve(QEasingCurve::InOutCubic);
    connect(m_cursorAnim, &QPropertyAnimation::finished, this, &DemoHelpWindow::onCursorArrived);

    m_recorder = new DemoRecorder(this);
}

DemoHelpWindow::~DemoHelpWindow()
{
    if (m_recorder)
        m_recorder->stop();
}

void DemoHelpWindow::buildChrome()
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
    QToolButton *minBtn = makeWinBtn(QStringLiteral(":/icons/icons/window-controls/window-minimize.svg"));
    QToolButton *maxBtn = makeWinBtn(QStringLiteral(":/icons/icons/window-controls/window-maximize.svg"));
    QToolButton *closeBtn = makeWinBtn(QStringLiteral(":/icons/icons/window-controls/window-close.svg"));
    connect(minBtn, &QToolButton::clicked, this, &QWidget::showMinimized);
    connect(maxBtn, &QToolButton::clicked, this, [this, maxBtn]() {
        if (isMaximized()) {
            showNormal();
            maxBtn->setIcon(demoThemedIcon(
                QStringLiteral(":/icons/icons/window-controls/window-maximize.svg"),
                QStringLiteral("#333333"), dsSize(21, 21)));
        } else {
            showMaximized();
            maxBtn->setIcon(demoThemedIcon(
                QStringLiteral(":/icons/icons/window-controls/window-restore.svg"),
                QStringLiteral("#333333"), dsSize(21, 21)));
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
            demoThemedIcon(icons + file, QStringLiteral("#000000"), dsSize(16, 16)), tip);
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
    m_help = new HelpWidget(m_tabs);
    m_help->setTheme(QStringLiteral("light"));
    if (m_help->treeWidget()) {
        m_help->treeWidget()->setMinimumWidth(ds(200));
        m_help->treeWidget()->setMaximumWidth(ds(300));
    }
    m_tabs->addTab(m_help, QString::fromUtf8("Справка"));
    setCentralWidget(m_tabs);
    statusBar()->setSizeGripEnabled(false);
    statusBar()->setStyleSheet(QStringLiteral(
        "QStatusBar { background: #f0f0f0; color: #333333; border-top: 1px solid #d0d0d0; }"
        "QStatusBar::item { border: none; }"));
    statusBar()->showMessage(QString::fromUtf8("Готов"));
    statusBar()->show();

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
        "QTreeWidget { background: #ffffff; border: none; }"
        "QTextBrowser { background: #ffffff; border: none; }"
    ).arg(ds(12)).arg(ds(20)).arg(ds(6)).arg(ds(5)).arg(ds(15)).arg(ds(2)));
}

void DemoHelpWindow::prepareGrade6()
{
    m_keys = {
        QStringLiteral("grade6_import"),
        QStringLiteral("grade6_shape"),
        QStringLiteral("grade6_window"),
        QStringLiteral("grade6_move"),
        QStringLiteral("grade6_pen"),
        QStringLiteral("grade6_color"),
        QStringLiteral("grade6_state"),
        QStringLiteral("grade6_functions")
    };
    if (!m_help || !m_help->treeWidget())
        return;
    QTreeWidget *tree = m_help->treeWidget();
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = tree->topLevelItem(i);
        const QString key = item->data(0, Qt::UserRole).toString();
        item->setExpanded(key == QLatin1String("grade6_main"));
    }
    if (QTreeWidgetItem *grade6 = m_help->findItemByKey(QStringLiteral("grade6_main")))
        m_help->showItem(grade6);
}

void DemoHelpWindow::beginHelpDemo()
{
    prepareGrade6();
    if (m_recorder)
        m_recorder->start(this, QStringLiteral("DemoHelp"));
    QTimer::singleShot(800, this, [this]() {
        m_itemIndex = 0;
        goToCurrentItem();
    });
}

bool DemoHelpWindow::eventFilter(QObject *watched, QEvent *event)
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

void DemoHelpWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_cursor)
        m_cursor->raise();
}

QTreeWidgetItem *DemoHelpWindow::currentItem() const
{
    if (!m_help || m_itemIndex < 0 || m_itemIndex >= m_keys.size())
        return nullptr;
    return m_help->findItemByKey(m_keys.at(m_itemIndex));
}

QPoint DemoHelpWindow::itemHotPos(QTreeWidgetItem *item) const
{
    if (!m_help || !m_help->treeWidget() || !item)
        return QPoint(ds(80), height() / 2);
    QTreeWidget *tree = m_help->treeWidget();
    tree->scrollToItem(item, QAbstractItemView::EnsureVisible);
    const QRect r = tree->visualItemRect(item);
    return tree->viewport()->mapTo(const_cast<DemoHelpWindow *>(this), r.center());
}

QPoint DemoHelpWindow::browserHotPos() const
{
    if (!m_help || !m_help->contentBrowser())
        return QPoint(width() * 2 / 3, height() / 2);
    QTextBrowser *browser = m_help->contentBrowser();
    const QPoint local(browser->width() / 2, browser->height() / 3);
    return browser->mapTo(const_cast<DemoHelpWindow *>(this), local);
}

void DemoHelpWindow::moveCursorTo(const QPoint &end, AfterMove after)
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
    m_cursorAnim->setStartValue(start);
    m_cursorAnim->setEndValue(end);
    m_cursorAnim->start();
}

void DemoHelpWindow::goToCurrentItem()
{
    QTreeWidgetItem *item = currentItem();
    if (!item) {
        finishDemo();
        return;
    }
    moveCursorTo(itemHotPos(item), AfterClickItem);
}

void DemoHelpWindow::onCursorArrived()
{
    if (m_ignoreCursorFinish)
        return;
    const AfterMove after = m_afterMove;
    m_afterMove = AfterNone;
    if (after == AfterClickItem)
        clickCurrentItem();
    else if (after == AfterGotoBrowser)
        startZoom();
}

void DemoHelpWindow::clickCurrentItem()
{
    if (!m_cursor || !m_help)
        return;
    m_cursor->setPressed(true);
    QTimer::singleShot(110, this, [this]() {
        if (m_cursor)
            m_cursor->setPressed(false);
        if (QTreeWidgetItem *item = currentItem())
            m_help->showItem(item);
        if (m_itemIndex == 0) {
            QTimer::singleShot(380, this, [this]() {
                moveCursorTo(browserHotPos(), AfterGotoBrowser);
            });
        } else {
            QTimer::singleShot(280, this, &DemoHelpWindow::scrollThenNext);
        }
    });
}

void DemoHelpWindow::startZoom()
{
    QTextBrowser *browser = m_help ? m_help->contentBrowser() : nullptr;
    if (!browser) {
        scrollThenNext();
        return;
    }
    browser->setFocus();
    const qreal base = browserFontSize();
    auto *zoomIn = new QVariantAnimation(this);
    zoomIn->setDuration(850);
    zoomIn->setStartValue(base);
    zoomIn->setEndValue(base + 4.0);
    zoomIn->setEasingCurve(QEasingCurve::InOutCubic);
    connect(zoomIn, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        applyBrowserZoom(v.toReal());
    });
    connect(zoomIn, &QVariantAnimation::finished, this, [this, base, zoomIn]() {
        zoomIn->deleteLater();
        QTimer::singleShot(220, this, [this, base]() {
            auto *zoomOut = new QVariantAnimation(this);
            zoomOut->setDuration(520);
            zoomOut->setStartValue(base + 4.0);
            zoomOut->setEndValue(base + 2.0);
            zoomOut->setEasingCurve(QEasingCurve::InOutCubic);
            connect(zoomOut, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
                applyBrowserZoom(v.toReal());
            });
            connect(zoomOut, &QVariantAnimation::finished, this, [this, zoomOut]() {
                zoomOut->deleteLater();
                QTimer::singleShot(280, this, &DemoHelpWindow::scrollThenNext);
            });
            zoomOut->start();
        });
    });
    zoomIn->start();
}

qreal DemoHelpWindow::browserFontSize() const
{
    QTextBrowser *browser = m_help ? m_help->contentBrowser() : nullptr;
    if (!browser)
        return 10.0;
    const QFont f = browser->font();
    if (f.pointSizeF() > 0)
        return f.pointSizeF();
    if (f.pointSize() > 0)
        return f.pointSize();
    return 10.0;
}

void DemoHelpWindow::applyBrowserZoom(qreal size)
{
    QTextBrowser *browser = m_help ? m_help->contentBrowser() : nullptr;
    if (!browser)
        return;
    QFont f = browser->font();
    f.setPointSizeF(qMax(6.0, size));
    browser->setFont(f);
}

void DemoHelpWindow::scrollThenNext()
{
    QTextBrowser *browser = m_help ? m_help->contentBrowser() : nullptr;
    if (browser) {
        QScrollBar *bar = browser->verticalScrollBar();
        if (bar && bar->maximum() > 0) {
            auto *anim = new QPropertyAnimation(bar, "value", this);
            anim->setDuration(420);
            anim->setStartValue(bar->value());
            anim->setEndValue(qMin(bar->maximum(), bar->value() + ds(110)));
            anim->setEasingCurve(QEasingCurve::InOutQuad);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }
    QTimer::singleShot(620, this, &DemoHelpWindow::advanceItem);
}

void DemoHelpWindow::advanceItem()
{
    if (m_itemIndex + 1 >= m_keys.size()) {
        finishDemo();
        return;
    }
    ++m_itemIndex;
    goToCurrentItem();
}

void DemoHelpWindow::finishDemo()
{
    QTimer::singleShot(900, this, [this]() {
        if (m_cursor)
            m_cursor->hide();
        if (m_recorder)
            m_recorder->stop();
    });
}
