#include "DemoStepWindow.h"
#include "DemoCursor.h"
#include "DemoIcons.h"
#include "DemoScale.h"
#include "DemoRecorder.h"

#include <QAction>
#include <QAbstractItemView>
#include <QApplication>
#include <QDockWidget>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QList>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStatusBar>
#include <QSyntaxHighlighter>
#include <QTabWidget>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFormat>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QEasingCurve>

namespace {

class PythonHighlighter : public QSyntaxHighlighter {
public:
    explicit PythonHighlighter(QTextDocument *parent)
        : QSyntaxHighlighter(parent)
    {
        m_keyword.setForeground(QColor("#6cb6ff"));
        m_keyword.setFontWeight(QFont::Bold);
        m_string.setForeground(QColor("#ce9178"));
        m_comment.setForeground(QColor("#6a9955"));
        m_number.setForeground(QColor("#b5cea8"));
    }

protected:
    void highlightBlock(const QString &text) override
    {
        static const QStringList words = {
            QStringLiteral("from"), QStringLiteral("import"), QStringLiteral("for"),
            QStringLiteral("in"), QStringLiteral("range"), QStringLiteral("def"),
            QStringLiteral("if"), QStringLiteral("else"), QStringLiteral("while"),
            QStringLiteral("print"), QStringLiteral("return")
        };
        for (const QString &w : words) {
            int i = 0;
            while ((i = text.indexOf(w, i)) >= 0) {
                const bool leftOk = (i == 0) || !text.at(i - 1).isLetterOrNumber();
                const int end = i + w.size();
                const bool rightOk = (end >= text.size()) || !text.at(end).isLetterOrNumber();
                if (leftOk && rightOk)
                    setFormat(i, w.size(), m_keyword);
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
    QTextCharFormat m_keyword;
    QTextCharFormat m_string;
    QTextCharFormat m_comment;
    QTextCharFormat m_number;
};

} // namespace

DemoStepWindow::DemoStepWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setWindowTitle(QStringLiteral("Vuzhyk"));
    resize(1440, 900);
    buildChrome();
    buildEditor();
    buildOutputDock();
    buildScript();

    m_hint = new QListWidget(m_editor ? m_editor->viewport() : static_cast<QWidget *>(this));
    m_hint->setFocusPolicy(Qt::NoFocus);
    m_hint->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_hint->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_hint->setSelectionMode(QAbstractItemView::SingleSelection);
    m_hint->setFont(QFont(QStringLiteral("Consolas"), ds(10)));
    m_hint->setStyleSheet(QStringLiteral(
        "QListWidget {"
        " background: #252526; color: #e8e8e8;"
        " border: 1px solid #3c3c3c; outline: none; padding: 2px;"
        "}"
        "QListWidget::item { padding: %1px %2px; }"
        "QListWidget::item:selected { background: #094771; color: #ffffff; }"
    ).arg(ds(3)).arg(ds(10)));
    m_hint->hide();

    m_cursor = new DemoCursor(this);
    m_typeTimer = new QTimer(this);
    m_typeTimer->setInterval(40);
    connect(m_typeTimer, &QTimer::timeout, this, &DemoStepWindow::typeNext);

    m_cursorAnim = new QPropertyAnimation(m_cursor, "hotPos", this);
    m_cursorAnim->setDuration(520);
    m_cursorAnim->setEasingCurve(QEasingCurve::InOutCubic);
    connect(m_cursorAnim, &QPropertyAnimation::finished, this, &DemoStepWindow::onCursorArrived);

    m_recorder = new DemoRecorder(this);
}

DemoStepWindow::~DemoStepWindow()
{
    if (m_recorder)
        m_recorder->stop();
}

void DemoStepWindow::buildScript()
{
    auto typeEv = [](const QString &text) {
        ScriptEvent e;
        e.type = TypeChars;
        e.text = text;
        e.index = 0;
        e.count = 0;
        return e;
    };
    auto showEv = [](const QStringList &items, int index) {
        ScriptEvent e;
        e.type = ShowHint;
        e.items = items;
        e.index = index;
        e.count = 0;
        return e;
    };
    auto moveEv = [](int index) {
        ScriptEvent e;
        e.type = MoveHint;
        e.index = index;
        e.count = 0;
        return e;
    };
    auto acceptEv = [](const QString &suffix) {
        ScriptEvent e;
        e.type = AcceptHint;
        e.text = suffix;
        e.index = 0;
        e.count = 0;
        return e;
    };
    auto pauseEv = []() {
        ScriptEvent e;
        e.type = Pause;
        e.index = 0;
        e.count = 0;
        return e;
    };

    // Как в IDE: слова с тем же префиксом, по алфавиту.
    const QStringList printHints = QStringList()
        << QStringLiteral("print")
        << QStringLiteral("profile")
        << QStringLiteral("property");
    const QStringList forHints = QStringList()
        << QStringLiteral("for")
        << QStringLiteral("format")
        << QStringLiteral("frozenset");
    const QStringList rangeHints = QStringList()
        << QStringLiteral("raise")
        << QStringLiteral("random")
        << QStringLiteral("range");

    m_events.append(typeEv(QStringLiteral("pr")));
    m_events.append(showEv(printHints, 0));
    m_events.append(pauseEv());
    m_events.append(acceptEv(QStringLiteral("int")));
    m_events.append(typeEv(QString::fromUtf8("(\"Привет, Vuzhyk!\")\n")));
    m_events.append(typeEv(QStringLiteral("fo")));
    m_events.append(showEv(forHints, 0));
    m_events.append(pauseEv());
    m_events.append(moveEv(1));
    m_events.append(pauseEv());
    m_events.append(moveEv(0));
    m_events.append(pauseEv());
    m_events.append(acceptEv(QStringLiteral("r")));
    m_events.append(typeEv(QStringLiteral(" i in ")));
    m_events.append(typeEv(QStringLiteral("ra")));
    m_events.append(showEv(rangeHints, 0));
    m_events.append(pauseEv());
    m_events.append(moveEv(1));
    m_events.append(pauseEv());
    m_events.append(moveEv(2));
    m_events.append(pauseEv());
    m_events.append(acceptEv(QStringLiteral("nge")));
    m_events.append(typeEv(QStringLiteral("(1, 4):\n    ")));
    m_events.append(typeEv(QStringLiteral("pr")));
    m_events.append(showEv(printHints, 0));
    m_events.append(pauseEv());
    m_events.append(acceptEv(QStringLiteral("int")));
    m_events.append(typeEv(QStringLiteral("(i, \"*\", i, \"=\", i * i)\n")));

    auto step = [](int line, const QString &output, bool finished = false) {
        DebugStep s;
        s.line = line;
        s.output = output;
        s.finished = finished;
        return s;
    };
    m_debugSteps.append(step(1, QString::fromUtf8("Привет, Vuzhyk!")));
    m_debugSteps.append(step(2, QString()));
    m_debugSteps.append(step(2, QStringLiteral("1 * 1 = 1")));
    m_debugSteps.append(step(2, QStringLiteral("2 * 2 = 4")));
    m_debugSteps.append(step(2, QStringLiteral("3 * 3 = 9")));
    m_debugSteps.append(step(-1, QString(), true));
}

void DemoStepWindow::buildChrome()
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
    addDummyMenu(QString::fromUtf8("Запуск"), {
        QString::fromUtf8("Запустить скрипт"),
        QString::fromUtf8("Запуск с отладкой") });
    addDummyMenu(QString::fromUtf8("Инструменты"), {
        QString::fromUtf8("Выбрать Python..."),
        QString::fromUtf8("REPL"),
        QString::fromUtf8("Переключить тему") });
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

