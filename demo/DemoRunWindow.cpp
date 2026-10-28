#include "DemoRunWindow.h"
#include "DemoCursor.h"
#include "DemoIcons.h"
#include "DemoScale.h"
#include "DemoRecorder.h"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QEvent>
#include <QFont>
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
#include <QSizePolicy>
#include <QStatusBar>
#include <QSyntaxHighlighter>
#include <QTabWidget>
#include <QTextCharFormat>
#include <QTextCursor>
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
            QStringLiteral("print")
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

DemoRunWindow::DemoRunWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setWindowTitle(QStringLiteral("Vuzhyk"));
    resize(1440, 900);
    buildChrome();
    buildEditor();
    buildOutputDock();

    m_code = QString::fromUtf8(
        "print(\"Привет, Vuzhyk!\")\n"
        "for i in range(4, 10):\n"
        "    print(i, \"*\", i, \"=\", i * i)\n");
    m_outputLines = QStringList()
        << QString::fromUtf8("Привет, Vuzhyk!")
        << QStringLiteral("4 * 4 = 16")
        << QStringLiteral("5 * 5 = 25")
        << QStringLiteral("6 * 6 = 36")
        << QStringLiteral("7 * 7 = 49")
        << QStringLiteral("8 * 8 = 64")
        << QStringLiteral("9 * 9 = 81");

    m_cursor = new DemoCursor(this);
    m_typeTimer = new QTimer(this);
    m_typeTimer->setInterval(48);
    connect(m_typeTimer, &QTimer::timeout, this, &DemoRunWindow::typeNext);

    m_cursorAnim = new QPropertyAnimation(m_cursor, "hotPos", this);
    m_cursorAnim->setDuration(560);
    m_cursorAnim->setEasingCurve(QEasingCurve::InOutCubic);
    connect(m_cursorAnim, &QPropertyAnimation::finished, this, &DemoRunWindow::onCursorArrived);

    m_recorder = new DemoRecorder(this);
}

DemoRunWindow::~DemoRunWindow()
{
    if (m_recorder)
        m_recorder->stop();
}

void DemoRunWindow::buildChrome()
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
        QString::fromUtf8("Выбрать Python..."),
        QString::fromUtf8("REPL"),
        QString::fromUtf8("Редактор задач pyrob"),
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
    m_runAction = addTb(QStringLiteral("play.svg"), QString::fromUtf8("Запустить"));
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
        "QScrollBar::handle:vertical:hover { background-color: #666666; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
        "QScrollBar:horizontal { background-color: #2a2a2a; height: %4px; border: none; }"
        "QScrollBar::handle:horizontal { background-color: #555555; min-width: %5px;"
        " border-radius: %6px; margin: 2px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }"
    ).arg(ds(2)).arg(ds(5)).arg(ds(15)).arg(ds(12)).arg(ds(20)).arg(ds(6)));
}

void DemoRunWindow::buildEditor()
{
    m_tabs = new QTabWidget(this);
    m_tabs->setTabsClosable(false);
    m_tabs->setMovable(false);

    m_editor = new QPlainTextEdit(m_tabs);
    m_editor->setFont(QFont(QStringLiteral("Consolas"), ds(11)));
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_editor->setPlaceholderText(QString());
    m_editor->setTabStopDistance(4 * m_editor->fontMetrics().horizontalAdvance(QLatin1Char(' ')));
    new PythonHighlighter(m_editor->document());
    m_editor->setStyleSheet(QStringLiteral(
        "QPlainTextEdit {"
        " background: #232323;"
        " color: #ffffff;"
        " border: none;"
        " padding: %1px;"
        " selection-background-color: #2a82da;"
        "}"
    ).arg(ds(8)));

    m_tabs->addTab(m_editor, QString::fromUtf8("Новый файл"));
    setCentralWidget(m_tabs);
}

void DemoRunWindow::buildOutputDock()
{
    m_outputDock = new QDockWidget(this);
    m_outputDock->setObjectName(QStringLiteral("OutputDock"));
    m_outputDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    m_outputDock->setAllowedAreas(Qt::BottomDockWidgetArea);

    auto *titleWidget = new QWidget(m_outputDock);
    titleWidget->setStyleSheet(QStringLiteral("background: #2a2a2a;"));
    auto *titleLayout = new QHBoxLayout(titleWidget);
    titleLayout->setContentsMargins(ds(6), 0, ds(6), 0);
    titleLayout->setSpacing(0);
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
        " background: #232323;"
        " color: #ffffff;"
        " border: none;"
        " padding: %1px;"
        "}"
    ).arg(ds(6)));
    m_outputDock->setWidget(m_output);
    m_outputDock->setMinimumHeight(ds(140));
    addDockWidget(Qt::BottomDockWidgetArea, m_outputDock);
    resizeDocks({ m_outputDock }, { ds(190) }, Qt::Vertical);
}

