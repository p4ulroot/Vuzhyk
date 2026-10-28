#include "DemoThemeWindow.h"
#include "DemoCursor.h"
#include "DemoIcons.h"
#include "DemoScale.h"
#include "DemoRecorder.h"

#include <QAbstractAnimation>
#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRegion>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStatusBar>
#include <QSyntaxHighlighter>
#include <QTabBar>
#include <QTabWidget>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QElapsedTimer>
#include <QEasingCurve>

namespace {
constexpr int kThemeSwitchMs = 2500;
constexpr int kClickMs = 80;
constexpr int kMenuAnimMs = 160;
constexpr int kCursorMs = 280;
}

class DemoPythonHighlighter : public QSyntaxHighlighter {
public:
    explicit DemoPythonHighlighter(QTextDocument *parent)
        : QSyntaxHighlighter(parent)
    {
        setDark(false);
    }

    void setDark(bool dark)
    {
        m_keywords.setFontWeight(QFont::Bold);
        if (dark) {
            m_keywords.setForeground(QColor("#6cb6ff"));
            m_string.setForeground(QColor("#ce9178"));
            m_comment.setForeground(QColor("#6a9955"));
            m_number.setForeground(QColor("#b5cea8"));
        } else {
            m_keywords.setForeground(QColor("#0000aa"));
            m_string.setForeground(QColor("#008000"));
            m_comment.setForeground(QColor("#808080"));
            m_number.setForeground(QColor("#ae00ae"));
        }
        rehighlight();
    }

protected:
    void highlightBlock(const QString &text) override
    {
        static const QStringList words = {
            QStringLiteral("from"), QStringLiteral("import"), QStringLiteral("for"),
            QStringLiteral("in"), QStringLiteral("range"), QStringLiteral("def"),
            QStringLiteral("if"), QStringLiteral("else"), QStringLiteral("elif"),
            QStringLiteral("while"), QStringLiteral("return"), QStringLiteral("class"),
            QStringLiteral("print"), QStringLiteral("True"), QStringLiteral("False"),
            QStringLiteral("None"), QStringLiteral("and"), QStringLiteral("or"),
            QStringLiteral("not"), QStringLiteral("pass"), QStringLiteral("break"),
            QStringLiteral("continue")
        };
        for (const QString &w : words) {
            int i = 0;
            while ((i = text.indexOf(w, i)) >= 0) {
                const bool leftOk = (i == 0) || !text.at(i - 1).isLetterOrNumber();
                const int end = i + w.size();
                const bool rightOk = (end >= text.size()) || !text.at(end).isLetterOrNumber();
                if (leftOk && rightOk)
                    setFormat(i, w.size(), m_keywords);
                i = end;
            }
        }
        for (int i = 0; i < text.size(); ++i) {
            if (text.at(i).isDigit())
                setFormat(i, 1, m_number);
        }
        const int hash = text.indexOf(QLatin1Char('#'));
        if (hash >= 0)
            setFormat(hash, text.size() - hash, m_comment);
        int i = 0;
        while (i < text.size()) {
            const QChar q = text.at(i);
            if (q == QLatin1Char('"') || q == QLatin1Char('\'')) {
                const int start = i++;
                while (i < text.size() && text.at(i) != q)
                    ++i;
                if (i < text.size())
                    ++i;
                setFormat(start, i - start, m_string);
            } else {
                ++i;
            }
        }
    }

private:
    QTextCharFormat m_keywords;
    QTextCharFormat m_string;
    QTextCharFormat m_comment;
    QTextCharFormat m_number;
};

DemoThemeWindow::DemoThemeWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setWindowTitle(QStringLiteral("Vuzhyk"));
    resize(1440, 900);
    buildChrome();
    buildEditor();
    buildFakeMenu();
    fillSampleCode();

    m_cursor = new DemoCursor(this);
    m_cursorAnim = new QPropertyAnimation(m_cursor, "hotPos", this);
    m_cursorAnim->setDuration(kCursorMs);
    m_cursorAnim->setEasingCurve(QEasingCurve::InOutCubic);
    connect(m_cursorAnim, &QPropertyAnimation::finished, this, &DemoThemeWindow::onCursorArrived);

    m_recorder = new DemoRecorder(this);
}