    m_toolBar = addToolBar(QString::fromUtf8("Основное"));
    m_toolBar->setMovable(false);
    m_toolBar->setIconSize(dsSize(16, 16));
    const QString icons = QStringLiteral(":/icons/icons/light-theme/");
    auto addTb = [this, icons](const QString &file, const QString &tip) {
        QAction *a = m_toolBar->addAction(
            demoThemedIcon(icons + file, QStringLiteral("#e0e0e0"), dsSize(16, 16)), tip);
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
    m_debugAction = addTb(QStringLiteral("bug.svg"), QString::fromUtf8("Запуск с отладкой"));
    m_stepAction = addTb(QStringLiteral("next-bug.svg"), QString::fromUtf8("Следующий шаг отладки"));
    addTb(QStringLiteral("run-in-terminal.svg"), QString::fromUtf8("В браузере"));

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
        "QDockWidget { background-color: #353535; color: #ffffff; }"
        "QDockWidget::title { background-color: #2a2a2a; color: #ffffff; padding: %2px; }"
        "QStatusBar { background-color: #2a2a2a; color: #ffffff; }"
        "QScrollBar:vertical { background-color: #2a2a2a; width: %4px; border: none; }"
        "QScrollBar::handle:vertical { background-color: #555555; min-height: %5px;"
        " border-radius: %6px; margin: 2px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QScrollBar:horizontal { background-color: #2a2a2a; height: %4px; border: none; }"
        "QScrollBar::handle:horizontal { background-color: #555555; min-width: %5px;"
        " border-radius: %6px; margin: 2px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }"
    ).arg(ds(2)).arg(ds(5)).arg(ds(15)).arg(ds(12)).arg(ds(20)).arg(ds(6)));
}

