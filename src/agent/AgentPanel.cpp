#include "AgentPanel.h"
#include "LmStudioClient.h"
#include "AgentOpenRouterKeys.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <QIcon>
#include <QResizeEvent>
#include <QDockWidget>
#include <QSizePolicy>
#include <QSettings>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QEvent>
#include <QTimer>
#include <QScrollBar>
#include <QScrollArea>
#include <QLayout>
#include <QSignalBlocker>
#include <QJsonObject>
#include <QActionGroup>
#include <QMouseEvent>
#include <QFile>
#include <QRegularExpression>
#include <QPlainTextEdit>
#include <QFont>
#include <QVector>
#include <algorithm>

namespace {

constexpr char kSettingsGroup[] = "agent";
constexpr char kSendIconPath[] = ":/icons/icons/light-theme/up-arrow.svg";
constexpr char kKeyIconPath[] = ":/icons/icons/light-theme/key.svg";
constexpr int kToolbarSpacing = 6;
constexpr int kInputHorizontalMargins = 24; // wrap 12+12
constexpr int kCardHorizontalMargins = 24;  // card 12+12
constexpr int kKeyIconOnlyWidth = 32;
constexpr int kKeyIconSize = 13;
constexpr int kModelPickerMinWidth = 144;
constexpr int kSendButtonWidth = 28;
constexpr char kOpenRouterBaseUrl[] = "https://openrouter.ai/api/v1";
constexpr char kProviderLmStudio[] = "lmstudio";
constexpr char kProviderOpenRouter[] = "openrouter";

QByteArray recolorSvg(const QString &resourcePath, const QString &newColor)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    QByteArray svgData = file.readAll();
    file.close();

    QString svgString = QString::fromUtf8(svgData);
    svgString.replace(QStringLiteral("#000000"), newColor, Qt::CaseInsensitive);
    svgString.replace(QStringLiteral("black"), newColor, Qt::CaseInsensitive);
    svgString.replace(QStringLiteral("rgb(0,0,0)"), newColor, Qt::CaseInsensitive);
    return svgString.toUtf8();
}

QIcon themedSvgIcon(const QString &resourcePath, const QString &theme, int size = 16)
{
    QIcon icon;
    QByteArray svgData;
    if (theme == QLatin1String("dark")) {
        svgData = recolorSvg(resourcePath, QStringLiteral("#e8e8e8"));
    } else {
        QFile file(resourcePath);
        if (file.open(QIODevice::ReadOnly)) {
            svgData = file.readAll();
        }
    }
    if (svgData.isEmpty()) {
        return icon;
    }
    QSvgRenderer renderer(svgData);
    if (!renderer.isValid()) {
        return icon;
    }
    for (int s : {size, size * 2}) {
        QPixmap pixmap(s, s);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        renderer.render(&painter);
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

class AgentApiKeyPicker : public QFrame {
public:
    explicit AgentApiKeyPicker(QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    void setComfortableWidth(int width)
    {
        width = qMax(width, m_iconWidth);
        if (m_comfortableWidth == width) {
            return;
        }
        m_comfortableWidth = width;
        updateGeometry();
    }

    void setIconWidth(int width)
    {
        if (m_iconWidth == width) {
            return;
        }
        m_iconWidth = width;
        setMinimumWidth(width);
        updateGeometry();
    }

    QSize sizeHint() const override
    {
        return QSize(m_comfortableWidth, qMax(28, QFrame::sizeHint().height()));
    }

    QSize minimumSizeHint() const override
    {
        return QSize(m_iconWidth, qMax(28, QFrame::minimumSizeHint().height()));
    }

private:
    int m_comfortableWidth { 90 };
    int m_iconWidth { 32 };
};

enum class ChatSegType { Text, Code, Edit };

struct ChatSegment {
    ChatSegType type { ChatSegType::Text };
    QString title;
    QString body;
};

QString editTitleFromHeaders(const QString &headers)
{
    auto headerValue = [&headers](const QString &key) {
        const QRegularExpression re(
            QStringLiteral("(?im)^\\s*%1\\s*:\\s*(.+)\\s*$").arg(QRegularExpression::escape(key)));
        const QRegularExpressionMatch m = re.match(headers);
        return m.hasMatch() ? m.captured(1).trimmed() : QString();
    };
    const QString type = headerValue(QStringLiteral("type")).toLower();
    if (type == QLatin1String("replace_lines") || type == QLatin1String("replace-lines")) {
        const QString start = headerValue(QStringLiteral("start"));
        const QString end = headerValue(QStringLiteral("end"));
        if (start.isEmpty()) {
            return QObject::tr("Правка");
        }
        return QObject::tr("Правка · строки %1-%2").arg(start, end.isEmpty() ? start : end);
    }
    if (type == QLatin1String("insert_after") || type == QLatin1String("insert-after")) {
        QString line = headerValue(QStringLiteral("line"));
        if (line.isEmpty()) {
            line = headerValue(QStringLiteral("after"));
        }
        return QObject::tr("Правка · вставка после %1").arg(line);
    }
    if (type == QLatin1String("delete_lines") || type == QLatin1String("delete-lines")) {
        const QString start = headerValue(QStringLiteral("start"));
        const QString end = headerValue(QStringLiteral("end"));
        return QObject::tr("Правка · удаление %1-%2").arg(start, end.isEmpty() ? start : end);
    }
    return QObject::tr("Правка · файл");
}

QVector<ChatSegment> parseChatSegments(const QString &text)
{
    struct Marker {
        int start { 0 };
        int end { 0 };
        ChatSegment seg;
    };
    QVector<Marker> markers;

    const QRegularExpression editRe(
        QStringLiteral("<<<VUZHYK_EDIT\\s*\\n([\\s\\S]*?)>>>\\s*\\n([\\s\\S]*?)<<<END_EDIT"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator eit = editRe.globalMatch(text);
    while (eit.hasNext()) {
        const QRegularExpressionMatch m = eit.next();
        Marker mk;
        mk.start = m.capturedStart();
        mk.end = m.capturedEnd();
        mk.seg.type = ChatSegType::Edit;
        mk.seg.title = editTitleFromHeaders(m.captured(1));
        mk.seg.body = m.captured(2);
        markers.append(mk);
    }

    const QRegularExpression fenceRe(QStringLiteral("```([^\\n`]*)\\n([\\s\\S]*?)```"));
    QRegularExpressionMatchIterator fit = fenceRe.globalMatch(text);
    while (fit.hasNext()) {
        const QRegularExpressionMatch m = fit.next();
        bool overlaps = false;
        for (const Marker &mk : markers) {
            if (m.capturedStart() < mk.end && m.capturedEnd() > mk.start) {
                overlaps = true;
                break;
            }
        }
        if (overlaps) {
            continue;
        }
        Marker mk;
        mk.start = m.capturedStart();
        mk.end = m.capturedEnd();
        mk.seg.type = ChatSegType::Code;
        const QString lang = m.captured(1).trimmed();
        mk.seg.title = lang.isEmpty() ? QObject::tr("Код") : lang;
        mk.seg.body = m.captured(2);
        if (mk.seg.body.endsWith(QLatin1Char('\n'))) {
            mk.seg.body.chop(1);
        }
        markers.append(mk);
    }

    std::sort(markers.begin(), markers.end(),
              [](const Marker &a, const Marker &b) { return a.start < b.start; });

    QVector<ChatSegment> out;
    int pos = 0;
    for (const Marker &mk : markers) {
        if (mk.start > pos) {
            const QString plain = text.mid(pos, mk.start - pos).trimmed();
            if (!plain.isEmpty()) {
                ChatSegment seg;
                seg.type = ChatSegType::Text;
                seg.body = plain;
                out.append(seg);
            }
        }
        out.append(mk.seg);
        pos = mk.end;
    }
    if (pos < text.size()) {
        const QString plain = text.mid(pos).trimmed();
        if (!plain.isEmpty()) {
            ChatSegment seg;
            seg.type = ChatSegType::Text;
            seg.body = plain;
            out.append(seg);
        }
    }
    if (out.isEmpty() && !text.trimmed().isEmpty()) {
        ChatSegment seg;
        seg.type = ChatSegType::Text;
        seg.body = text;
        out.append(seg);
    }
    return out;
}

class CollapsibleCodeFrame : public QFrame {
public:
    CollapsibleCodeFrame(const QString &title,
                         const QString &code,
                         bool isEdit,
                         QWidget *parent)
        : QFrame(parent)
    {
        setObjectName(isEdit ? QStringLiteral("agentEditFrame") : QStringLiteral("agentCodeFrame"));
        setAttribute(Qt::WA_StyledBackground, true);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Maximum);
        setMinimumWidth(0);
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        m_header = new QWidget(this);
        m_header->setObjectName(QStringLiteral("agentCodeHeader"));
        m_header->setCursor(Qt::PointingHandCursor);
        m_header->setAttribute(Qt::WA_StyledBackground, true);
        auto *headerLayout = new QHBoxLayout(m_header);
        headerLayout->setContentsMargins(8, 6, 8, 6);
        headerLayout->setSpacing(6);

        m_chevron = new QLabel(m_header);
        m_chevron->setObjectName(QStringLiteral("agentCodeChevron"));
        m_chevron->setFixedWidth(12);
        headerLayout->addWidget(m_chevron);

        auto *titleLabel = new QLabel(title, m_header);
        titleLabel->setObjectName(QStringLiteral("agentCodeTitle"));
        QFont titleFont = titleLabel->font();
        titleFont.setPointSize(qMax(8, titleFont.pointSize() - 1));
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        titleLabel->setMinimumWidth(0);
        headerLayout->addWidget(titleLabel, 1);

        const int lines = qMax(1, code.count(QLatin1Char('\n')) + 1);
        auto *meta = new QLabel(QStringLiteral("%1").arg(lines), m_header);
        meta->setObjectName(QStringLiteral("agentCodeMeta"));
        headerLayout->addWidget(meta);
        root->addWidget(m_header);

        m_body = new QPlainTextEdit(this);
        m_body->setObjectName(QStringLiteral("agentCodeBody"));
        m_body->setReadOnly(true);
        m_body->setFrameShape(QFrame::NoFrame);
        m_body->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_body->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_body->setMinimumWidth(0);
        m_body->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_body->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_body->setPlainText(code);
        QFont mono(QStringLiteral("Consolas"));
        if (!mono.exactMatch()) {
            mono = QFont(QStringLiteral("Courier New"));
        }
        mono.setStyleHint(QFont::Monospace);
        mono.setPointSize(10);
        m_body->setFont(mono);
        const int visible = qBound(3, lines, 16);
        m_body->setFixedHeight(m_body->fontMetrics().lineSpacing() * visible + 16);
        root->addWidget(m_body);

        m_expanded = (lines <= 12);
        applyExpanded();

        m_header->installEventFilter(this);
        m_chevron->installEventFilter(this);
        titleLabel->installEventFilter(this);
        meta->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                m_expanded = !m_expanded;
                applyExpanded();
                return true;
            }
        }
        Q_UNUSED(obj);
        return QFrame::eventFilter(obj, event);
    }

private:
    void applyExpanded()
    {
        m_body->setVisible(m_expanded);
        m_chevron->setText(QString(QChar(m_expanded ? 0x25BE : 0x25B8)));
    }

    QLabel *m_chevron { nullptr };
    QPlainTextEdit *m_body { nullptr };
    QWidget *m_header { nullptr };
    bool m_expanded { true };
};

} // namespace

AgentSendButton::AgentSendButton(QWidget *parent)
    : QSvgWidget(parent)
{
    setObjectName(QStringLiteral("agentSendBtn"));
    setCursor(Qt::PointingHandCursor);
    setFixedSize(28, 28);
    setToolTip(tr("Отправить (Enter)"));
    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(1.0);
    setGraphicsEffect(m_opacityEffect);
}

void AgentSendButton::applyTheme(const QString &theme)
{
    const bool dark = (theme == QLatin1String("dark"));
    if (dark) {
        load(recolorSvg(QLatin1String(kSendIconPath), QStringLiteral("#e8e8e8")));
    } else {
        load(QLatin1String(kSendIconPath));
    }
}

void AgentSendButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (isEnabled() && event->button() == Qt::LeftButton && rect().contains(event->pos())) {
        emit clicked();
        event->accept();
        return;
    }
    QSvgWidget::mouseReleaseEvent(event);
}

