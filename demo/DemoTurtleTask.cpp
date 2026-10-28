#include "DemoTurtleTask.h"
#include "DemoTurtleCanvas.h"
#include "DemoIcons.h"
#include "DemoScale.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSize>
#include <QSplitter>
#include <QTimer>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QVBoxLayout>
#include <QFont>

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
            QStringLiteral("if"), QStringLiteral("else"), QStringLiteral("while")
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

DemoTurtleTask::DemoTurtleTask(QWidget *parent)
    : QWidget(parent)
{
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);

    m_editor = new QPlainTextEdit(m_splitter);
    m_editor->setFont(QFont(QStringLiteral("Consolas"), ds(11)));
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_editor->setPlaceholderText(QString());
    m_editor->setTabStopDistance(4 * m_editor->fontMetrics().horizontalAdvance(QLatin1Char(' ')));
    m_highlighter = new DemoPythonHighlighter(m_editor->document());
    m_editor->setStyleSheet(QStringLiteral(
        "QPlainTextEdit {"
        " background: #ffffff;"
        " color: #222222;"
        " border: none;"
        " padding: %1px;"
        "}"
    ).arg(ds(8)));

    m_panel = new QWidget(m_splitter);
    m_panel->setMinimumWidth(ds(260));
    m_panel->setMaximumWidth(ds(460));
    auto *panelLayout = new QVBoxLayout(m_panel);
    panelLayout->setContentsMargins(ds(10), ds(10), ds(10), ds(10));
    panelLayout->setSpacing(ds(8));

    m_instruction = new QLabel(m_panel);
    m_instruction->setWordWrap(true);
    m_instruction->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_instruction->setMinimumHeight(ds(48));
    m_instruction->setTextInteractionFlags(Qt::TextSelectableByMouse);
    setInstruction(QString::fromUtf8(
        "Нарисуй домик по образцу: квадрат — это стены, сверху — треугольная крыша. "
        "Синяя линия на экране показывает, каким должен получиться рисунок."));

    m_canvas = new DemoTurtleCanvas(m_panel);

    auto *controlRow = new QWidget(m_panel);
    auto *btnLayout = new QHBoxLayout(controlRow);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(ds(8));

    m_runBtn = new QPushButton(QString::fromUtf8("Выполнить"), controlRow);
    m_runBtn->setCursor(Qt::PointingHandCursor);
    m_runBtn->setMinimumHeight(ds(28));
    m_stepBtn = new QPushButton(QString::fromUtf8("Шаг"), controlRow);
    m_stepBtn->setCursor(Qt::PointingHandCursor);
    m_stepBtn->setMinimumHeight(ds(28));
    m_resetBtn = new QPushButton(QString::fromUtf8("Сброс"), controlRow);
    m_resetBtn->setCursor(Qt::PointingHandCursor);
    m_resetBtn->setMinimumHeight(ds(28));
    btnLayout->addWidget(m_runBtn, 1);
    btnLayout->addWidget(m_stepBtn, 1);
    btnLayout->addWidget(m_resetBtn, 1);

    auto *catalogScroll = new QScrollArea(m_panel);
    catalogScroll->setWidgetResizable(true);
    catalogScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    catalogScroll->setFrameShape(QFrame::NoFrame);
    catalogScroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"));
    auto *catalogHost = new QWidget(catalogScroll);
    catalogScroll->setWidget(catalogHost);
    auto *catLayout = new QVBoxLayout(catalogHost);
    catLayout->setContentsMargins(0, ds(8), 0, 0);
    catLayout->setSpacing(ds(10));
    auto *chapter = new QLabel(QString::fromUtf8("Знакомство с черепахой"), catalogHost);
    chapter->setObjectName(QStringLiteral("taskChapterTitle"));
    chapter->setWordWrap(true);
    catLayout->addWidget(chapter);
    auto *row = new QWidget(catalogHost);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(ds(6));
    for (int i = 1; i <= 6; ++i) {
        auto *btn = new QPushButton(QString::number(i), row);
        btn->setObjectName(QStringLiteral("taskNumBtn"));
        btn->setFixedSize(dsSize(32, 28));
        btn->setCheckable(true);
        btn->setChecked(i == 6);
        btn->setCursor(Qt::PointingHandCursor);
        rowLayout->addWidget(btn);
        m_catalogButtons.append(btn);
    }
    rowLayout->addStretch(1);
    catLayout->addWidget(row);
    catLayout->addStretch(1);

    panelLayout->addWidget(m_instruction, 0);
    panelLayout->addWidget(m_canvas, 0);
    panelLayout->addWidget(controlRow, 0);
    panelLayout->addWidget(catalogScroll, 1);

    m_splitter->addWidget(m_editor);
    m_splitter->addWidget(m_panel);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);
    m_splitter->setSizes({ ds(720), ds(320) });

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(m_splitter);

    m_panel->installEventFilter(this);
    m_canvas->installEventFilter(this);
    applyTheme();
    QTimer::singleShot(0, this, [this]() { updateCanvasHeight(); });
}