DemoThemeWindow::~DemoThemeWindow()
{
    if (m_recorder)
        m_recorder->stop();
}

void DemoThemeWindow::buildChrome()
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
        return menu;
    };
    addDummyMenu(QString::fromUtf8("Файл"), {
        QString::fromUtf8("Новый"), QString::fromUtf8("Открыть..."),
        QString::fromUtf8("Сохранить"), QString::fromUtf8("Выход") });
    addDummyMenu(QString::fromUtf8("Запуск"), { QString::fromUtf8("Запустить скрипт") });
    m_toolsAction = m_menuBar->addAction(QString::fromUtf8("Инструменты"));
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
        "QScrollBar:horizontal { background-color: #f0f0f0; height: %1px; border: none; }"
        "QScrollBar::handle:horizontal { background-color: #c0c0c0; min-width: %2px;"
        " border-radius: %3px; margin: 2px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }"
        "QMainWindow { background: #f3f3f3; }"
        "QTabWidget::pane { border: 1px solid #d0d0d0; background: #ffffff; }"
        "QTabBar::tab { background: #e8e8e8; padding: %4px %5px; margin-right: 2px;"
        " border: 1px solid #d0d0d0; }"
        "QTabBar::tab:selected { background: #ffffff; border-bottom-color: #ffffff; }"
        "QStatusBar { background: #f0f0f0; }"
        "QToolBar { background: #f3f3f3; border: none; spacing: %6px; padding: %6px; }"
        "QMenu { background: #ffffff; color: #222222; border: 1px solid #c0c0c0; }"
        "QMenu::item { padding: %4px %5px; }"
        "QMenu::item:selected { background: #cce4f7; }"
    ).arg(ds(12)).arg(ds(20)).arg(ds(6)).arg(ds(5)).arg(ds(15)).arg(ds(2)));
}

void DemoThemeWindow::buildEditor()
{
    m_tabs = new QTabWidget(this);
    m_tabs->setTabsClosable(false);
    m_tabs->setMovable(false);

    m_editor = makeEditor();
    m_highlighter = new DemoPythonHighlighter(m_editor->document());
    m_editor2 = makeEditor();
    m_highlighter2 = new DemoPythonHighlighter(m_editor2->document());
    applyEditorTheme(false);

    m_tabs->addTab(m_editor, QString::fromUtf8("Новый файл"));
    m_tabs->addTab(m_editor2, QString::fromUtf8("star.py"));
    m_tabs->setCurrentIndex(0);
    setCentralWidget(m_tabs);
}

QPlainTextEdit *DemoThemeWindow::makeEditor()
{
    auto *editor = new QPlainTextEdit(m_tabs);
    editor->setFont(QFont(QStringLiteral("Consolas"), ds(11)));
    editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    editor->setPlaceholderText(QString());
    editor->setTabStopDistance(4 * editor->fontMetrics().horizontalAdvance(QLatin1Char(' ')));
    return editor;
}

void DemoThemeWindow::buildFakeMenu()
{
    m_fakeMenu = new QFrame(this);
    m_fakeMenu->setObjectName(QStringLiteral("DemoPopupMenu"));
    m_fakeMenu->setStyleSheet(QStringLiteral(
        "QFrame#DemoPopupMenu {"
        " background: #ffffff;"
        " border: 1px solid #c0c0c0;"
        "}"));
    auto *lay = new QVBoxLayout(m_fakeMenu);
    lay->setContentsMargins(ds(1), ds(3), ds(1), ds(3));
    lay->setSpacing(0);

    auto makeItem = [this, lay](const QString &text) {
        auto *item = new QLabel(text, m_fakeMenu);
        item->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        item->setMinimumWidth(ds(230));
        item->setFixedHeight(ds(28));
        QFont itemFont = m_menuBar ? m_menuBar->font() : font();
        item->setFont(itemFont);
        styleMenuItem(item, false);
        lay->addWidget(item);
        return item;
    };
    auto makeSep = [this, lay]() {
        auto *wrap = new QWidget(m_fakeMenu);
        wrap->setFixedHeight(ds(9));
        auto *sepLay = new QVBoxLayout(wrap);
        sepLay->setContentsMargins(ds(8), ds(4), ds(8), ds(4));
        sepLay->setSpacing(0);
        auto *line = new QFrame(wrap);
        line->setFrameShape(QFrame::HLine);
        line->setFixedHeight(1);
        line->setStyleSheet(QStringLiteral("QFrame { color: #d0d0d0; background: #d0d0d0; border: none; }"));
        sepLay->addWidget(line);
        lay->addWidget(wrap);
    };

    makeItem(QString::fromUtf8("Выбрать Python..."));
    makeSep();
    makeItem(QString::fromUtf8("REPL"));
    makeSep();
    makeItem(QString::fromUtf8("Редактор задач pyrob"));
    makeSep();
    m_themeItem = makeItem(QString::fromUtf8("Переключить тему"));
    m_fakeMenu->adjustSize();
    m_fakeMenu->hide();
}