void AgentSendButton::enterEvent(QEvent *event)
{
    if (isEnabled() && m_opacityEffect) {
        m_opacityEffect->setOpacity(0.82);
    }
    QSvgWidget::enterEvent(event);
}

void AgentSendButton::leaveEvent(QEvent *event)
{
    if (m_opacityEffect) {
        m_opacityEffect->setOpacity(1.0);
    }
    QSvgWidget::leaveEvent(event);
}

AgentPanel::AgentPanel(QWidget *parent)
    : QWidget(parent)
{
    m_client = new LmStudioClient(this);
    connect(m_client, &LmStudioClient::modelsLoaded, this, &AgentPanel::onModelsLoaded);
    connect(m_client, &LmStudioClient::modelsFailed, this, &AgentPanel::onModelsFailed);
    connect(m_client, &LmStudioClient::streamChunk, this, &AgentPanel::onStreamChunk);
    connect(m_client, &LmStudioClient::chatFinished, this, &AgentPanel::onChatFinished);
    connect(m_client, &LmStudioClient::chatFailed, this, &AgentPanel::onChatFailed);

    setupUi();
    m_waitTimer = new QTimer(this);
    m_waitTimer->setInterval(80);
    connect(m_waitTimer, &QTimer::timeout, this, &AgentPanel::onWaitAnimationTick);
    reloadSettings();
}