void DemoTurtleTask::setInstruction(const QString &text, const QString &kind)
{
    m_kind = kind;
    if (m_instruction)
        m_instruction->setText(text);
    applyTheme();
}

QPushButton *DemoTurtleTask::catalogButton(int number) const
{
    if (number < 1 || number > m_catalogButtons.size())
        return nullptr;
    return m_catalogButtons.at(number - 1);
}

void DemoTurtleTask::setCatalogSelected(int number)
{
    for (int i = 0; i < m_catalogButtons.size(); ++i)
        m_catalogButtons.at(i)->setChecked(i + 1 == number);
}

void DemoTurtleTask::appendTyped(const QString &chunk)
{
    if (!m_editor)
        return;
    m_editor->moveCursor(QTextCursor::End);
    m_editor->insertPlainText(chunk);
    m_editor->moveCursor(QTextCursor::End);
    m_editor->ensureCursorVisible();
}

bool DemoTurtleTask::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == m_panel || watched == m_canvas) && event && event->type() == QEvent::Resize)
        updateCanvasHeight();
    return QWidget::eventFilter(watched, event);
}

void DemoTurtleTask::updateCanvasHeight()
{
    if (!m_canvas)
        return;
    const int w = m_canvas->width();
    if (w <= 0)
        return;
    const int h = m_canvas->heightForWidth(w);
    if (m_canvas->height() != h)
        m_canvas->setFixedHeight(h);
}

void DemoTurtleTask::setTheme(const QString &theme)
{
    m_theme = theme;
    if (m_highlighter)
        m_highlighter->setDark(theme == QLatin1String("dark"));
    if (m_editor) {
        if (theme == QLatin1String("dark")) {
            m_editor->setStyleSheet(QStringLiteral(
                "QPlainTextEdit {"
                " background: #232323;"
                " color: #ffffff;"
                " border: none;"
                " padding: %1px;"
                " selection-background-color: #2a82da;"
                "}"
            ).arg(ds(8)));
        } else {
            m_editor->setStyleSheet(QStringLiteral(
                "QPlainTextEdit {"
                " background: #ffffff;"
                " color: #222222;"
                " border: none;"
                " padding: %1px;"
                "}"
            ).arg(ds(8)));
        }
    }
    applyTheme();
}