void DemoThemeWindow::styleMenuItem(QLabel *item, bool selected)
{
    if (!item)
        return;
    item->setStyleSheet(selected
        ? QStringLiteral(
            "QLabel { background: #cce4f7; color: #222222; padding: 0 %1px 0 %2px; }")
              .arg(ds(20)).arg(ds(28))
        : QStringLiteral(
            "QLabel { background: transparent; color: #222222; padding: 0 %1px 0 %2px; }")
              .arg(ds(20)).arg(ds(28)));
}

void DemoThemeWindow::fillSampleCode()
{
    if (!m_editor)
        return;
    m_editor->setPlainText(QString::fromUtf8(
        "from turtle import *\n"
        "\n"
        "# Домик с крышей, окном и дорожкой\n"
        "speed(0)\n"
        "pensize(2)\n"
        "bgcolor(\"aliceblue\")\n"
        "\n"
        "def square(size):\n"
        "    for i in range(4):\n"
        "        forward(size)\n"
        "        left(90)\n"
        "\n"
        "def rectangle(width, height):\n"
        "    for i in range(2):\n"
        "        forward(width)\n"
        "        left(90)\n"
        "        forward(height)\n"
        "        left(90)\n"
        "\n"
        "def triangle(size):\n"
        "    for i in range(3):\n"
        "        forward(size)\n"
        "        left(120)\n"
        "\n"
        "def filled_square(size, fill):\n"
        "    color(fill)\n"
        "    begin_fill()\n"
        "    square(size)\n"
        "    end_fill()\n"
        "\n"
        "# Стены дома\n"
        "penup()\n"
        "goto(-90, -80)\n"
        "pendown()\n"
        "filled_square(180, \"wheat\")\n"
        "\n"
        "# Крыша\n"
        "penup()\n"
        "goto(-90, 100)\n"
        "pendown()\n"
        "color(\"firebrick\")\n"
        "begin_fill()\n"
        "goto(0, 180)\n"
        "goto(90, 100)\n"
        "goto(-90, 100)\n"
        "end_fill()\n"
        "\n"
        "# Дверь\n"
        "penup()\n"
        "goto(-20, -80)\n"
        "pendown()\n"
        "color(\"sienna\")\n"
        "begin_fill()\n"
        "rectangle(40, 70)\n"
        "end_fill()\n"
        "\n"
        "# Окно слева\n"
        "penup()\n"
        "goto(-70, 10)\n"
        "pendown()\n"
        "color(\"skyblue\")\n"
        "begin_fill()\n"
        "square(40)\n"
        "end_fill()\n"
        "color(\"sienna\")\n"
        "goto(-50, 10)\n"
        "goto(-50, 50)\n"
        "penup()\n"
        "goto(-70, 30)\n"
        "pendown()\n"
        "goto(-30, 30)\n"
        "\n"
        "# Окно справа\n"
        "penup()\n"
        "goto(30, 10)\n"
        "pendown()\n"
        "color(\"skyblue\")\n"
        "begin_fill()\n"
        "square(40)\n"
        "end_fill()\n"
        "\n"
        "# Труба\n"
        "penup()\n"
        "goto(40, 130)\n"
        "pendown()\n"
        "color(\"dimgray\")\n"
        "begin_fill()\n"
        "rectangle(18, 40)\n"
        "end_fill()\n"
        "\n"
        "hideturtle()\n"
        "done()\n"));
    QTextCursor cur = m_editor->textCursor();
    cur.movePosition(QTextCursor::Start);
    m_editor->setTextCursor(cur);

    if (!m_editor2)
        return;
    m_editor2->setPlainText(QString::fromUtf8(
        "from turtle import *\n"
        "\n"
        "# Звезда и снежинка\n"
        "speed(0)\n"
        "pensize(3)\n"
        "bgcolor(\"navy\")\n"
        "\n"
        "def star(size):\n"
        "    for i in range(5):\n"
        "        forward(size)\n"
        "        right(144)\n"
        "\n"
        "def snowflake(size):\n"
        "    for i in range(6):\n"
        "        star(size)\n"
        "        right(60)\n"
        "\n"
        "penup()\n"
        "goto(-40, 60)\n"
        "pendown()\n"
        "color(\"gold\")\n"
        "begin_fill()\n"
        "star(160)\n"
        "end_fill()\n"
        "\n"
        "penup()\n"
        "goto(0, -20)\n"
        "pendown()\n"
        "color(\"white\")\n"
        "snowflake(50)\n"
        "\n"
        "hideturtle()\n"
        "done()\n"));
    QTextCursor cur2 = m_editor2->textCursor();
    cur2.movePosition(QTextCursor::Start);
    m_editor2->setTextCursor(cur2);
}