void AgentPanel::setupUi()
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // --- Header: clear ---
    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("agentHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 8, 8, 4);
    headerLayout->setSpacing(6);
    headerLayout->addStretch();
    m_clearBtn = new QToolButton(header);
    m_clearBtn->setObjectName(QStringLiteral("agentClearBtn"));
    m_clearBtn->setText(tr("Очистить"));
    m_clearBtn->setToolTip(tr("Очистить чат"));
    m_clearBtn->setAutoRaise(true);
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    headerLayout->addWidget(m_clearBtn);
    root->addWidget(header);

    // --- Chat (виджеты, не HTML: иначе точки не анимируются) ---
    m_chatScroll = new QScrollArea(this);
    m_chatScroll->setObjectName(QStringLiteral("agentChatScroll"));
    m_chatScroll->setWidgetResizable(true);
    m_chatScroll->setFrameShape(QFrame::NoFrame);
    m_chatScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_chatScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_chatScroll->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_chatScroll->setMinimumWidth(0);

    m_chatInner = new QWidget(m_chatScroll);
    m_chatInner->setObjectName(QStringLiteral("agentChatInner"));
    m_chatInner->setMinimumWidth(0);
    m_chatInner->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    m_messagesLayout = new QVBoxLayout(m_chatInner);
    m_messagesLayout->setContentsMargins(14, 12, 14, 18);
    m_messagesLayout->setSpacing(14);

    m_emptyHint = new QLabel(tr("Чем помочь?"), m_chatInner);
    m_emptyHint->setObjectName(QStringLiteral("agentEmptyHint"));
    m_emptyHint->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_messagesLayout->addWidget(m_emptyHint);

    m_waitWidget = new QWidget(m_chatInner);
    m_waitWidget->setObjectName(QStringLiteral("agentWaitWidget"));
    auto *waitLay = new QVBoxLayout(m_waitWidget);
    waitLay->setContentsMargins(0, 2, 0, 4);
    waitLay->setSpacing(4);

    auto *dotsHost = new QWidget(m_waitWidget);
    dotsHost->setObjectName(QStringLiteral("agentWaitDotsHost"));
    dotsHost->setFixedSize(32, 14);
    for (int i = 0; i < 3; ++i) {
        m_waitDot[i] = new QLabel(dotsHost);
        m_waitDot[i]->setObjectName(QStringLiteral("agentWaitDot"));
        m_waitDot[i]->setFixedSize(5, 5);
        m_waitDot[i]->setAttribute(Qt::WA_StyledBackground, true);
        m_waitDot[i]->move(2 + i * 9, 8);
    }
    waitLay->addWidget(dotsHost, 0, Qt::AlignLeft);

    m_waitStageLabel = new QLabel(m_waitWidget);
    m_waitStageLabel->setObjectName(QStringLiteral("agentWaitStage"));
    m_waitStageLabel->setWordWrap(true);
    waitLay->addWidget(m_waitStageLabel);
    m_waitWidget->hide();
    m_messagesLayout->addWidget(m_waitWidget);
    m_messagesLayout->addStretch(1);

    m_chatScroll->setWidget(m_chatInner);
    root->addWidget(m_chatScroll, 1);

    // --- Status ---
    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("agentStatusLabel"));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setContentsMargins(14, 2, 14, 2);
    root->addWidget(m_statusLabel);

    // --- Input card (Cursor-style) ---
    auto *inputWrap = new QWidget(this);
    inputWrap->setObjectName(QStringLiteral("agentInputWrap"));
    inputWrap->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Maximum);
    inputWrap->setMinimumWidth(0);
    auto *inputWrapLayout = new QVBoxLayout(inputWrap);
    inputWrapLayout->setContentsMargins(12, 4, 12, 12);
    inputWrapLayout->setSpacing(0);

    m_inputCard = new QFrame(inputWrap);
    m_inputCard->setObjectName(QStringLiteral("agentInputCard"));
    m_inputCard->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Maximum);
    m_inputCard->setMinimumWidth(0);
    auto *cardLayout = new QVBoxLayout(m_inputCard);
    cardLayout->setContentsMargins(12, 10, 12, 8);
    cardLayout->setSpacing(8);

    m_input = new QPlainTextEdit(m_inputCard);
    m_input->setObjectName(QStringLiteral("agentInputEdit"));
    m_input->setPlaceholderText(tr("Спросите что угодно…  Enter — отправить, Shift+Enter — новая строка"));
    m_input->setFixedHeight(72);
    m_input->setFrameShape(QFrame::NoFrame);
    m_input->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_input->setMinimumWidth(0);
    m_input->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    cardLayout->addWidget(m_input);

    auto *toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(kToolbarSpacing);

    // Mode pill (Ask / Code) — меню открывается вверх
    m_modeBtn = new QToolButton(m_inputCard);
    m_modeBtn->setObjectName(QStringLiteral("agentModeBtn"));
    m_modeBtn->setPopupMode(QToolButton::DelayedPopup);
    m_modeBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_modeBtn->setAutoRaise(false);
    m_modeBtn->setAttribute(Qt::WA_StyledBackground, true);
    m_modeBtn->setCursor(Qt::PointingHandCursor);
    m_modeBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_modeMenu = new QMenu(this);
    auto *modeGroup = new QActionGroup(m_modeMenu);
    modeGroup->setExclusive(true);
    m_askAction = m_modeMenu->addAction(tr("Ask"));
    m_askAction->setCheckable(true);
    m_askAction->setData(static_cast<int>(AgentPrompts::Mode::Ask));
    modeGroup->addAction(m_askAction);
    m_codeAction = m_modeMenu->addAction(tr("Code"));
    m_codeAction->setCheckable(true);
    m_codeAction->setData(static_cast<int>(AgentPrompts::Mode::Code));
    modeGroup->addAction(m_codeAction);
    connect(m_modeBtn, &QToolButton::clicked, this, &AgentPanel::showModeMenu);
    connect(m_modeMenu, &QMenu::triggered, this, &AgentPanel::onModeMenuTriggered);
    toolbar->addWidget(m_modeBtn);

    // Hidden provider combo (synced from settings)
    m_providerCombo = new QComboBox(m_inputCard);
    m_providerCombo->addItem(tr("LM Studio"), QStringLiteral("lmstudio"));
    m_providerCombo->addItem(QStringLiteral("OpenRouter"), QStringLiteral("openrouter"));
    m_providerCombo->hide();

    // OpenRouter: выбор API-ключа
    m_apiKeyPicker = new AgentApiKeyPicker(m_inputCard);
    m_apiKeyPicker->setObjectName(QStringLiteral("agentApiKeyPicker"));
    m_apiKeyPicker->setCursor(Qt::PointingHandCursor);
    if (auto *picker = static_cast<AgentApiKeyPicker *>(m_apiKeyPicker)) {
        picker->setIconWidth(kKeyIconOnlyWidth);
        picker->setComfortableWidth(kKeyIconOnlyWidth);
    }

    auto *apiKeyLayout = new QHBoxLayout(m_apiKeyPicker);
    apiKeyLayout->setContentsMargins(0, 0, 0, 0);
    apiKeyLayout->setSpacing(0);

    m_apiKeyBtn = new QToolButton(m_apiKeyPicker);
    m_apiKeyBtn->setObjectName(QStringLiteral("agentApiKeyBtn"));
    m_apiKeyBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_apiKeyBtn->setIconSize(QSize(kKeyIconSize, kKeyIconSize));
    m_apiKeyBtn->setAutoRaise(true);
    m_apiKeyBtn->setCursor(Qt::PointingHandCursor);
    m_apiKeyBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_apiKeyBtn->setMinimumWidth(kKeyIconOnlyWidth);
    apiKeyLayout->addWidget(m_apiKeyBtn, 1);

    m_apiKeyMenu = new QMenu(this);
    m_apiKeyActionGroup = new QActionGroup(m_apiKeyMenu);
    m_apiKeyActionGroup->setExclusive(true);
    connect(m_apiKeyBtn, &QToolButton::clicked, this, &AgentPanel::showApiKeyMenu);
    connect(m_apiKeyMenu, &QMenu::triggered, this, &AgentPanel::onApiKeyMenuTriggered);
    m_apiKeyPicker->installEventFilter(this);
    toolbar->addWidget(m_apiKeyPicker);

    // Выбор модели: название + обновление в одном блоке, список выпадает из него
    m_modelPicker = new QFrame(m_inputCard);
    m_modelPicker->setObjectName(QStringLiteral("agentModelPicker"));
    m_modelPicker->setCursor(Qt::PointingHandCursor);
    m_modelPicker->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_modelPicker->setMinimumWidth(kModelPickerMinWidth);

    auto *pickerLayout = new QHBoxLayout(m_modelPicker);
    pickerLayout->setContentsMargins(0, 0, 0, 0);
    pickerLayout->setSpacing(0);

    m_modelBtn = new QToolButton(m_modelPicker);
    m_modelBtn->setObjectName(QStringLiteral("agentModelBtn"));
    m_modelBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_modelBtn->setAutoRaise(true);
    m_modelBtn->setCursor(Qt::PointingHandCursor);
    m_modelBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_modelBtn->setMinimumWidth(80);
    pickerLayout->addWidget(m_modelBtn, 1);

    m_refreshModelsBtn = new QToolButton(m_modelPicker);
    m_refreshModelsBtn->setObjectName(QStringLiteral("agentModelRefreshBtn"));
    m_refreshModelsBtn->setText(QString(QChar(0x21BB)));
    m_refreshModelsBtn->setToolTip(tr("Обновить список моделей"));
    m_refreshModelsBtn->setAutoRaise(true);
    m_refreshModelsBtn->setCursor(Qt::PointingHandCursor);
    m_refreshModelsBtn->setFixedSize(26, 26);
    pickerLayout->addWidget(m_refreshModelsBtn);

    m_modelMenu = new QMenu(this);
    m_modelActionGroup = new QActionGroup(m_modelMenu);
    m_modelActionGroup->setExclusive(true);

    connect(m_modelBtn, &QToolButton::clicked, this, &AgentPanel::showModelMenu);
    connect(m_modelMenu, &QMenu::triggered, this, &AgentPanel::onModelMenuTriggered);
    toolbar->addWidget(m_modelPicker, 1);

    toolbar->addStretch();

    m_stopBtn = new QToolButton(m_inputCard);
    m_stopBtn->setObjectName(QStringLiteral("agentStopBtn"));
    m_stopBtn->setText(tr("Стоп"));
    m_stopBtn->setToolTip(tr("Остановить генерацию"));
    m_stopBtn->setAutoRaise(true);
    m_stopBtn->setCursor(Qt::PointingHandCursor);
    m_stopBtn->setEnabled(false);
    m_stopBtn->setVisible(false);
    toolbar->addWidget(m_stopBtn);

    m_sendBtn = new AgentSendButton(m_inputCard);
    toolbar->addWidget(m_sendBtn);

    cardLayout->addLayout(toolbar);
    inputWrapLayout->addWidget(m_inputCard);
    root->addWidget(inputWrap);

    connect(m_providerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AgentPanel::onProviderChanged);
    connect(m_refreshModelsBtn, &QToolButton::clicked, this, &AgentPanel::refreshModels);
    connect(m_sendBtn, &AgentSendButton::clicked, this, &AgentPanel::onSend);
    connect(m_stopBtn, &QToolButton::clicked, this, &AgentPanel::onStop);
    connect(m_clearBtn, &QToolButton::clicked, this, &AgentPanel::clearChat);

    m_input->installEventFilter(this);
    updateModelButtonText();
    updateModeButton();
    updateToolbarMinimumWidth();
}

