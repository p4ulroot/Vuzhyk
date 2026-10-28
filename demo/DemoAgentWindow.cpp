#include "DemoAgentWindow.h"
#include "DemoCursor.h"
#include "DemoIcons.h"
#include "DemoScale.h"
#include "DemoRecorder.h"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
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
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStatusBar>
#include <QSyntaxHighlighter>
#include <QTabWidget>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVector>
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

class CollapsibleCodeFrame : public QFrame {
public:
    CollapsibleCodeFrame(const QString &title, const QString &code, QWidget *parent)
        : QFrame(parent)
    {
        setObjectName(QStringLiteral("agentEditFrame"));
        setAttribute(Qt::WA_StyledBackground, true);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Maximum);
        setStyleSheet(QStringLiteral(
            "QFrame#agentEditFrame {"
            " background: #252526; border: 1px solid #3c3c3c; border-radius: 8px;"
            " border-left: 3px solid #2a82da;"
            "}"));
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        auto *header = new QWidget(this);
        header->setObjectName(QStringLiteral("agentCodeHeader"));
        header->setStyleSheet(QStringLiteral("QWidget#agentCodeHeader { background: #2d2d2d; }"));
        auto *headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(ds(8), ds(6), ds(8), ds(6));
        headerLayout->setSpacing(ds(6));

        auto *chevron = new QLabel(QString(QChar(0x25BE)), header);
        chevron->setFixedWidth(ds(12));
        chevron->setStyleSheet(QStringLiteral("QLabel { color: #e8e8e8; background: transparent; }"));
        headerLayout->addWidget(chevron);

        auto *titleLabel = new QLabel(title, header);
        QFont titleFont = titleLabel->font();
        titleFont.setPointSize(qMax(8, titleFont.pointSize() - 1));
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        titleLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #e8e8e8; background: transparent; }"));
        headerLayout->addWidget(titleLabel, 1);

        const int lines = qMax(1, code.count(QLatin1Char('\n')) + 1);
        auto *meta = new QLabel(QString::number(lines), header);
        meta->setStyleSheet(QStringLiteral(
            "QLabel { color: #9a9a9a; font-size: 11px; background: transparent; }"));
        headerLayout->addWidget(meta);
        root->addWidget(header);

        auto *body = new QPlainTextEdit(this);
        body->setReadOnly(true);
        body->setFrameShape(QFrame::NoFrame);
        body->setLineWrapMode(QPlainTextEdit::NoWrap);
        body->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        body->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        body->setPlainText(code);
        QFont mono(QStringLiteral("Consolas"));
        if (!mono.exactMatch())
            mono = QFont(QStringLiteral("Courier New"));
        mono.setStyleHint(QFont::Monospace);
        mono.setPointSize(ds(10));
        body->setFont(mono);
        body->setStyleSheet(QStringLiteral(
            "QPlainTextEdit { background: #1b1b1b; color: #e8e8e8; border: none; }"));
        body->setFixedHeight(body->fontMetrics().lineSpacing() * qBound(1, lines, 8) + ds(12));
        root->addWidget(body);
    }
};

} // namespace