void DemoThemeWindow::applyEditorTheme(bool dark)
{
    styleOneEditor(m_editor, m_highlighter, dark);
    styleOneEditor(m_editor2, m_highlighter2, dark);
}

void DemoThemeWindow::styleOneEditor(QPlainTextEdit *editor, DemoPythonHighlighter *highlighter, bool dark)
{
    if (!editor)
        return;
    if (highlighter)
        highlighter->setDark(dark);
    if (dark) {
        editor->setStyleSheet(QStringLiteral(
            "QPlainTextEdit {"
            " background: #232323;"
            " color: #ffffff;"
            " border: none;"
            " padding: %1px;"
            " selection-background-color: #2a82da;"
            "}"
        ).arg(ds(8)));
    } else {
        editor->setStyleSheet(QStringLiteral(
            "QPlainTextEdit {"
            " background: #ffffff;"
            " color: #222222;"
            " border: none;"
            " padding: %1px;"
            "}"
        ).arg(ds(8)));
    }
}

void DemoThemeWindow::beginThemeDemo()
{
    m_demoClock.start();
    if (m_recorder)
        m_recorder->start(this, QStringLiteral("DemoTheme"));
    QTimer::singleShot(200, this, [this]() {
        moveCursorTo(editorHotPos(), AfterZoom, kCursorMs);
    });
}

bool DemoThemeWindow::eventFilter(QObject *watched, QEvent *event)
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

void DemoThemeWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_fakeMenu && m_fakeMenu->isVisible())
        showFakeMenu();
    if (m_cursor)
        m_cursor->raise();
}

QPoint DemoThemeWindow::editorHotPos() const
{
    if (!m_editor)
        return QPoint(width() * 2 / 3, height() / 2);
    const QPoint local(m_editor->width() / 2, m_editor->height() / 3);
    return m_editor->mapTo(const_cast<DemoThemeWindow *>(this), local);
}

QPoint DemoThemeWindow::toolsHotPos() const
{
    if (!m_menuBar || !m_toolsAction)
        return QPoint(ds(220), ds(14));
    const QRect r = m_menuBar->actionGeometry(m_toolsAction);
    return m_menuBar->mapTo(const_cast<DemoThemeWindow *>(this), r.center());
}

QPoint DemoThemeWindow::themeItemHotPos() const
{
    if (!m_themeItem)
        return toolsHotPos() + QPoint(ds(80), ds(50));
    return m_themeItem->mapTo(const_cast<DemoThemeWindow *>(this), m_themeItem->rect().center());
}