void AgentPanel::applyProviderSettings()
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));
    m_provider = settings.value(QStringLiteral("provider"), QLatin1String(kProviderLmStudio)).toString();
    const QString baseUrl = settings.value(QStringLiteral("lmstudioBaseUrl"),
                                           QStringLiteral("http://127.0.0.1:1234/v1")).toString();
    m_selectedOpenRouterKeyName = settings.value(QStringLiteral("selectedOpenRouterKeyName")).toString();
    settings.endGroup();

    const int providerIndex = m_providerCombo->findData(m_provider);
    QSignalBlocker blocker(m_providerCombo);
    m_providerCombo->setCurrentIndex(providerIndex >= 0 ? providerIndex : 0);

    if (m_provider == QLatin1String(kProviderOpenRouter)) {
        m_client->setBaseUrl(QLatin1String(kOpenRouterBaseUrl));
        m_client->setApiKey(AgentOpenRouterKeys::keyForName(m_selectedOpenRouterKeyName));
    } else {
        m_client->setBaseUrl(baseUrl);
        m_client->clearApiKey();
    }

    updateProviderUi();
    rebuildApiKeyMenu();
}

void AgentPanel::updateProviderUi()
{
    const bool openRouter = (m_provider == QLatin1String(kProviderOpenRouter));
    if (m_apiKeyPicker) {
        m_apiKeyPicker->setVisible(openRouter);
    }
    updateApiKeyButtonText();
    updateToolbarMinimumWidth();
}

void AgentPanel::applyApiKeyIcon()
{
    if (!m_apiKeyBtn) {
        return;
    }
    m_apiKeyBtn->setIcon(themedSvgIcon(QLatin1String(kKeyIconPath), m_theme, kKeyIconSize));
}

void AgentPanel::updateApiKeyButtonText()
{
    if (!m_apiKeyBtn || !m_apiKeyPicker) {
        return;
    }

    applyApiKeyIcon();

    QString label;
    if (m_selectedOpenRouterKeyName.isEmpty()) {
        label = tr("API ключ");
    } else {
        label = m_selectedOpenRouterKeyName;
    }
    m_apiKeyBtn->setToolTip(label);

    const QFontMetrics fm(m_apiKeyBtn->font());
    const int textWidth = fm.horizontalAdvance(label);
    const int withTextWidth = kKeyIconOnlyWidth + 8 + textWidth;
    if (auto *picker = static_cast<AgentApiKeyPicker *>(m_apiKeyPicker)) {
        picker->setIconWidth(kKeyIconOnlyWidth);
        picker->setComfortableWidth(withTextWidth);
    } else {
        m_apiKeyPicker->setMinimumWidth(kKeyIconOnlyWidth);
        m_apiKeyPicker->setMaximumWidth(qMax(kKeyIconOnlyWidth, withTextWidth));
    }
    m_apiKeyBtn->setMinimumWidth(kKeyIconOnlyWidth);

    const int available = m_apiKeyPicker->width() > 0 ? m_apiKeyPicker->width() : withTextWidth;
    const bool showText = available >= withTextWidth - 4;
    if (showText) {
        m_apiKeyBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_apiKeyBtn->setText(label);
    } else {
        m_apiKeyBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        m_apiKeyBtn->setText(QString());
    }
}

void AgentPanel::rebuildApiKeyMenu()
{
    if (!m_apiKeyMenu || !m_apiKeyActionGroup) {
        return;
    }

    m_apiKeyMenu->clear();
    const QVector<OpenRouterKeyEntry> keys = AgentOpenRouterKeys::load();
    if (keys.isEmpty()) {
        updateApiKeyButtonText();
        return;
    }

    bool hasSelection = false;
    for (const OpenRouterKeyEntry &entry : keys) {
        auto *action = m_apiKeyMenu->addAction(entry.name);
        action->setCheckable(true);
        action->setData(entry.name);
        m_apiKeyActionGroup->addAction(action);
        if (entry.name.compare(m_selectedOpenRouterKeyName, Qt::CaseInsensitive) == 0) {
            action->setChecked(true);
            m_selectedOpenRouterKeyName = entry.name;
            hasSelection = true;
        }
    }

    if (!hasSelection) {
        m_selectedOpenRouterKeyName = keys.first().name;
        if (QAction *firstAction = m_apiKeyMenu->actions().value(0)) {
            firstAction->setChecked(true);
        }
        saveSelectedOpenRouterKey();
        m_client->setApiKey(AgentOpenRouterKeys::keyForName(m_selectedOpenRouterKeyName));
    }

    updateApiKeyButtonText();
}

void AgentPanel::saveSelectedOpenRouterKey()
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));
    settings.setValue(QStringLiteral("selectedOpenRouterKeyName"), m_selectedOpenRouterKeyName);
    settings.endGroup();
}

