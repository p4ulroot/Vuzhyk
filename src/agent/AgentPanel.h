#pragma once

#include <QWidget>
#include <QComboBox>
#include <QToolButton>
#include <QPlainTextEdit>
#include <QLabel>
#include <QTimer>
#include <QFrame>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QJsonArray>
#include <QEvent>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QSvgWidget>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QGraphicsOpacityEffect>
#include <functional>

#include "AgentPrompts.h"

class LmStudioClient;

class AgentSendButton : public QSvgWidget {
    Q_OBJECT
public:
    explicit AgentSendButton(QWidget *parent = nullptr);

    void applyTheme(const QString &theme);

signals:
    void clicked();

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QGraphicsOpacityEffect *m_opacityEffect { nullptr };
};

class AgentPanel : public QWidget {
    Q_OBJECT
public:
    using ContextProvider = std::function<QString()>;
    using CodeApplier = std::function<bool(const QString &assistantResponse, QString *statusMessage)>;

    explicit AgentPanel(QWidget *parent = nullptr);

    void setTheme(const QString &theme);
    void setContextProvider(ContextProvider provider);
    void setCodeApplier(CodeApplier applier);
    void reloadSettings();
    void refreshModels();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    int toolbarMinimumWidth() const;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

public slots:
    void clearChat();

private slots:
    void onSend();
    void onStop();
    void onModeMenuTriggered(QAction *action);
    void showModeMenu();
    void showModelMenu();
    void showApiKeyMenu();
    void onProviderChanged();
    void onModelsLoaded(const QStringList &models);
    void onModelsFailed(const QString &error);
    void onStreamChunk(const QString &delta);
    void onChatFinished(const QString &fullText);
    void onChatFailed(const QString &error);
    void onModelMenuTriggered(QAction *action);
    void onApiKeyMenuTriggered(QAction *action);
    void onWaitAnimationTick();

private:
    void setupUi();
    void applyThemeStyles();
    void applyModeButtonStyle();
    void applyApiKeyIcon();
    void updateBusyState(bool busy);
    void updateModeButton();
    void applyProviderSettings();
    void updateProviderUi();
    void updateModelButtonText();
    void updateApiKeyButtonText();
    void updateToolbarMinimumWidth();
    void rebuildModelMenu(const QStringList &models);
    void rebuildApiKeyMenu();
    void saveSelectedOpenRouterKey();
    QString shortModelName(const QString &id) const;
    void appendUserMessage(const QString &text);
    void beginAssistantMessage();
    void appendAssistantDelta(const QString &delta);
    void finishAssistantMessage(const QString &fullText);
    void rebuildChat();
    void startWaitAnimation(const QString &stage);
    void stopWaitAnimation();
    void setWaitStage(const QString &stage);
    void updateWaitDots();
    void scrollChatToBottom();
    void clearMessageWidgets();
    QWidget *createUserBubble(const QString &text);
    QWidget *createAssistantMessage(const QString &text, bool streaming);
    QJsonArray buildMessages(const QString &userText) const;
    void saveSelectedModel();
    void saveMode();
    bool tryApplyCodeFromResponse(const QString &fullText, QString *statusMessage);

    LmStudioClient *m_client { nullptr };
    ContextProvider m_contextProvider;
    CodeApplier m_codeApplier;

    QComboBox *m_providerCombo { nullptr };
    QFrame *m_apiKeyPicker { nullptr };
    QToolButton *m_apiKeyBtn { nullptr };
    QMenu *m_apiKeyMenu { nullptr };
    QActionGroup *m_apiKeyActionGroup { nullptr };
    QFrame *m_modelPicker { nullptr };
    QToolButton *m_modelBtn { nullptr };
    QToolButton *m_refreshModelsBtn { nullptr };
    QMenu *m_modelMenu { nullptr };
    QActionGroup *m_modelActionGroup { nullptr };
    QToolButton *m_modeBtn { nullptr };
    QMenu *m_modeMenu { nullptr };
    QAction *m_askAction { nullptr };
    QAction *m_codeAction { nullptr };
    QScrollArea *m_chatScroll { nullptr };
    QWidget *m_chatInner { nullptr };
    QVBoxLayout *m_messagesLayout { nullptr };
    QLabel *m_emptyHint { nullptr };
    QWidget *m_waitWidget { nullptr };
    QLabel *m_waitDot[3] { nullptr, nullptr, nullptr };
    QLabel *m_waitStageLabel { nullptr };
    QLabel *m_streamingLabel { nullptr };
    QFrame *m_inputCard { nullptr };
    QPlainTextEdit *m_input { nullptr };
    AgentSendButton *m_sendBtn { nullptr };
    QToolButton *m_stopBtn { nullptr };
    QToolButton *m_clearBtn { nullptr };
    QLabel *m_statusLabel { nullptr };

    AgentPrompts::Mode m_mode { AgentPrompts::Mode::Ask };
    QString m_theme { QStringLiteral("light") };
    QString m_provider { QStringLiteral("lmstudio") };
    QString m_selectedOpenRouterKeyName;
    QString m_selectedModelId;
    QString m_pendingModelId;
    QString m_pendingAssistantHtml;
    bool m_assistantStreaming { false };
    QTimer *m_waitTimer { nullptr };
    int m_waitFrame { 0 };
    QString m_waitStage;
    QStringList m_historyUser;
    QStringList m_historyAssistant;
};