QPoint DemoThemeWindow::secondTabHotPos() const
{
    if (!m_tabs || !m_tabs->tabBar())
        return QPoint(ds(180), ds(70));
    QTabBar *bar = m_tabs->tabBar();
    const QRect r = bar->tabRect(1);
    return bar->mapTo(const_cast<DemoThemeWindow *>(this), r.center());
}

void DemoThemeWindow::moveCursorTo(const QPoint &end, AfterMove after, int durationMs)
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
    m_cursorAnim->setDuration(durationMs);
    m_cursorAnim->setStartValue(start);
    m_cursorAnim->setEndValue(end);
    m_cursorAnim->start();
}

void DemoThemeWindow::onCursorArrived()
{
    if (m_ignoreCursorFinish)
        return;
    const AfterMove after = m_afterMove;
    m_afterMove = AfterNone;
    if (after == AfterZoom)
        startZoom();
    else if (after == AfterOpenTools)
        clickTools();
    else if (after == AfterClickTheme) {
        styleMenuItem(m_themeItem, true);
        const int clickAt = kThemeSwitchMs - kClickMs - kMenuAnimMs;
        QTimer::singleShot(msUntil(clickAt), this, &DemoThemeWindow::clickTheme);
    } else if (after == AfterClickTab)
        clickSecondTab();
}

void DemoThemeWindow::startZoom()
{
    if (!m_editor) {
        moveCursorTo(toolsHotPos(), AfterOpenTools);
        return;
    }
    m_editor->setFocus();
    const qreal base = editorFontSize();
    auto *zoomIn = new QVariantAnimation(this);
    zoomIn->setDuration(450);
    zoomIn->setStartValue(base);
    zoomIn->setEndValue(base + 12.0);
    zoomIn->setEasingCurve(QEasingCurve::InOutCubic);
    connect(zoomIn, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        applyEditorZoom(v.toReal());
    });
    connect(zoomIn, &QVariantAnimation::finished, this, [this, base, zoomIn]() {
        zoomIn->deleteLater();
        QTimer::singleShot(80, this, [this, base]() {
            auto *zoomOut = new QVariantAnimation(this);
            zoomOut->setDuration(280);
            zoomOut->setStartValue(base + 12.0);
            zoomOut->setEndValue(base + 7.0);
            zoomOut->setEasingCurve(QEasingCurve::InOutCubic);
            connect(zoomOut, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
                applyEditorZoom(v.toReal());
            });
            connect(zoomOut, &QVariantAnimation::finished, this, [this, zoomOut]() {
                zoomOut->deleteLater();
                QTimer::singleShot(80, this, [this]() {
                    moveCursorTo(toolsHotPos(), AfterOpenTools, kCursorMs);
                });
            });
            zoomOut->start();
        });
    });
    zoomIn->start();
}

qreal DemoThemeWindow::editorFontSize() const
{
    if (!m_editor)
        return 11.0;
    const QFont f = m_editor->font();
    if (f.pointSizeF() > 0)
        return f.pointSizeF();
    if (f.pointSize() > 0)
        return f.pointSize();
    return 11.0;
}

void DemoThemeWindow::applyEditorZoom(qreal size)
{
    auto zoomOne = [size](QPlainTextEdit *editor) {
        if (!editor)
            return;
        QFont f = editor->font();
        f.setPointSizeF(qMax(6.0, size));
        editor->setFont(f);
        editor->setTabStopDistance(4 * editor->fontMetrics().horizontalAdvance(QLatin1Char(' ')));
    };
    zoomOne(m_editor);
    zoomOne(m_editor2);
}

void DemoThemeWindow::clickTools()
{
    if (!m_cursor)
        return;
    m_cursor->setPressed(true);
    if (m_menuBar && m_toolsAction)
        m_menuBar->setActiveAction(m_toolsAction);
    QTimer::singleShot(kClickMs, this, [this]() {
        if (m_cursor)
            m_cursor->setPressed(false);
        showFakeMenu();
    });
}