void AgentPanel::showApiKeyMenu()
{
    if (!m_apiKeyPicker || !m_apiKeyMenu || m_apiKeyMenu->isEmpty()) {
        m_statusLabel->setText(tr("Добавьте API-ключ OpenRouter в настройках агента."));
        return;
    }

    m_apiKeyMenu->adjustSize();
    const QSize menuSize = m_apiKeyMenu->sizeHint();
    const QPoint bottomLeft = m_apiKeyPicker->mapToGlobal(QPoint(0, m_apiKeyPicker->height()));
    m_apiKeyMenu->popup(QPoint(bottomLeft.x(), bottomLeft.y() - menuSize.height()));
}

void AgentPanel::onApiKeyMenuTriggered(QAction *action)
{
    if (!action) {
        return;
    }

    m_selectedOpenRouterKeyName = action->data().toString();
    saveSelectedOpenRouterKey();
    m_client->setApiKey(AgentOpenRouterKeys::keyForName(m_selectedOpenRouterKeyName));
    updateApiKeyButtonText();
    refreshModels();
}

bool AgentPanel::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_apiKeyPicker && event->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            showApiKeyMenu();
            return true;
        }
    }
    if (obj == m_input && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)) {
            if (ke->modifiers() & Qt::ShiftModifier) {
                return false; // новая строка
            }
            onSend();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void AgentPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateApiKeyButtonText();
    updateModelButtonText();
}

int AgentPanel::toolbarMinimumWidth() const
{
    int count = 0;
    int w = 0;
    auto add = [&](int part) {
        if (part <= 0) {
            return;
        }
        w += part;
        ++count;
    };

    if (m_modeBtn) {
        add(qMax(m_modeBtn->minimumWidth(), m_modeBtn->minimumSizeHint().width()));
    }
    if (m_apiKeyPicker && !m_apiKeyPicker->isHidden()) {
        add(qMax(m_apiKeyPicker->minimumWidth(), kKeyIconOnlyWidth));
    }
    if (m_modelPicker) {
        add(qMax(m_modelPicker->minimumWidth(), kModelPickerMinWidth));
    }
    add(kSendButtonWidth);
    if (count > 1) {
        w += kToolbarSpacing * (count - 1);
    }
    w += kInputHorizontalMargins + kCardHorizontalMargins;
    return w;
}

void AgentPanel::updateToolbarMinimumWidth()
{
    const int minW = toolbarMinimumWidth();
    setMinimumWidth(minW);
    if (auto *dock = qobject_cast<QDockWidget *>(parentWidget())) {
        dock->setMinimumWidth(minW);
    }
    updateGeometry();
}

void AgentPanel::setTheme(const QString &theme)
{
    m_theme = theme;
    applyThemeStyles();
}

QSize AgentPanel::sizeHint() const
{
    return QSize(qMax(260, toolbarMinimumWidth()), 480);
}

QSize AgentPanel::minimumSizeHint() const
{
    return QSize(toolbarMinimumWidth(), 200);
}

void AgentPanel::setContextProvider(ContextProvider provider)
{
    m_contextProvider = std::move(provider);
}

void AgentPanel::setCodeApplier(CodeApplier applier)
{
    m_codeApplier = std::move(applier);
}

void AgentPanel::updateModeButton()
{
    if (!m_modeBtn) {
        return;
    }
    // U+25BE маленький треугольник вниз
    const QString arrow = QString(QChar(0x25BE));
    if (m_mode == AgentPrompts::Mode::Ask) {
        m_modeBtn->setText(tr("Ask") + QLatin1Char(' ') + arrow);
        if (m_askAction) {
            m_askAction->setChecked(true);
        }
    } else {
        m_modeBtn->setText(tr("Code") + QLatin1Char(' ') + arrow);
        if (m_codeAction) {
            m_codeAction->setChecked(true);
        }
    }
    applyModeButtonStyle();

    const QString current = m_modeBtn->text();
    m_modeBtn->setText(tr("Ask") + QLatin1Char(' ') + arrow);
    const int askW = m_modeBtn->sizeHint().width();
    m_modeBtn->setText(tr("Code") + QLatin1Char(' ') + arrow);
    const int codeW = m_modeBtn->sizeHint().width();
    m_modeBtn->setText(current);
    m_modeBtn->setMinimumWidth(qMax(askW, codeW));
    updateToolbarMinimumWidth();
}

void AgentPanel::applyModeButtonStyle()
{
    if (!m_modeBtn) {
        return;
    }

    const bool dark = (m_theme == QLatin1String("dark"));
    const bool ask = (m_mode == AgentPrompts::Mode::Ask);
    const QString bg = ask ? (dark ? QStringLiteral("#1e3a2f") : QStringLiteral("#e6f6ee"))
                           : (dark ? QStringLiteral("#2a3340") : QStringLiteral("#e8eef8"));
    const QString fg = ask ? (dark ? QStringLiteral("#7dcea0") : QStringLiteral("#1b7a4a"))
                           : (dark ? QStringLiteral("#8ab4f8") : QStringLiteral("#1a56b0"));
    const QString hoverBg = ask ? (dark ? QStringLiteral("#254a3a") : QStringLiteral("#d8f0e4"))
                                : (dark ? QStringLiteral("#343f4f") : QStringLiteral("#dce6f4"));

    m_modeBtn->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: none;"
        "  border-radius: 14px;"
        "  padding: 5px 12px;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "}"
        "QToolButton:hover {"
        "  background-color: %3;"
        "  border-radius: 14px;"
        "}"
        "QToolButton:pressed {"
        "  background-color: %3;"
        "  border-radius: 14px;"
        "}"
        "QToolButton:disabled {"
        "  opacity: 0.6;"
        "  border-radius: 14px;"
        "}"
    ).arg(bg, fg, hoverBg));
}

void AgentPanel::showModeMenu()
{
    if (!m_modeBtn || !m_modeMenu) {
        return;
    }
    m_modeMenu->adjustSize();
    const QSize menuSize = m_modeMenu->sizeHint();
    const QPoint topLeft = m_modeBtn->mapToGlobal(QPoint(0, 0));
    m_modeMenu->popup(QPoint(topLeft.x(), topLeft.y() - menuSize.height()));
}

void AgentPanel::reloadSettings()
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));
    const QString selectedModel = settings.value(QStringLiteral("selectedModel")).toString();
    const QString mode = settings.value(QStringLiteral("mode"), QStringLiteral("ask")).toString();
    settings.endGroup();

    applyProviderSettings();

    m_mode = (mode == QLatin1String("code")) ? AgentPrompts::Mode::Code : AgentPrompts::Mode::Ask;
    updateModeButton();

    m_pendingModelId = selectedModel;
    refreshModels();
    applyThemeStyles();
}

void AgentPanel::showModelMenu()
{
    if (!m_modelPicker || !m_modelMenu || m_modelMenu->isEmpty()) {
        return;
    }
    m_modelMenu->adjustSize();
    const QSize menuSize = m_modelMenu->sizeHint();
    const QPoint bottomLeft = m_modelPicker->mapToGlobal(QPoint(0, m_modelPicker->height()));
    m_modelMenu->popup(QPoint(bottomLeft.x(), bottomLeft.y() - menuSize.height()));
}

QString AgentPanel::shortModelName(const QString &id) const
{
    if (id.isEmpty()) {
        return QString();
    }
    QString shortName = id;
    const int slash = shortName.lastIndexOf(QLatin1Char('/'));
    if (slash >= 0 && slash + 1 < shortName.size()) {
        shortName = shortName.mid(slash + 1);
    }
    return shortName;
}