void DemoStepWindow::buildEditor()
{
    m_tabs = new QTabWidget(this);
    m_tabs->setTabsClosable(false);
    m_tabs->setMovable(false);
    m_editor = new QPlainTextEdit(m_tabs);
    m_editor->setFont(QFont(QStringLiteral("Consolas"), ds(11)));
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_editor->setTabStopDistance(4 * m_editor->fontMetrics().horizontalAdvance(QLatin1Char(' ')));
    new PythonHighlighter(m_editor->document());
    m_editor->setStyleSheet(QStringLiteral(
        "QPlainTextEdit {"
        " background: #232323; color: #ffffff; border: none; padding: %1px;"
        " selection-background-color: #2a82da;"
        "}"
    ).arg(ds(8)));
    m_tabs->addTab(m_editor, QString::fromUtf8("demo.py"));
    setCentralWidget(m_tabs);
}

void DemoStepWindow::buildOutputDock()
{
    m_outputDock = new QDockWidget(this);
    m_outputDock->setObjectName(QStringLiteral("OutputDock"));
    m_outputDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    m_outputDock->setAllowedAreas(Qt::BottomDockWidgetArea);

    auto *titleWidget = new QWidget(m_outputDock);
    titleWidget->setStyleSheet(QStringLiteral("background: #2a2a2a;"));
    auto *titleLayout = new QHBoxLayout(titleWidget);
    titleLayout->setContentsMargins(ds(6), 0, ds(6), 0);
    auto *titleLabel = new QLabel(QString::fromUtf8("Вывод"), titleWidget);
    titleLabel->setStyleSheet(QStringLiteral(
        "QLabel { padding: %1px; color: #ffffff; background: transparent; }").arg(ds(4)));
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    auto *toggleButton = new QPushButton(QString::fromUtf8("Консоль"), titleWidget);
    toggleButton->setFlat(true);
    toggleButton->setStyleSheet(QStringLiteral(
        "QPushButton { padding: %1px %2px; border: 1px solid transparent; color: #e0e0e0; background: transparent; }"
        "QPushButton:hover { background-color: #555555; }")
        .arg(ds(4)).arg(ds(8)));
    titleLayout->addWidget(toggleButton);
    m_outputDock->setTitleBarWidget(titleWidget);

    m_output = new QPlainTextEdit(m_outputDock);
    m_output->setReadOnly(true);
    m_output->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_output->setFont(QFont(QStringLiteral("Consolas"), ds(10)));
    m_output->setStyleSheet(QStringLiteral(
        "QPlainTextEdit {"
        " background: #1e1e1e; color: #d4d4d4; border: none; padding: %1px;"
        "}"
    ).arg(ds(6)));
    m_outputDock->setWidget(m_output);
    m_outputDock->setMinimumHeight(ds(140));
    addDockWidget(Qt::BottomDockWidgetArea, m_outputDock);
    resizeDocks({ m_outputDock }, { ds(190) }, Qt::Vertical);
}

void DemoStepWindow::beginStepDemo()
{
    m_eventIndex = 0;
    m_typeIndex = 0;
    m_debugIndex = 0;
    if (m_editor)
        m_editor->clear();
    if (m_output)
        m_output->clear();
    hideHint();
    if (m_recorder)
        m_recorder->start(this, QStringLiteral("DemoStep"));
    QTimer::singleShot(0, this, [this]() {
        if (m_outputDock)
            resizeDocks({ m_outputDock }, { ds(190) }, Qt::Vertical);
        if (m_editor)
            m_editor->setFocus();
    });
    QTimer::singleShot(600, this, &DemoStepWindow::runNextEvent);
}