DemoAgentWindow::DemoAgentWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setWindowTitle(QStringLiteral("Vuzhyk"));
    resize(1440, 900);
    buildChrome();
    buildEditor();
    buildAgentDock();

    const QString v0 = QString::fromUtf8(
        "def square(n)\n"
        "    retrun n * n\n"
        "\n"
        "print(square(5)\n");
    const QString v1 = QString::fromUtf8(
        "def square(n):\n"
        "    retrun n * n\n"
        "\n"
        "print(square(5)\n");
    const QString v2 = QString::fromUtf8(
        "def square(n):\n"
        "    return n * n\n"
        "\n"
        "print(square(5)\n");
    const QString v3 = QString::fromUtf8(
        "def square(n):\n"
        "    return n * n\n"
        "\n"
        "print(square(5))\n");
    const QString v4 = QString::fromUtf8(
        "def square(n):\n"
        "    return n * n\n"
        "\n"
        "print(square(5))\n"
        "print(square(9))\n");

    Turn findBugs;
    findBugs.prompt = QString::fromUtf8("Найди ошибки в коде");
    findBugs.waitStage = QString::fromUtf8("Ожидание ответа модели");
    findBugs.intro = QString::fromUtf8(
        "Нашёл 3 ошибки:\n"
        "— нет «:» после def\n"
        "— retrun → return\n"
        "— нет «)» у print\n"
        "Исправляю.");
    findBugs.edits.append(EditStep{
        QString::fromUtf8("Правка · строки 1-1"),
        QString::fromUtf8("def square(n):"),
        v1, 1, 1
    });
    findBugs.edits.append(EditStep{
        QString::fromUtf8("Правка · строки 2-2"),
        QString::fromUtf8("    return n * n"),
        v2, 2, 2
    });
    findBugs.edits.append(EditStep{
        QString::fromUtf8("Правка · строки 4-4"),
        QString::fromUtf8("print(square(5))"),
        v3, 4, 4
    });

    Turn addCall;
    addCall.prompt = QString::fromUtf8("Допиши вызов square(9)");
    addCall.waitStage = QString::fromUtf8("Ожидание ответа модели");
    addCall.intro = QString::fromUtf8("Добавляю вызов после print(square(5)).");
    addCall.edits.append(EditStep{
        QString::fromUtf8("Правка · вставка после 4"),
        QString::fromUtf8("print(square(9))"),
        v4, 5, 5
    });

    m_turns.clear();
    m_turns.append(findBugs);
    m_turns.append(addCall);
    m_prompt = m_turns.first().prompt;
    if (m_editor)
        m_editor->setPlainText(v0);

    m_cursor = new DemoCursor(this);
    m_typeTimer = new QTimer(this);
    m_typeTimer->setInterval(26);
    connect(m_typeTimer, &QTimer::timeout, this, &DemoAgentWindow::typeNext);

    m_waitTimer = new QTimer(this);
    m_waitTimer->setInterval(80);
    connect(m_waitTimer, &QTimer::timeout, this, &DemoAgentWindow::onWaitTick);

    m_cursorAnim = new QPropertyAnimation(m_cursor, "hotPos", this);
    m_cursorAnim->setDuration(380);
    m_cursorAnim->setEasingCurve(QEasingCurve::InOutCubic);
    connect(m_cursorAnim, &QPropertyAnimation::finished, this, &DemoAgentWindow::onCursorArrived);

    m_recorder = new DemoRecorder(this);
}

DemoAgentWindow::~DemoAgentWindow()
{
    if (m_recorder)
        m_recorder->stop();
}

void DemoAgentWindow::buildChrome()
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
        "QMenuBar::item:selected{background:#555555;}"
        "QMenuBar::item:checked{background:#555555;}")
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
    QAction *agentAct = m_menuBar->addAction(QString::fromUtf8("Агент"));
    agentAct->setCheckable(true);
    agentAct->setChecked(true);
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
        return m_toolBar->addAction(
            demoThemedIcon(icons + file, QStringLiteral("#e0e0e0"), dsSize(16, 16)), tip);
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
        "QStatusBar { background: #2a2a2a; color: #ffffff; border-top: 1px solid #555555; }"
        "QStatusBar::item { border: none; }"));
    statusBar()->showMessage(QString::fromUtf8("Готов"));
    statusBar()->show();

    qApp->setStyleSheet(QStringLiteral(
        "QMainWindow { background-color: #353535; }"
        "QMenuBar { background-color: #353535; color: #ffffff; }"
        "QMenuBar::item:selected, QMenuBar::item:checked { background-color: #555555; }"
        "QMenu { background-color: #353535; color: #ffffff; border: 1px solid #555555; }"
        "QMenu::item:selected { background-color: #555555; }"
        "QToolBar { background-color: #353535; border: none; spacing: %1px; padding: %1px; }"
        "QToolButton { background-color: transparent; color: #ffffff; }"
        "QToolButton:hover { background-color: #555555; }"
        "QTabWidget::pane { background-color: #232323; border: 1px solid #555555; }"
        "QTabBar::tab { background-color: #2a2a2a; color: #ffffff;"
        " border: 1px solid #555555; padding: %2px %3px; margin-right: 2px; }"
        "QTabBar::tab:selected { background-color: #353535; border-bottom-color: #232323; }"
        "QDockWidget { background-color: #1e1e1e; color: #ffffff; }"
        "QDockWidget::title { background-color: #2a2a2a; color: #ffffff; padding: %2px; }"
        "QStatusBar { background-color: #2a2a2a; color: #ffffff; }"
        "QScrollBar:vertical { background-color: #2a2a2a; width: %4px; border: none; }"
        "QScrollBar::handle:vertical { background-color: #555555; min-height: %5px;"
        " border-radius: %6px; margin: 2px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
    ).arg(ds(2)).arg(ds(5)).arg(ds(15)).arg(ds(12)).arg(ds(20)).arg(ds(6)));
}

