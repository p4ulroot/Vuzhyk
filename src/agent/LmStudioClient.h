#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QStringList>
#include <QJsonArray>
#include <QPointer>
#include <QThread>
#include <atomic>

class LmStudioClient : public QObject {
    Q_OBJECT
public:
    explicit LmStudioClient(QObject *parent = nullptr);

    void setBaseUrl(const QString &baseUrl);
    QString baseUrl() const { return m_baseUrl; }

    void setApiKey(const QString &apiKey);
    QString apiKey() const { return m_apiKey; }
    void clearApiKey() { m_apiKey.clear(); }

    void refreshModels();
    void cancel();

    void chatCompletion(const QString &model,
                        const QJsonArray &messages,
                        bool stream = true);

    bool isBusy() const;

    static QString openRouterHeaders(const QString &apiKey);

signals:
    void modelsLoaded(const QStringList &modelIds);
    void modelsFailed(const QString &error);
    void streamChunk(const QString &deltaText);
    void chatFinished(const QString &fullText);
    void chatFailed(const QString &error);

private slots:
    void onModelsFinished();
    void onChatReadyRead();
    void onChatFinished();

private:
    QUrl endpoint(const QString &path) const;
    void applyAuthHeaders(QNetworkRequest &request) const;
    void clearReply();
    void processSseBuffer();
    bool usesWinHttp() const;
    void startWinHttp(const QString &method,
                      const QUrl &url,
                      const QByteArray &body,
                      int timeoutMs,
                      bool modelsRequest);

    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_reply;
    QPointer<QThread> m_httpsThread;
    std::atomic<int> m_httpsGeneration { 0 };
    QString m_baseUrl;
    QString m_apiKey;
    QByteArray m_sseBuffer;
    QString m_accumulated;
    bool m_streaming { false };
};