void AgentPanel::updateModelButtonText()
{
    if (!m_modelBtn) {
        return;
    }
    const QString arrow = QString(QChar(0x25BE));
    QString label;
    if (m_selectedModelId.isEmpty()) {
        label = tr("Модель");
    } else {
        label = shortModelName(m_selectedModelId);
        const QFontMetrics fm(m_modelBtn->font());
        int avail = m_modelBtn->width() - 12;
        if (avail < 24) {
            avail = 24;
        }
        label = fm.elidedText(label, Qt::ElideRight, avail);
    }
    m_modelBtn->setText(label + QLatin1Char(' ') + arrow);
    if (!m_selectedModelId.isEmpty()) {
        m_modelBtn->setToolTip(m_selectedModelId);
    } else {
        m_modelBtn->setToolTip(tr("Выберите модель"));
    }
}

void AgentPanel::rebuildModelMenu(const QStringList &models)
{
    if (!m_modelMenu || !m_modelActionGroup) {
        return;
    }
    m_modelMenu->clear();

    for (const QString &id : models) {
        auto *action = m_modelMenu->addAction(shortModelName(id));
        action->setCheckable(true);
        action->setData(id);
        action->setToolTip(id);
        m_modelActionGroup->addAction(action);
        if (id == m_selectedModelId) {
            action->setChecked(true);
        }
    }
    updateModelButtonText();
}

void AgentPanel::refreshModels()
{
    if (m_provider == QLatin1String(kProviderOpenRouter)) {
        rebuildApiKeyMenu();
        if (m_client->apiKey().isEmpty()) {
            m_statusLabel->setText(tr("Выберите или добавьте API-ключ OpenRouter в настройках."));
            if (m_modelBtn) {
                m_modelBtn->setText(tr("Модель"));
            }
            if (m_modelMenu) {
                m_modelMenu->clear();
            }
            if (m_refreshModelsBtn) {
                m_refreshModelsBtn->setEnabled(true);
            }
            return;
        }

        const QStringList saved = AgentOpenRouterKeys::loadModels();
        onModelsLoaded(saved);
        if (saved.isEmpty()) {
            m_statusLabel->setText(tr("Добавьте модели в Настройки → Агент."));
        }
        return;
    }

    m_statusLabel->setText(tr("Загрузка моделей…"));
    m_refreshModelsBtn->setEnabled(false);
    m_client->refreshModels();
}

void AgentPanel::onProviderChanged()
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));
    settings.setValue(QStringLiteral("provider"), m_providerCombo->currentData().toString());
    settings.endGroup();
    applyProviderSettings();
    refreshModels();
}

void AgentPanel::onModeMenuTriggered(QAction *action)
{
    if (!action) {
        return;
    }
    m_mode = static_cast<AgentPrompts::Mode>(action->data().toInt());
    updateModeButton();
    saveMode();
    m_statusLabel->setText(m_mode == AgentPrompts::Mode::Ask
                               ? tr("Ask — ответы и планы без правок файлов")
                               : tr("Code — правки в открытой вкладке"));
}

void AgentPanel::saveMode()
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));
    settings.setValue(QStringLiteral("mode"),
                      m_mode == AgentPrompts::Mode::Code ? QStringLiteral("code")
                                                         : QStringLiteral("ask"));
    settings.endGroup();
}

void AgentPanel::onModelMenuTriggered(QAction *action)
{
    if (!action) {
        return;
    }
    m_selectedModelId = action->data().toString();
    updateModelButtonText();
    saveSelectedModel();
}

void AgentPanel::saveSelectedModel()
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));
    settings.setValue(QStringLiteral("selectedModel"), m_selectedModelId);
    settings.endGroup();
}

void AgentPanel::onModelsLoaded(const QStringList &models)
{
    m_refreshModelsBtn->setEnabled(true);
    const QString pending = m_pendingModelId;
    const QString previous = m_selectedModelId;

    if (models.isEmpty()) {
        m_selectedModelId.clear();
        rebuildModelMenu(QStringList());
        m_modelBtn->setText(tr("Нет моделей"));
        m_statusLabel->setText(m_provider == QLatin1String(kProviderOpenRouter)
                                   ? tr("Добавьте модели в Настройки → Агент")
                                   : tr("Загрузите модель в LM Studio"));
        return;
    }

    QString select = pending.isEmpty() ? previous : pending;
    if (!select.isEmpty() && !models.contains(select)) {
        select = models.first();
    } else if (select.isEmpty()) {
        select = models.first();
    }
    m_selectedModelId = select;
    m_pendingModelId.clear();

    rebuildModelMenu(models);
    saveSelectedModel();
    m_statusLabel->clear();
}

void AgentPanel::onModelsFailed(const QString &error)
{
    m_refreshModelsBtn->setEnabled(true);
    m_selectedModelId.clear();
    m_modelMenu->clear();
    m_modelBtn->setText(m_provider == QLatin1String(kProviderOpenRouter)
                            ? tr("OpenRouter…")
                            : tr("LM Studio…"));
    m_statusLabel->setText(error);
}

void AgentPanel::clearChat()
{
    if (m_client->isBusy()) {
        m_client->cancel();
        updateBusyState(false);
    }
    m_historyUser.clear();
    m_historyAssistant.clear();
    m_pendingAssistantHtml.clear();
    m_assistantStreaming = false;
    stopWaitAnimation();
    m_statusLabel->clear();
    rebuildChat();
}

void AgentPanel::updateBusyState(bool busy)
{
    if (m_sendBtn) {
        m_sendBtn->setVisible(!busy);
        m_sendBtn->setEnabled(!busy);
    }
    m_stopBtn->setVisible(busy);
    m_stopBtn->setEnabled(busy);
    m_input->setReadOnly(busy);
    m_modeBtn->setEnabled(!busy);
    if (m_modelPicker) {
        m_modelPicker->setEnabled(!busy);
    }
    if (m_apiKeyPicker) {
        m_apiKeyPicker->setEnabled(!busy);
    }
}

void AgentPanel::appendUserMessage(const QString &text)
{
    Q_UNUSED(text);
    rebuildChat();
}

void AgentPanel::beginAssistantMessage()
{
    m_assistantStreaming = true;
    m_pendingAssistantHtml.clear();
    startWaitAnimation(m_provider == QLatin1String(kProviderOpenRouter)
                           ? tr("Подключение к OpenRouter")
                           : tr("Подключение к LM Studio"));
    rebuildChat();
}

void AgentPanel::appendAssistantDelta(const QString &delta)
{
    const bool firstChunk = m_pendingAssistantHtml.isEmpty();
    m_pendingAssistantHtml += delta;
    if (firstChunk) {
        setWaitStage(tr("Получение ответа"));
    }
    if (m_streamingLabel) {
        m_streamingLabel->setText(m_pendingAssistantHtml);
        m_streamingLabel->setVisible(true);
        scrollChatToBottom();
    } else {
        rebuildChat();
    }
}

void AgentPanel::finishAssistantMessage(const QString &fullText)
{
    stopWaitAnimation();
    m_assistantStreaming = false;
    m_pendingAssistantHtml.clear();
    m_historyAssistant.append(fullText);
    rebuildChat();
}

void AgentPanel::startWaitAnimation(const QString &stage)
{
    m_waitStage = stage;
    m_waitFrame = 0;
    if (m_waitWidget) {
        m_waitWidget->show();
    }
    if (m_waitStageLabel) {
        m_waitStageLabel->setText(stage);
    }
    updateWaitDots();
    if (m_waitTimer && !m_waitTimer->isActive()) {
        m_waitTimer->start();
    }
}