void DemoStepWindow::runNextEvent()
{
    if (m_eventIndex >= m_events.size()) {
        QTimer::singleShot(380, this, [this]() {
            moveCursorTo(cursorHotFor(actionButton(m_debugAction)), AfterDebug);
        });
        return;
    }
    const ScriptEvent e = m_events.at(m_eventIndex++);
    switch (e.type) {
    case TypeChars:
        m_pendingType = e.text;
        m_typeIndex = 0;
        if (m_typeTimer)
            m_typeTimer->start();
        break;
    case ShowHint:
        showHint(e.items, e.index);
        QTimer::singleShot(420, this, &DemoStepWindow::runNextEvent);
        break;
    case MoveHint:
        if (m_hint)
            m_hint->setCurrentRow(e.index);
        QTimer::singleShot(220, this, &DemoStepWindow::runNextEvent);
        break;
    case AcceptHint:
        appendTyped(e.text);
        hideHint();
        QTimer::singleShot(80, this, &DemoStepWindow::runNextEvent);
        break;
    case Backspace:
        if (m_editor) {
            QTextCursor c = m_editor->textCursor();
            c.movePosition(QTextCursor::End);
            c.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, e.count);
            c.removeSelectedText();
            m_editor->setTextCursor(c);
        }
        QTimer::singleShot(90, this, &DemoStepWindow::runNextEvent);
        break;
    case Pause:
        QTimer::singleShot(280, this, &DemoStepWindow::runNextEvent);
        break;
    }
}

void DemoStepWindow::typeNext()
{
    if (m_typeIndex >= m_pendingType.size()) {
        m_typeTimer->stop();
        runNextEvent();
        return;
    }
    const QChar ch = m_pendingType.at(m_typeIndex++);
    appendTyped(QString(ch));
    if (ch == QLatin1Char('\n'))
        m_typeTimer->setInterval(100);
    else if (ch == QLatin1Char(' '))
        m_typeTimer->setInterval(34);
    else
        m_typeTimer->setInterval(38 + (m_typeIndex % 6));
}

void DemoStepWindow::appendTyped(const QString &chunk)
{
    if (!m_editor)
        return;
    m_editor->moveCursor(QTextCursor::End);
    m_editor->insertPlainText(chunk);
    m_editor->moveCursor(QTextCursor::End);
    m_editor->ensureCursorVisible();
}

void DemoStepWindow::showHint(const QStringList &items, int index)
{
    if (!m_hint)
        return;
    m_hint->clear();
    for (const QString &item : items)
        m_hint->addItem(item);
    m_hint->setCurrentRow(qBound(0, index, items.size() - 1));
    const int rows = qMax(1, items.size());
    const int rowH = m_hint->fontMetrics().height() + ds(8);
    m_hint->setFixedSize(ds(150), rowH * rows + ds(8));
    positionHint();
    m_hint->show();
    m_hint->raise();
}

void DemoStepWindow::hideHint()
{
    if (m_hint)
        m_hint->hide();
}

void DemoStepWindow::positionHint()
{
    if (!m_hint || !m_editor)
        return;
    const QRect cr = m_editor->cursorRect();
    m_hint->move(cr.bottomLeft() + QPoint(0, ds(2)));
}

void DemoStepWindow::highlightLine(int line)
{
    if (!m_editor)
        return;
    QTextCursor c(m_editor->document());
    c.movePosition(QTextCursor::Start);
    for (int i = 0; i < line; ++i)
        c.movePosition(QTextCursor::NextBlock);
    c.movePosition(QTextCursor::StartOfBlock);
    QTextEdit::ExtraSelection sel;
    sel.cursor = c;
    sel.format.setBackground(QColor("#4b3d1f"));
    sel.format.setProperty(QTextFormat::FullWidthSelection, true);
    m_editor->setExtraSelections(QList<QTextEdit::ExtraSelection>() << sel);
    m_editor->setTextCursor(c);
    m_editor->ensureCursorVisible();
}