void DemoTurtleTask::applyTheme()
{
    const bool dark = (m_theme == QLatin1String("dark"));
    QString boxBg = dark ? QStringLiteral("#5a4e2a") : QStringLiteral("#fff3c4");
    QString boxBorder = dark ? QStringLiteral("#8a7a3a") : QStringLiteral("#e6d48a");
    QString boxText = dark ? QStringLiteral("#f5f0d8") : QStringLiteral("#333333");
    if (m_kind == QLatin1String("ok")) {
        boxBg = dark ? QStringLiteral("#1f3d1f") : QStringLiteral("#e4f5e4");
        boxBorder = dark ? QStringLiteral("#3d7a3d") : QStringLiteral("#8bc48b");
        boxText = dark ? QStringLiteral("#b6f0b6") : QStringLiteral("#1b7a1b");
    } else if (m_kind == QLatin1String("error")) {
        boxBg = dark ? QStringLiteral("#3d1f1f") : QStringLiteral("#fde8e8");
        boxBorder = dark ? QStringLiteral("#8a3a3a") : QStringLiteral("#e0a0a0");
        boxText = dark ? QStringLiteral("#ffb4b4") : QStringLiteral("#b00020");
    }
    if (m_panel) {
        m_panel->setStyleSheet(dark
            ? QStringLiteral("QWidget { background: #2b2b2b; }")
            : QStringLiteral("QWidget { background: #ececec; }"));
    }
    if (m_instruction) {
        m_instruction->setStyleSheet(QString(
            "QLabel {"
            " background: %1; color: %2; border: 1px solid %3;"
            " border-radius: %4px; padding: %5px; font-size: %6px;"
            "}"
        ).arg(boxBg, boxText, boxBorder)
         .arg(ds(4)).arg(ds(10)).arg(ds(12)));
    }
    const QString iconColor = dark ? QStringLiteral("#e0e0e0") : QStringLiteral("#000000");
    const QString btnStyle = dark
        ? QStringLiteral(
            "QPushButton {"
            " background-color: #4a4a4a; color: #ffffff;"
            " border: 1px solid #666666; border-radius: %1px; padding: %2px %3px;"
            "}"
            "QPushButton:hover { background-color: #555555; border-color: #777777; }"
            "QPushButton:pressed { background-color: #3a3a3a; }"
            "QPushButton#taskNumBtn {"
            " background-color: #3a3a3a; color: #ffffff;"
            " border: 1px solid #666666; border-radius: %1px; padding: 0px; font-weight: 600;"
            "}"
            "QPushButton#taskNumBtn:checked {"
            " background-color: #2a82da; color: #ffffff; border-color: #2a82da;"
            "}"
        ).arg(ds(3)).arg(ds(5)).arg(ds(8))
        : QStringLiteral(
            "QPushButton {"
            " background-color: #f0f0f0; color: #000000;"
            " border: 1px solid #adadad; border-radius: %1px; padding: %2px %3px;"
            "}"
            "QPushButton:hover { background-color: #e5e5e5; border-color: #888888; }"
            "QPushButton:pressed { background-color: #d0d0d0; }"
            "QPushButton#taskNumBtn {"
            " background-color: #ffffff; color: #000000;"
            " border: 1px solid #adadad; border-radius: %1px; padding: 0px; font-weight: 600;"
            "}"
            "QPushButton#taskNumBtn:checked {"
            " background-color: #2f6fed; color: #ffffff; border-color: #2f6fed;"
            "}"
        ).arg(ds(3)).arg(ds(5)).arg(ds(8));
    const QIcon play = demoThemedIcon(QStringLiteral(":/icons/icons/light-theme/play.svg"), iconColor, dsSize(16, 16));
    const QIcon step = demoThemedIcon(QStringLiteral(":/icons/icons/light-theme/arrow-right.svg"), iconColor, dsSize(16, 16));
    const QIcon undo = demoThemedIcon(QStringLiteral(":/icons/icons/light-theme/undo.svg"), iconColor, dsSize(16, 16));
    if (m_runBtn) {
        m_runBtn->setIcon(play);
        m_runBtn->setIconSize(dsSize(16, 16));
        m_runBtn->setStyleSheet(btnStyle);
    }
    if (m_stepBtn) {
        m_stepBtn->setIcon(step);
        m_stepBtn->setIconSize(dsSize(16, 16));
        m_stepBtn->setStyleSheet(btnStyle);
    }
    if (m_resetBtn) {
        m_resetBtn->setIcon(undo);
        m_resetBtn->setIconSize(dsSize(16, 16));
        m_resetBtn->setStyleSheet(btnStyle);
    }
    const QList<QPushButton *> nums = m_panel ? m_panel->findChildren<QPushButton *>(QStringLiteral("taskNumBtn")) : QList<QPushButton *>();
    for (QPushButton *btn : nums)
        btn->setStyleSheet(btnStyle);
    const QString chapterColor = dark ? QStringLiteral("#e8e8e8") : QStringLiteral("#222222");
    const QList<QLabel *> titles = m_panel ? m_panel->findChildren<QLabel *>(QStringLiteral("taskChapterTitle")) : QList<QLabel *>();
    for (QLabel *title : titles) {
        title->setStyleSheet(QString(
            "QLabel { color: %1; font-size: %2px; font-weight: 600; background: transparent; }")
            .arg(chapterColor).arg(ds(13)));
    }
}