void DemoRunWindow::beginRunDemo()
{
    m_typeIndex = 0;
    if (m_editor)
        m_editor->clear();
    if (m_output)
        m_output->clear();
    if (m_recorder)
        m_recorder->start(this, QStringLiteral("DemoRun"));
    QTimer::singleShot(0, this, [this]() {
        if (m_outputDock)
            resizeDocks({ m_outputDock }, { ds(190) }, Qt::Vertical);
    });
    QTimer::singleShot(700, this, [this]() {
        if (m_typeTimer)
            m_typeTimer->start();
    });
}

void DemoRunWindow::appendTyped(const QString &chunk)
{
    if (!m_editor)
        return;
    m_editor->moveCursor(QTextCursor::End);
    m_editor->insertPlainText(chunk);
    m_editor->moveCursor(QTextCursor::End);
    m_editor->ensureCursorVisible();
}

void DemoRunWindow::typeNext()
{
    if (m_typeIndex >= m_code.size()) {
        m_typeTimer->stop();
        QTimer::singleShot(400, this, &DemoRunWindow::moveCursorToRun);
        return;
    }
    const QChar ch = m_code.at(m_typeIndex++);
    appendTyped(QString(ch));
    if (ch == QLatin1Char('\n'))
        m_typeTimer->setInterval(110);
    else if (ch == QLatin1Char(' '))
        m_typeTimer->setInterval(36);
    else
        m_typeTimer->setInterval(42 + (m_typeIndex % 7));
}

QPoint DemoRunWindow::cursorHotFor(QWidget *widget) const
{
    if (!widget)
        return QPoint(width() / 2, height() / 2);
    const QPoint local = widget->rect().center();
    return widget->mapTo(const_cast<DemoRunWindow *>(this), local);
}

QPoint DemoRunWindow::appearFromEditor() const
{
    if (!m_editor)
        return QPoint(width() / 3, height() / 3);
    QTextCursor tc = m_editor->textCursor();
    tc.movePosition(QTextCursor::End);
    QPoint local = m_editor->viewport()->mapTo(m_editor, m_editor->cursorRect(tc).center());
    local += QPoint(ds(24), ds(-18));
    local.setX(qBound(ds(18), local.x(), qMax(ds(20), m_editor->width() - ds(20))));
    local.setY(qBound(ds(18), local.y(), qMax(ds(20), m_editor->height() - ds(20))));
    return m_editor->mapTo(const_cast<DemoRunWindow *>(this), local);
}

QWidget *DemoRunWindow::runButtonWidget() const
{
    if (!m_toolBar || !m_runAction)
        return nullptr;
    return m_toolBar->widgetForAction(m_runAction);
}

void DemoRunWindow::moveCursorToRun()
{
    QWidget *target = runButtonWidget();
    if (!m_cursor || !m_cursorAnim || !target)
        return;
    const QPoint start = appearFromEditor();
    const QPoint end = cursorHotFor(target);
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

void DemoRunWindow::onCursorArrived()
{
    if (m_ignoreCursorFinish)
        return;
    clickRun();
}

void DemoRunWindow::clickRun()
{
    QWidget *target = runButtonWidget();
    auto *tb = qobject_cast<QToolButton *>(target);
    if (m_cursor)
        m_cursor->setPressed(true);
    if (tb)
        tb->setDown(true);
    QTimer::singleShot(120, this, [this, tb]() {
        if (tb)
            tb->setDown(false);
        if (m_cursor)
            m_cursor->setPressed(false);
        startOutput();
        QTimer::singleShot(280, this, [this]() {
            if (m_cursor)
                m_cursor->hide();
        });
    });
}

void DemoRunWindow::startOutput()
{
    if (m_output)
        m_output->clear();
    m_outputLine = 0;
    QTimer::singleShot(220, this, &DemoRunWindow::emitNextOutputLine);
}

void DemoRunWindow::emitNextOutputLine()
{
    if (!m_output) {
        finishDemo();
        return;
    }
    if (m_outputLine >= m_outputLines.size()) {
        QTextCharFormat gray;
        gray.setForeground(QColor("#9a9a9a"));
        QTextCursor cur = m_output->textCursor();
        cur.movePosition(QTextCursor::End);
        cur.setCharFormat(gray);
        cur.insertText(QString::fromUtf8("\n[Process exited with code 0]\n"));
        m_output->setTextCursor(cur);
        m_output->moveCursor(QTextCursor::End);
        finishDemo();
        return;
    }
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(m_outputLines.at(m_outputLine++) + QLatin1Char('\n'));
    m_output->moveCursor(QTextCursor::End);
    QTimer::singleShot(160, this, &DemoRunWindow::emitNextOutputLine);
}

void DemoRunWindow::finishDemo()
{
    QTimer::singleShot(1400, this, [this]() {
        if (m_recorder)
            m_recorder->stop();
    });
}

bool DemoRunWindow::eventFilter(QObject *watched, QEvent *event)
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

void DemoRunWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_cursor)
        m_cursor->raise();
}