void DemoStepWindow::moveCursorTo(const QPoint &end, AfterMove after)
{
    if (!m_cursor || !m_cursorAnim)
        return;
    QPoint start = m_cursor->hotPos();
    if (!m_cursor->isVisible() || start.isNull())
        start = appearFromEditor();
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

void DemoStepWindow::onCursorArrived()
{
    if (m_ignoreCursorFinish)
        return;
    const AfterMove after = m_afterMove;
    m_afterMove = AfterNone;
    if (after == AfterDebug)
        clickDebug();
    else if (after == AfterStep)
        clickStep();
}

QPoint DemoStepWindow::cursorHotFor(QWidget *widget) const
{
    if (!widget)
        return QPoint(width() / 2, height() / 2);
    return widget->mapTo(const_cast<DemoStepWindow *>(this), widget->rect().center());
}

QPoint DemoStepWindow::appearFromEditor() const
{
    if (!m_editor)
        return QPoint(width() / 3, height() / 3);
    QTextCursor tc = m_editor->textCursor();
    tc.movePosition(QTextCursor::End);
    QPoint local = m_editor->viewport()->mapTo(m_editor, m_editor->cursorRect(tc).center());
    local += QPoint(ds(24), ds(-18));
    return m_editor->mapTo(const_cast<DemoStepWindow *>(this), local);
}

QWidget *DemoStepWindow::actionButton(QAction *action) const
{
    if (!m_toolBar || !action)
        return nullptr;
    return m_toolBar->widgetForAction(action);
}

void DemoStepWindow::clickDebug()
{
    QWidget *target = actionButton(m_debugAction);
    auto *tb = qobject_cast<QToolButton *>(target);
    if (m_cursor)
        m_cursor->setPressed(true);
    if (tb)
        tb->setDown(true);
    QTimer::singleShot(110, this, [this, tb]() {
        if (tb)
            tb->setDown(false);
        if (m_cursor)
            m_cursor->setPressed(false);
        highlightLine(0);
        statusBar()->showMessage(QString::fromUtf8("Отладка запущена. Остановка на строке 1."));
        QTimer::singleShot(320, this, [this]() {
            moveCursorTo(cursorHotFor(actionButton(m_stepAction)), AfterStep);
        });
    });
}

void DemoStepWindow::clickStep()
{
    QWidget *target = actionButton(m_stepAction);
    auto *tb = qobject_cast<QToolButton *>(target);
    if (m_cursor)
        m_cursor->setPressed(true);
    if (tb)
        tb->setDown(true);
    QTimer::singleShot(100, this, [this, tb]() {
        if (tb)
            tb->setDown(false);
        if (m_cursor)
            m_cursor->setPressed(false);
        applyDebugStep();
    });
}

void DemoStepWindow::applyDebugStep()
{
    if (m_debugIndex >= m_debugSteps.size()) {
        finishDemo();
        return;
    }
    const DebugStep step = m_debugSteps.at(m_debugIndex++);
    if (step.line >= 0)
        highlightLine(step.line);
    if (!step.output.isEmpty() && m_output) {
        m_output->moveCursor(QTextCursor::End);
        m_output->insertPlainText(step.output + QLatin1Char('\n'));
        m_output->moveCursor(QTextCursor::End);
    }
    if (step.line >= 0)
        statusBar()->showMessage(QString::fromUtf8("Остановка на строке %1.").arg(step.line + 1));
    if (step.finished) {
        if (m_output) {
            QTextCharFormat gray;
            gray.setForeground(QColor("#9a9a9a"));
            QTextCursor cur = m_output->textCursor();
            cur.movePosition(QTextCursor::End);
            cur.setCharFormat(gray);
            cur.insertText(QString::fromUtf8("\n[Process exited with code 0]\n"));
            m_output->setTextCursor(cur);
        }
        statusBar()->showMessage(QString::fromUtf8("Отладка завершена"));
        QTimer::singleShot(280, this, [this]() {
            if (m_cursor)
                m_cursor->hide();
        });
        finishDemo();
        return;
    }
    QTimer::singleShot(420, this, &DemoStepWindow::clickStep);
}

void DemoStepWindow::finishDemo()
{
    QTimer::singleShot(1500, this, [this]() {
        if (m_recorder)
            m_recorder->stop();
    });
}

bool DemoStepWindow::eventFilter(QObject *watched, QEvent *event)
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

void DemoStepWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_hint && m_hint->isVisible())
        positionHint();
    if (m_cursor)
        m_cursor->raise();
}