void DemoAgentWindow::buildEditor()
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
    m_tabs->addTab(m_editor, QString::fromUtf8("square.py"));
    setCentralWidget(m_tabs);
}

void DemoAgentWindow::buildAgentDock()
{
    m_agentDock = new QDockWidget(this);
    m_agentDock->setObjectName(QStringLiteral("AgentDock"));
    m_agentDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    m_agentDock->setAllowedAreas(Qt::RightDockWidgetArea);

    auto *titleWidget = new QWidget(m_agentDock);
    titleWidget->setStyleSheet(QStringLiteral("background: #2a2a2a;"));
    auto *titleLay = new QHBoxLayout(titleWidget);
    titleLay->setContentsMargins(ds(8), 0, ds(8), 0);
    auto *titleLabel = new QLabel(QString::fromUtf8("Агент"), titleWidget);
    titleLabel->setStyleSheet(QStringLiteral(
        "QLabel { padding: %1px; color: #ffffff; background: transparent; }").arg(ds(4)));
    titleLay->addWidget(titleLabel);
    titleLay->addStretch();
    m_agentDock->setTitleBarWidget(titleWidget);

    auto *panel = new QWidget(m_agentDock);
    panel->setObjectName(QStringLiteral("agentPanel"));
    panel->setStyleSheet(QStringLiteral("QWidget#agentPanel { background: #1e1e1e; }"));
    auto *root = new QVBoxLayout(panel);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *header = new QWidget(panel);
    auto *headerLay = new QHBoxLayout(header);
    headerLay->setContentsMargins(ds(12), ds(8), ds(8), ds(4));
    headerLay->addStretch();
    auto *clearBtn = new QToolButton(header);
    clearBtn->setText(QString::fromUtf8("Очистить"));
    clearBtn->setAutoRaise(true);
    clearBtn->setStyleSheet(QStringLiteral(
        "QToolButton { color: #9a9a9a; border: none; padding: 4px 8px; font-size: 12px; }"));
    headerLay->addWidget(clearBtn);
    root->addWidget(header);

    m_chatScroll = new QScrollArea(panel);
    m_chatScroll->setFrameShape(QFrame::NoFrame);
    m_chatScroll->setWidgetResizable(true);
    m_chatScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_chatScroll->setStyleSheet(QStringLiteral("QScrollArea { background: #1e1e1e; border: none; }"));
    m_chatInner = new QWidget(m_chatScroll);
    m_chatInner->setStyleSheet(QStringLiteral("background: #1e1e1e;"));
    m_messagesLayout = new QVBoxLayout(m_chatInner);
    m_messagesLayout->setContentsMargins(ds(14), ds(12), ds(14), ds(18));
    m_messagesLayout->setSpacing(ds(14));
    m_emptyHint = new QLabel(QString::fromUtf8("Чем помочь?"), m_chatInner);
    m_emptyHint->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_emptyHint->setStyleSheet(QStringLiteral(
        "QLabel { color: #9a9a9a; font-size: 14px; padding: 48px 12px; background: transparent; }"));
    m_messagesLayout->addWidget(m_emptyHint);

    m_waitWidget = new QWidget(m_chatInner);
    auto *waitLay = new QVBoxLayout(m_waitWidget);
    waitLay->setContentsMargins(0, ds(2), 0, ds(4));
    waitLay->setSpacing(ds(4));
    m_waitDotsHost = new QWidget(m_waitWidget);
    m_waitDotsHost->setFixedSize(ds(32), ds(14));
    for (int i = 0; i < 3; ++i) {
        m_waitDot[i] = new QLabel(m_waitDotsHost);
        m_waitDot[i]->setFixedSize(ds(5), ds(5));
        m_waitDot[i]->setAttribute(Qt::WA_StyledBackground, true);
        m_waitDot[i]->setStyleSheet(QStringLiteral(
            "QLabel { background-color: #9a9a9a; border: none; border-radius: 2px; }"));
        m_waitDot[i]->move(ds(2) + i * ds(9), ds(8));
    }
    waitLay->addWidget(m_waitDotsHost, 0, Qt::AlignLeft);
    m_waitStageLabel = new QLabel(m_waitWidget);
    m_waitStageLabel->setWordWrap(true);
    m_waitStageLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #9a9a9a; font-size: 11px; background: transparent; }"));
    waitLay->addWidget(m_waitStageLabel);
    m_waitWidget->hide();
    m_messagesLayout->addWidget(m_waitWidget);
    m_messagesLayout->addStretch(1);
    m_chatScroll->setWidget(m_chatInner);
    root->addWidget(m_chatScroll, 1);

    auto *inputWrap = new QWidget(panel);
    auto *wrapLay = new QVBoxLayout(inputWrap);
    wrapLay->setContentsMargins(ds(12), ds(4), ds(12), ds(12));
    auto *card = new QFrame(inputWrap);
    card->setObjectName(QStringLiteral("agentInputCard"));
    card->setStyleSheet(QStringLiteral(
        "QFrame#agentInputCard {"
        " background: #252526; border: 1px solid #3c3c3c; border-radius: 14px;"
        "}"));
    auto *cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(ds(12), ds(10), ds(12), ds(8));
    cardLay->setSpacing(ds(8));

    m_input = new QPlainTextEdit(card);
    m_input->setPlaceholderText(QString::fromUtf8("Спросите что угодно…"));
    m_input->setFixedHeight(ds(56));
    m_input->setFrameShape(QFrame::NoFrame);
    m_input->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: transparent; color: #e8e8e8; border: none; font-size: 15px; }"));
    {
        QFont inputFont = m_input->font();
        inputFont.setPixelSize(15);
        m_input->setFont(inputFont);
    }
    cardLay->addWidget(m_input);

    auto *toolbar = new QHBoxLayout();
    toolbar->setSpacing(ds(6));
    auto *modeBtn = new QToolButton(card);
    modeBtn->setText(QString::fromUtf8("Code ▾"));
    modeBtn->setStyleSheet(QStringLiteral(
        "QToolButton {"
        " background: #2a3340; color: #8ab4f8; border: none; border-radius: 14px;"
        " padding: 5px 12px; font-size: 12px; font-weight: 600;"
        "}"));
    toolbar->addWidget(modeBtn);
    auto *modelBtn = new QToolButton(card);
    modelBtn->setText(QStringLiteral("qwen2.5-coder"));
    modelBtn->setAutoRaise(true);
    modelBtn->setStyleSheet(QStringLiteral(
        "QToolButton { color: #e8e8e8; border: 1px solid #3c3c3c; border-radius: 8px;"
        " padding: 4px 8px; font-size: 12px; }"));
    toolbar->addWidget(modelBtn, 1);
    toolbar->addStretch();
    m_sendBtn = new QToolButton(card);
    m_sendBtn->setObjectName(QStringLiteral("agentSendBtn"));
    m_sendBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_sendBtn->setCursor(Qt::PointingHandCursor);
    m_sendBtn->setIcon(demoThemedIcon(
        QStringLiteral(":/icons/icons/light-theme/up-arrow.svg"),
        QStringLiteral("#e8e8e8"), QSize(28, 28)));
    m_sendBtn->setIconSize(QSize(28, 28));
    m_sendBtn->setFixedSize(28, 28);
    m_sendBtn->setAutoRaise(true);
    m_sendBtn->setStyleSheet(QStringLiteral(
        "QToolButton { background: transparent; border: none; padding: 0; margin: 0; }"
        "QToolButton:hover { background: transparent; }"
        "QToolButton:pressed { background: transparent; }"));
    toolbar->addWidget(m_sendBtn);
    cardLay->addLayout(toolbar);
    wrapLay->addWidget(card);
    root->addWidget(inputWrap);

    m_agentDock->setWidget(panel);
    addDockWidget(Qt::RightDockWidgetArea, m_agentDock);
    m_agentDock->setMinimumWidth(ds(280));
    resizeDocks({ m_agentDock }, { ds(340) }, Qt::Horizontal);
}