void AgentPanel::stopWaitAnimation()
{
    if (m_waitTimer) {
        m_waitTimer->stop();
    }
    m_waitFrame = 0;
    m_waitStage.clear();
    if (m_waitWidget) {
        m_waitWidget->hide();
    }
}

void AgentPanel::setWaitStage(const QString &stage)
{
    m_waitStage = stage;
    if (m_waitStageLabel) {
        m_waitStageLabel->setText(stage);
    }
}

void AgentPanel::onWaitAnimationTick()
{
    m_waitFrame = (m_waitFrame + 1) % 8;
    updateWaitDots();
}

void AgentPanel::updateWaitDots()
{
    static const int kBounce[] = { 0, 1, 2, 4, 5, 4, 2, 1 };
    for (int i = 0; i < 3; ++i) {
        if (!m_waitDot[i]) {
            continue;
        }
        const int lift = kBounce[(m_waitFrame + (2 - i) * 2) % 8];
        m_waitDot[i]->move(2 + i * 9, 8 - lift);
    }
}

void AgentPanel::scrollChatToBottom()
{
    if (!m_chatScroll) {
        return;
    }
    QScrollBar *bar = m_chatScroll->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void AgentPanel::clearMessageWidgets()
{
    m_streamingLabel = nullptr;
    if (!m_messagesLayout) {
        return;
    }
    QVector<QWidget *> toDelete;
    for (int i = 0; i < m_messagesLayout->count(); ++i) {
        QLayoutItem *item = m_messagesLayout->itemAt(i);
        QWidget *w = item ? item->widget() : nullptr;
        if (w && w != m_emptyHint && w != m_waitWidget) {
            toDelete.append(w);
        }
    }
    for (QWidget *w : toDelete) {
        m_messagesLayout->removeWidget(w);
        delete w;
    }
}

QWidget *AgentPanel::createUserBubble(const QString &text)
{
    auto *wrap = new QWidget(m_chatInner);
    wrap->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Maximum);
    wrap->setMinimumWidth(0);
    auto *lay = new QVBoxLayout(wrap);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);

    auto *role = new QLabel(tr("Вы"), wrap);
    role->setObjectName(QStringLiteral("agentRoleLabel"));
    lay->addWidget(role);

    auto *bubble = new QLabel(text, wrap);
    bubble->setObjectName(QStringLiteral("agentUserBubble"));
    bubble->setTextFormat(Qt::PlainText);
    bubble->setWordWrap(true);
    bubble->setMinimumWidth(0);
    bubble->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    bubble->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bubble->setAttribute(Qt::WA_StyledBackground, true);
    lay->addWidget(bubble);
    return wrap;
}

QWidget *AgentPanel::createAssistantMessage(const QString &text, bool streaming)
{
    auto *wrap = new QWidget(m_chatInner);
    wrap->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Maximum);
    wrap->setMinimumWidth(0);
    auto *lay = new QVBoxLayout(wrap);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    auto *role = new QLabel(tr("Агент"), wrap);
    role->setObjectName(QStringLiteral("agentRoleLabel"));
    lay->addWidget(role);

    if (streaming) {
        auto *body = new QLabel(wrap);
        body->setObjectName(QStringLiteral("agentAssistantText"));
        body->setTextFormat(Qt::PlainText);
        body->setWordWrap(true);
        body->setMinimumWidth(0);
        body->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        body->setTextInteractionFlags(Qt::TextSelectableByMouse);
        body->setText(text);
        body->setVisible(!text.isEmpty());
        m_streamingLabel = body;
        lay->addWidget(body);
        return wrap;
    }

    const QVector<ChatSegment> segs = parseChatSegments(text);
    for (const ChatSegment &seg : segs) {
        if (seg.type == ChatSegType::Text) {
            auto *body = new QLabel(seg.body, wrap);
            body->setObjectName(QStringLiteral("agentAssistantText"));
            body->setTextFormat(Qt::PlainText);
            body->setWordWrap(true);
            body->setMinimumWidth(0);
            body->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            body->setTextInteractionFlags(Qt::TextSelectableByMouse);
            lay->addWidget(body);
        } else {
            lay->addWidget(new CollapsibleCodeFrame(
                seg.title, seg.body, seg.type == ChatSegType::Edit, wrap));
        }
    }
    return wrap;
}

void AgentPanel::rebuildChat()
{
    if (!m_messagesLayout) {
        return;
    }
    clearMessageWidgets();
    if (m_emptyHint) {
        m_emptyHint->setVisible(m_historyUser.isEmpty());
    }

    auto insertBeforeWait = [this](QWidget *w) {
        const int idx = m_messagesLayout->indexOf(m_waitWidget);
        m_messagesLayout->insertWidget(idx < 0 ? m_messagesLayout->count() : idx, w);
    };

    const int pairs = qMin(m_historyUser.size(), m_historyAssistant.size());
    for (int i = 0; i < pairs; ++i) {
        insertBeforeWait(createUserBubble(m_historyUser.at(i)));
        insertBeforeWait(createAssistantMessage(m_historyAssistant.at(i), false));
    }
    if (m_historyUser.size() > m_historyAssistant.size()) {
        insertBeforeWait(createUserBubble(m_historyUser.last()));
        if (m_assistantStreaming) {
            insertBeforeWait(createAssistantMessage(m_pendingAssistantHtml, true));
        }
    }

    if (m_assistantStreaming && m_waitWidget) {
        m_waitWidget->show();
        if (m_waitStageLabel) {
            m_waitStageLabel->setText(m_waitStage);
        }
        updateWaitDots();
    } else if (m_waitWidget) {
        m_waitWidget->hide();
    }

    scrollChatToBottom();
    QTimer::singleShot(0, this, [this]() { scrollChatToBottom(); });
}

QJsonArray AgentPanel::buildMessages(const QString &userText) const
{
    QJsonArray messages;
    QJsonObject systemMsg;
    systemMsg.insert(QStringLiteral("role"), QStringLiteral("system"));
    systemMsg.insert(QStringLiteral("content"), AgentPrompts::systemPromptForMode(m_mode));
    messages.append(systemMsg);

    if (m_contextProvider) {
        const QString ctx = m_contextProvider();
        if (!ctx.trimmed().isEmpty()) {
            QJsonObject ctxMsg;
            ctxMsg.insert(QStringLiteral("role"), QStringLiteral("system"));
            ctxMsg.insert(QStringLiteral("content"),
                          QStringLiteral("Current IDE context:\n%1").arg(ctx));
            messages.append(ctxMsg);
        }
    }

    const int pairs = qMin(m_historyUser.size(), m_historyAssistant.size());
    for (int i = 0; i < pairs; ++i) {
        QJsonObject u;
        u.insert(QStringLiteral("role"), QStringLiteral("user"));
        u.insert(QStringLiteral("content"), m_historyUser.at(i));
        messages.append(u);
        QJsonObject a;
        a.insert(QStringLiteral("role"), QStringLiteral("assistant"));
        a.insert(QStringLiteral("content"), m_historyAssistant.at(i));
        messages.append(a);
    }

    QJsonObject current;
    current.insert(QStringLiteral("role"), QStringLiteral("user"));
    current.insert(QStringLiteral("content"), userText);
    messages.append(current);
    return messages;
}