void DemoThemeWindow::showFakeMenu()
{
    if (!m_fakeMenu || !m_menuBar || !m_toolsAction)
        return;
    const QRect r = m_menuBar->actionGeometry(m_toolsAction);
    const QPoint p = m_menuBar->mapTo(this, r.bottomLeft());
    m_fakeMenu->adjustSize();
    m_fakeMenu->move(p);
    startMenuOpenAnimation();
}

void DemoThemeWindow::clipFakeMenu(int visibleHeight)
{
    if (!m_fakeMenu)
        return;
    const int w = qMax(1, m_fakeMenu->width());
    const int h = qMax(1, visibleHeight);
    m_fakeMenu->setMask(QRegion(0, 0, w, h));
}

void DemoThemeWindow::startMenuOpenAnimation()
{
    if (!m_fakeMenu)
        return;
    const int h = qMax(1, m_fakeMenu->height());
    clipFakeMenu(1);
    m_fakeMenu->show();
    m_fakeMenu->raise();
    if (m_cursor)
        m_cursor->raise();

    auto *anim = new QVariantAnimation(this);
    anim->setDuration(kMenuAnimMs);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->setStartValue(1);
    anim->setEndValue(h);
    connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        clipFakeMenu(value.toInt());
        if (m_cursor)
            m_cursor->raise();
    });
    connect(anim, &QVariantAnimation::finished, this, [this, anim]() {
        anim->deleteLater();
        if (m_fakeMenu)
            m_fakeMenu->clearMask();
        QTimer::singleShot(80, this, [this]() {
            moveCursorTo(themeItemHotPos(), AfterClickTheme, kCursorMs);
        });
    });
    anim->start();
}

void DemoThemeWindow::startMenuCloseAnimation()
{
    if (!m_fakeMenu || !m_fakeMenu->isVisible()) {
        if (m_menuBar)
            m_menuBar->setActiveAction(nullptr);
        applyDarkThemeAtDeadline();
        return;
    }

    const int h = qMax(1, m_fakeMenu->height());
    clipFakeMenu(h);

    auto *anim = new QVariantAnimation(this);
    anim->setDuration(kMenuAnimMs);
    anim->setEasingCurve(QEasingCurve::InCubic);
    anim->setStartValue(h);
    anim->setEndValue(1);
    connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        clipFakeMenu(value.toInt());
    });
    connect(anim, &QVariantAnimation::finished, this, [this, anim]() {
        anim->deleteLater();
        if (m_fakeMenu) {
            m_fakeMenu->clearMask();
            m_fakeMenu->hide();
        }
        if (m_menuBar)
            m_menuBar->setActiveAction(nullptr);
        applyDarkThemeAtDeadline();
    });
    anim->start();
}

void DemoThemeWindow::clickTheme()
{
    if (m_cursor)
        m_cursor->setPressed(true);
    QTimer::singleShot(kClickMs, this, [this]() {
        if (m_cursor)
            m_cursor->setPressed(false);
        startMenuCloseAnimation();
    });
}

int DemoThemeWindow::msUntil(int targetMs) const
{
    if (!m_demoClock.isValid())
        return 0;
    return qMax(0, targetMs - int(m_demoClock.elapsed()));
}

void DemoThemeWindow::applyDarkThemeAtDeadline()
{
    QTimer::singleShot(msUntil(kThemeSwitchMs), this, [this]() {
        applyDarkTheme();
        QTimer::singleShot(480, this, [this]() {
            moveCursorTo(secondTabHotPos(), AfterClickTab, 400);
        });
    });
}

void DemoThemeWindow::applyDarkTheme()
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

    statusBar()->setStyleSheet(QStringLiteral(
        "QStatusBar { background: #2a2a2a; color: #ffffff; border-top: 1px solid #555555; }"
        "QStatusBar::item { border: none; }"));

    applyEditorTheme(true);
}

void DemoThemeWindow::clickSecondTab()
{
    if (m_cursor)
        m_cursor->setPressed(true);
    QTimer::singleShot(110, this, [this]() {
        if (m_cursor)
            m_cursor->setPressed(false);
        animateTabSwitch(0, 1);
    });
}