void DemoAgentWindow::beginAgentDemo()
{
    m_turnIndex = 0;
    m_editIndex = 0;
    m_typeIndex = 0;
    m_streamIndex = 0;
    if (!m_turns.isEmpty())
        m_prompt = m_turns.first().prompt;
    if (m_recorder)
        m_recorder->start(this, QStringLiteral("DemoAgent"));
    QTimer::singleShot(0, this, [this]() {
        if (m_agentDock)
            resizeDocks({ m_agentDock }, { ds(340) }, Qt::Horizontal);
    });
    QTimer::singleShot(280, this, [this]() {
        moveCursorTo(inputHotPos(), AfterInput);
    });
}

void DemoAgentWindow::moveCursorTo(const QPoint &end, AfterMove after)
{
    if (!m_cursor || !m_cursorAnim)
        return;
    QPoint start = m_cursor->hotPos();
    if (!m_cursor->isVisible() || start.isNull())
        start = QPoint(width() / 2, height() / 2);
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

void DemoAgentWindow::onCursorArrived()
{
    if (m_ignoreCursorFinish)
        return;
    const AfterMove after = m_afterMove;
    m_afterMove = AfterNone;
    if (after == AfterInput) {
        if (m_input)
            m_input->setFocus();
        m_typeIndex = 0;
        if (m_typeTimer)
            m_typeTimer->start();
    } else if (after == AfterSend) {
        clickSend();
    }
}

QPoint DemoAgentWindow::inputHotPos() const
{
    if (!m_input)
        return QPoint(width() - ds(160), height() - ds(120));
    return m_input->mapTo(const_cast<DemoAgentWindow *>(this),
                          QPoint(ds(40), m_input->height() / 2));
}

QPoint DemoAgentWindow::sendHotPos() const
{
    if (!m_sendBtn)
        return inputHotPos();
    return m_sendBtn->mapTo(const_cast<DemoAgentWindow *>(this), m_sendBtn->rect().center());
}

void DemoAgentWindow::typeNext()
{
    if (m_typeIndex >= m_prompt.size()) {
        m_typeTimer->stop();
        QTimer::singleShot(120, this, [this]() {
            moveCursorTo(sendHotPos(), AfterSend);
        });
        return;
    }
    if (m_input) {
        m_input->moveCursor(QTextCursor::End);
        m_input->insertPlainText(QString(m_prompt.at(m_typeIndex)));
    }
    ++m_typeIndex;
}

void DemoAgentWindow::clickSend()
{
    if (m_cursor)
        m_cursor->setPressed(true);
    if (m_sendBtn)
        m_sendBtn->setDown(true);
    QTimer::singleShot(80, this, [this]() {
        if (m_sendBtn)
            m_sendBtn->setDown(false);
        if (m_cursor)
            m_cursor->setPressed(false);
        const QString text = m_prompt;
        if (m_input)
            m_input->clear();
        appendUserBubble(text);
        startWaiting();
        QTimer::singleShot(220, this, [this]() {
            if (m_cursor)
                m_cursor->hide();
        });
        const int waitMs = (m_turnIndex == 0) ? 1400 : 1200;
        QTimer::singleShot(waitMs, this, &DemoAgentWindow::startAssistantStream);
    });
}

void DemoAgentWindow::insertBeforeWait(QWidget *w)
{
    if (!m_messagesLayout || !w)
        return;
    const int idx = m_messagesLayout->indexOf(m_waitWidget);
    m_messagesLayout->insertWidget(idx < 0 ? m_messagesLayout->count() : idx, w);
}

void DemoAgentWindow::scrollChatToBottom()
{
    if (!m_chatScroll || !m_chatScroll->verticalScrollBar())
        return;
    m_chatScroll->verticalScrollBar()->setValue(m_chatScroll->verticalScrollBar()->maximum());
}

void DemoAgentWindow::appendUserBubble(const QString &text)
{
    if (m_emptyHint)
        m_emptyHint->hide();
    if (!m_messagesLayout)
        return;
    auto *wrap = new QWidget(m_chatInner);
    auto *lay = new QVBoxLayout(wrap);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(ds(4));
    auto *role = new QLabel(QString::fromUtf8("Вы"), wrap);
    role->setStyleSheet(QStringLiteral("QLabel { color: #9a9a9a; font-size: 11px; background: transparent; }"));
    lay->addWidget(role);
    auto *bubble = new QLabel(text, wrap);
    bubble->setWordWrap(true);
    bubble->setAttribute(Qt::WA_StyledBackground, true);
    bubble->setStyleSheet(QStringLiteral(
        "QLabel { background: #2f2f2f; color: #e8e8e8; border-radius: 12px;"
        " padding: 10px 12px; font-size: 13px; }"));
    lay->addWidget(bubble);
    insertBeforeWait(wrap);
    scrollChatToBottom();
}

void DemoAgentWindow::startWaiting()
{
    if (!m_waitWidget)
        return;
    const QString stage = (m_turnIndex >= 0 && m_turnIndex < m_turns.size())
        ? m_turns.at(m_turnIndex).waitStage
        : QString::fromUtf8("Ожидание ответа модели");
    if (m_waitStageLabel)
        m_waitStageLabel->setText(stage);
    m_waitFrame = 0;
    updateWaitDots();
    m_waitWidget->show();
    if (m_waitTimer && !m_waitTimer->isActive())
        m_waitTimer->start();
    scrollChatToBottom();
}

void DemoAgentWindow::stopWaiting()
{
    if (m_waitTimer)
        m_waitTimer->stop();
    if (m_waitWidget)
        m_waitWidget->hide();
}

void DemoAgentWindow::onWaitTick()
{
    m_waitFrame = (m_waitFrame + 1) % 8;
    updateWaitDots();
}

void DemoAgentWindow::updateWaitDots()
{
    static const int kBounce[] = { 0, 1, 2, 4, 5, 4, 2, 1 };
    for (int i = 0; i < 3; ++i) {
        if (!m_waitDot[i])
            continue;
        const int lift = ds(kBounce[(m_waitFrame + (2 - i) * 2) % 8]);
        m_waitDot[i]->move(ds(2) + i * ds(9), ds(8) - lift);
    }
}

void DemoAgentWindow::startAssistantStream()
{
    stopWaiting();
    if (!m_messagesLayout || m_turnIndex < 0 || m_turnIndex >= m_turns.size())
        return;
    m_assistantWrap = new QWidget(m_chatInner);
    m_assistantLay = new QVBoxLayout(m_assistantWrap);
    m_assistantLay->setContentsMargins(0, 0, 0, 0);
    m_assistantLay->setSpacing(ds(6));
    auto *role = new QLabel(QString::fromUtf8("Агент"), m_assistantWrap);
    role->setStyleSheet(QStringLiteral("QLabel { color: #9a9a9a; font-size: 11px; background: transparent; }"));
    m_assistantLay->addWidget(role);
    m_assistantLabel = new QLabel(m_assistantWrap);
    m_assistantLabel->setWordWrap(true);
    m_assistantLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #e8e8e8; font-size: 13px; background: transparent; }"));
    m_assistantLay->addWidget(m_assistantLabel);
    insertBeforeWait(m_assistantWrap);
    m_streamIndex = 0;
    m_editIndex = 0;
    streamNext();
}

void DemoAgentWindow::streamNext()
{
    if (!m_assistantLabel || m_turnIndex < 0 || m_turnIndex >= m_turns.size())
        return;
    const QString &reply = m_turns.at(m_turnIndex).intro;
    if (m_streamIndex >= reply.size()) {
        QTimer::singleShot(280, this, &DemoAgentWindow::showNextEdit);
        return;
    }
    int step = 2;
    if (m_streamIndex + step > reply.size())
        step = reply.size() - m_streamIndex;
    m_assistantLabel->setText(reply.left(m_streamIndex + step));
    m_streamIndex += step;
    scrollChatToBottom();
    QTimer::singleShot(18, this, &DemoAgentWindow::streamNext);
}

void DemoAgentWindow::showNextEdit()
{
    if (m_turnIndex < 0 || m_turnIndex >= m_turns.size() || !m_assistantLay) {
        startNextTurn();
        return;
    }
    const Turn &turn = m_turns.at(m_turnIndex);
    if (m_editIndex >= turn.edits.size()) {
        auto *done = new QLabel(QString::fromUtf8("Готово."), m_assistantWrap);
        done->setStyleSheet(QStringLiteral(
            "QLabel { color: #9a9a9a; font-size: 12px; background: transparent; }"));
        m_assistantLay->addWidget(done);
        scrollChatToBottom();
        statusBar()->showMessage(QString::fromUtf8("Код обновлён"));
        QTimer::singleShot(450, this, &DemoAgentWindow::startNextTurn);
        return;
    }
    const EditStep &step = turn.edits.at(m_editIndex++);
    m_assistantLay->addWidget(new CollapsibleCodeFrame(step.title, step.body, m_assistantWrap));
    applyEditorEdit(step);
    scrollChatToBottom();
    QTimer::singleShot(520, this, &DemoAgentWindow::showNextEdit);
}

void DemoAgentWindow::applyEditorEdit(const EditStep &step)
{
    if (!m_editor)
        return;
    m_editor->setPlainText(step.editorText);
    QTextCursor start(m_editor->document());
    start.movePosition(QTextCursor::Start);
    for (int i = 1; i < step.startLine; ++i)
        start.movePosition(QTextCursor::NextBlock);
    QTextCursor end = start;
    for (int i = step.startLine; i < step.endLine; ++i)
        end.movePosition(QTextCursor::NextBlock);
    end.movePosition(QTextCursor::EndOfBlock);
    start.setPosition(end.position(), QTextCursor::KeepAnchor);
    m_editor->setTextCursor(start);
    m_editor->ensureCursorVisible();
}

void DemoAgentWindow::startNextTurn()
{
    if (m_turnIndex + 1 >= m_turns.size()) {
        QTimer::singleShot(1600, this, &DemoAgentWindow::finishDemo);
        return;
    }
    ++m_turnIndex;
    m_editIndex = 0;
    m_typeIndex = 0;
    m_prompt = m_turns.at(m_turnIndex).prompt;
    QTimer::singleShot(280, this, [this]() {
        moveCursorTo(inputHotPos(), AfterInput);
    });
}

void DemoAgentWindow::finishDemo()
{
    if (m_cursor)
        m_cursor->hide();
    if (m_recorder)
        m_recorder->stop();
}

bool DemoAgentWindow::eventFilter(QObject *watched, QEvent *event)
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

void DemoAgentWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_cursor)
        m_cursor->raise();
}