void AgentPanel::onSend()
{
    if (m_client->isBusy()) {
        return;
    }

    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty()) {
        return;
    }

    const QString model = m_selectedModelId;
    if (model.isEmpty()) {
        m_statusLabel->setText(tr("Выберите модель"));
        return;
    }

    const QJsonArray messages = buildMessages(text);

    m_input->clear();
    m_historyUser.append(text);
    appendUserMessage(text);

    beginAssistantMessage();
    updateBusyState(true);
    m_statusLabel->setText(tr("Запрос отправлен"));
    QTimer::singleShot(600, this, [this]() {
        if (m_assistantStreaming && m_pendingAssistantHtml.isEmpty()) {
            setWaitStage(tr("Ожидание ответа модели"));
            m_statusLabel->setText(tr("Ожидание ответа…"));
        }
    });
    m_client->chatCompletion(model, messages, true);
}

void AgentPanel::onStop()
{
    m_client->cancel();
    updateBusyState(false);
    if (m_assistantStreaming) {
        const QString partial = m_pendingAssistantHtml.isEmpty()
                                    ? tr("(остановлено)")
                                    : m_pendingAssistantHtml + tr("\n\n(остановлено)");
        finishAssistantMessage(partial);
    }
    m_statusLabel->setText(tr("Остановлено"));
}

void AgentPanel::onStreamChunk(const QString &delta)
{
    appendAssistantDelta(delta);
    m_statusLabel->setText(tr("Получение ответа…"));
}

void AgentPanel::onChatFinished(const QString &fullText)
{
    updateBusyState(false);
    finishAssistantMessage(fullText);

    if (m_mode == AgentPrompts::Mode::Code) {
        QString status;
        if (tryApplyCodeFromResponse(fullText, &status)) {
            m_statusLabel->setText(status.isEmpty()
                                       ? tr("Изменения применены")
                                       : status);
            return;
        }
        if (!status.isEmpty()) {
            m_statusLabel->setText(status);
            return;
        }
    }
    m_statusLabel->clear();
}

bool AgentPanel::tryApplyCodeFromResponse(const QString &fullText, QString *statusMessage)
{
    if (!m_codeApplier) {
        if (statusMessage) {
            *statusMessage = tr("Нет обработчика вставки кода.");
        }
        return false;
    }
    return m_codeApplier(fullText, statusMessage);
}

void AgentPanel::onChatFailed(const QString &error)
{
    updateBusyState(false);
    stopWaitAnimation();
    m_assistantStreaming = false;
    if (m_historyUser.size() > m_historyAssistant.size()) {
        finishAssistantMessage(tr("Ошибка: %1").arg(error));
    }
    m_statusLabel->setText(error);
}

void AgentPanel::applyThemeStyles()
{
    const bool dark = (m_theme == QLatin1String("dark"));
    const QString bg = dark ? QStringLiteral("#1e1e1e") : QStringLiteral("#ffffff");
    const QString fg = dark ? QStringLiteral("#e8e8e8") : QStringLiteral("#1a1a1a");
    const QString muted = dark ? QStringLiteral("#9a9a9a") : QStringLiteral("#6b6b6b");
    const QString cardBg = dark ? QStringLiteral("#252526") : QStringLiteral("#ffffff");
    const QString border = dark ? QStringLiteral("#3c3c3c") : QStringLiteral("#d4d4d4");
    const QString inputBg = dark ? QStringLiteral("#1e1e1e") : QStringLiteral("#fafafa");
    const QString hover = dark ? QStringLiteral("#333333") : QStringLiteral("#f3f3f3");
    const QString userBg = dark ? QStringLiteral("#2f2f2f") : QStringLiteral("#f0f0f0");

    setStyleSheet(QStringLiteral(
        "AgentPanel {"
        "  background: %1;"
        "}"
        "#agentHeader {"
        "  background: %1;"
        "}"
        "#agentClearBtn {"
        "  color: %2; border: none; padding: 4px 8px; font-size: 12px;"
        "}"
        "#agentClearBtn:hover { background: %3; border-radius: 6px; }"
        "QScrollArea#agentChatScroll {"
        "  background: %1; border: none;"
        "}"
        "QScrollArea#agentChatScroll > QWidget {"
        "  background: %1;"
        "}"
        "#agentChatInner {"
        "  background: %1;"
        "}"
        "#agentEmptyHint {"
        "  color: %2; font-size: 14px; padding: 48px 12px;"
        "}"
        "#agentRoleLabel {"
        "  color: %2; font-size: 11px;"
        "}"
        "#agentUserBubble {"
        "  background: %7; color: %6;"
        "  border-radius: 12px; padding: 10px 12px; font-size: 13px;"
        "}"
        "#agentAssistantText {"
        "  color: %6; font-size: 13px;"
        "}"
        "#agentCodeFrame, #agentEditFrame {"
        "  background: %4; border: 1px solid %5; border-radius: 8px;"
        "}"
        "#agentEditFrame {"
        "  border-left: 3px solid #2a82da;"
        "}"
        "#agentCodeHeader {"
        "  background: %3;"
        "}"
        "#agentCodeTitle, #agentCodeChevron {"
        "  color: %6;"
        "}"
        "#agentCodeMeta {"
        "  color: %2; font-size: 11px;"
        "}"
        "QPlainTextEdit#agentCodeBody {"
        "  background: %8; color: %6; border: none; font-size: 12px;"
        "}"
        "#agentWaitDot {"
        "  background-color: %2; border: none; border-radius: 2px;"
        "}"
        "#agentWaitStage {"
        "  color: %2; font-size: 11px;"
        "}"
        "#agentStatusLabel {"
        "  color: %2; font-size: 11px; background: transparent;"
        "}"
        "#agentInputWrap { background: %1; }"
        "#agentInputCard {"
        "  background: %4;"
        "  border: 1px solid %5;"
        "  border-radius: 14px;"
        "}"
        "#agentInputEdit {"
        "  background: transparent; color: %6; border: none;"
        "  font-size: 13px; selection-background-color: #2a82da;"
        "}"
        "#agentModelPicker {"
        "  background: transparent; border: 1px solid %5; border-radius: 8px;"
        "}"
        "#agentApiKeyPicker {"
        "  background: transparent; border: 1px solid %5; border-radius: 8px;"
        "}"
        "#agentApiKeyBtn {"
        "  background: transparent; color: %6; border: none;"
        "  padding: 4px 6px; font-size: 12px; text-align: left;"
        "}"
        "#agentApiKeyBtn:hover { background: %3; border-radius: 7px; }"
        "#agentModelBtn {"
        "  background: transparent; color: %6; border: none;"
        "  padding: 4px 4px 4px 8px; font-size: 12px; text-align: left;"
        "}"
        "#agentModelBtn:hover { background: %3; border-top-left-radius: 7px; border-bottom-left-radius: 7px; }"
        "#agentModelRefreshBtn {"
        "  color: %2; border: none; border-left: 1px solid %5;"
        "  background: transparent; border-top-right-radius: 7px; border-bottom-right-radius: 7px;"
        "  font-size: 13px;"
        "}"
        "#agentModelRefreshBtn:hover { background: %3; }"
        "#agentStopBtn {"
        "  color: %2; border: none; padding: 4px 8px; font-size: 13px;"
        "  background: transparent; border-radius: 6px;"
        "}"
        "#agentStopBtn:hover { background: %3; }"
        )
        .arg(bg, muted, hover, cardBg, border, fg, userBg, inputBg));

    applyModeButtonStyle();
    applyApiKeyIcon();
    if (m_sendBtn) {
        m_sendBtn->applyTheme(m_theme);
    }
    updateApiKeyButtonText();
    updateToolbarMinimumWidth();
    rebuildChat();
}