void DemoThemeWindow::animateTabSwitch(int fromIndex, int toIndex)
{
    if (!m_tabs || fromIndex < 0 || toIndex < 0
        || fromIndex >= m_tabs->count() || toIndex >= m_tabs->count()) {
        finishDemo();
        return;
    }

    QWidget *fromWidget = m_tabs->widget(fromIndex);
    QWidget *toWidget = m_tabs->widget(toIndex);
    if (!fromWidget || !toWidget) {
        m_tabs->setCurrentIndex(toIndex);
        finishDemo();
        return;
    }

    QStackedWidget *stack = nullptr;
    const QList<QStackedWidget *> stacks =
        m_tabs->findChildren<QStackedWidget *>(QString(), Qt::FindDirectChildrenOnly);
    if (!stacks.isEmpty())
        stack = stacks.first();
    QWidget *parentForAnimation = stack ? static_cast<QWidget *>(stack) : static_cast<QWidget *>(m_tabs);

    QRect fromStart = fromWidget->geometry();
    if (fromStart.width() <= 0 || fromStart.height() <= 0)
        fromStart = QRect(0, 0, parentForAnimation->width(), parentForAnimation->height());
    const int width = fromStart.width();
    QRect fromEnd = fromStart;
    QRect toStart = fromStart;
    QRect toEnd = fromStart;
    if (toIndex > fromIndex) {
        fromEnd.moveLeft(fromStart.left() - width);
        toStart.moveLeft(fromStart.left() + width);
    } else {
        fromEnd.moveLeft(fromStart.left() + width);
        toStart.moveLeft(fromStart.left() - width);
    }

    if (m_tabs->tabBar())
        m_tabs->tabBar()->setCurrentIndex(toIndex);

    QWidget *fromOrigParent = fromWidget->parentWidget();
    QWidget *toOrigParent = toWidget->parentWidget();
    fromWidget->setParent(parentForAnimation);
    toWidget->setParent(parentForAnimation);
    fromWidget->setGeometry(fromStart);
    toWidget->setGeometry(toStart);
    fromWidget->show();
    toWidget->show();
    fromWidget->raise();
    toWidget->raise();
    if (m_cursor)
        m_cursor->raise();

    QPointer<QWidget> fromPtr = fromWidget;
    QPointer<QWidget> toPtr = toWidget;
    QPointer<QTabWidget> tabsPtr = m_tabs;

    auto *fromAnim = new QPropertyAnimation(fromWidget, "geometry", fromWidget);
    fromAnim->setDuration(250);
    fromAnim->setEasingCurve(QEasingCurve::InOutCubic);
    fromAnim->setStartValue(fromStart);
    fromAnim->setEndValue(fromEnd);

    auto *toAnim = new QPropertyAnimation(toWidget, "geometry", toWidget);
    toAnim->setDuration(250);
    toAnim->setEasingCurve(QEasingCurve::InOutCubic);
    toAnim->setStartValue(toStart);
    toAnim->setEndValue(toEnd);

    connect(toAnim, &QPropertyAnimation::finished, this,
            [this, fromPtr, toPtr, tabsPtr, toEnd, toIndex, fromOrigParent, toOrigParent]() {
        if (fromPtr && fromOrigParent)
            fromPtr->setParent(fromOrigParent);
        if (toPtr && toOrigParent)
            toPtr->setParent(toOrigParent);
        if (tabsPtr) {
            tabsPtr->setCurrentIndex(toIndex);
            if (fromPtr) {
                const int fromIdx = tabsPtr->indexOf(fromPtr);
                if (fromIdx >= 0 && fromIdx != tabsPtr->currentIndex())
                    fromPtr->hide();
            }
        }
        if (toPtr)
            toPtr->setGeometry(toEnd);
        if (m_editor2)
            m_editor2->setFocus();
        if (m_cursor)
            m_cursor->raise();
        finishDemo();
    });

    fromAnim->start(QAbstractAnimation::DeleteWhenStopped);
    toAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void DemoThemeWindow::finishDemo()
{
    QTimer::singleShot(1600, this, [this]() {
        if (m_cursor)
            m_cursor->hide();
        if (m_recorder)
            m_recorder->stop();
    });
}
