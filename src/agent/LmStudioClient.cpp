#include "LmStudioClient.h"
#include "AgentHttps.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>
#include <QTimer>
#include <QThread>
#include <QMetaObject>

LmStudioClient::LmStudioClient(QObject *parent)
    : QObject(parent)
    , m_baseUrl(QStringLiteral("http://127.0.0.1:1234/v1"))
{
}

void LmStudioClient::setBaseUrl(const QString &baseUrl)
{
    QString url = baseUrl.trimmed();
    while (url.endsWith(QLatin1Char('/'))) {
        url.chop(1);
    }
    if (url.isEmpty()) {
        url = QStringLiteral("http://127.0.0.1:1234/v1");
    }
    m_baseUrl = url;
}

void LmStudioClient::setApiKey(const QString &apiKey)
{
    m_apiKey = apiKey.trimmed();
}

QString LmStudioClient::openRouterHeaders(const QString &apiKey)
{
    QString headers = QStringLiteral(
        "Accept: application/json\r\n"
        "User-Agent: Vuzhyk/1.0\r\n"
        "HTTP-Referer: https://vuzhyk.local\r\n"
        "X-Title: Vuzhyk");
    const QString key = apiKey.trimmed();
    if (!key.isEmpty()) {
        headers += QStringLiteral("\r\nAuthorization: Bearer %1").arg(key);
    }
    headers += QStringLiteral("\r\nContent-Type: application/json");
    return headers;
}

bool LmStudioClient::isBusy() const
{
    return m_reply != nullptr || m_httpsThread != nullptr;
}

bool LmStudioClient::usesWinHttp() const
{
    return m_baseUrl.startsWith(QLatin1String("https://"), Qt::CaseInsensitive);
}

void LmStudioClient::applyAuthHeaders(QNetworkRequest &request) const
{
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "Vuzhyk/1.0");
    if (!m_apiKey.isEmpty()) {
        request.setRawHeader("Authorization",
                             QStringLiteral("Bearer %1").arg(m_apiKey).toUtf8());
    }
}

void LmStudioClient::startWinHttp(const QString &method,
                                  const QUrl &url,
                                  const QByteArray &body,
                                  int timeoutMs,
                                  bool modelsRequest)
{
    const int generation = ++m_httpsGeneration;
    const QString headers = openRouterHeaders(m_apiKey);

    auto *thread = QThread::create(
        [this, method, url, headers, body, timeoutMs, modelsRequest, generation]() {
            const AgentHttpsResult result = AgentHttps::request(method, url, headers, body, timeoutMs);
            QMetaObject::invokeMethod(this, [this, result, modelsRequest, generation]() {
                if (generation != m_httpsGeneration.load()) {
                    return;
                }
                m_httpsThread = nullptr;

                if (!result.ok) {
                    QString message = result.error;
                    if (!result.body.isEmpty()) {
                        const QJsonDocument errDoc = QJsonDocument::fromJson(result.body);
                        const QString apiMsg = errDoc.object()
                            .value(QStringLiteral("error")).toObject()
                            .value(QStringLiteral("message")).toString();
                        if (!apiMsg.isEmpty()) {
                            message = apiMsg;
                        }
                    }
                    if (modelsRequest) {
                        emit modelsFailed(message);
                    } else {
                        emit chatFailed(message);
                    }
                    return;
                }

                if (modelsRequest) {
                    const QJsonDocument doc = QJsonDocument::fromJson(result.body);
                    if (!doc.isObject()) {
                        emit modelsFailed(tr("Некорректный ответ /v1/models."));
                        return;
                    }
                    QStringList models;
                    const QJsonArray data = doc.object().value(QStringLiteral("data")).toArray();
                    for (const QJsonValue &value : data) {
                        const QString id = value.toObject().value(QStringLiteral("id")).toString();
                        if (!id.isEmpty()) {
                            models.append(id);
                        }
                    }
                    models.removeDuplicates();
                    models.sort(Qt::CaseInsensitive);
                    emit modelsLoaded(models);
                    return;
                }

                m_accumulated.clear();
                const QJsonDocument doc = QJsonDocument::fromJson(result.body);
                if (doc.isObject()) {
                    const QJsonArray choices = doc.object().value(QStringLiteral("choices")).toArray();
                    if (!choices.isEmpty()) {
                        m_accumulated = choices.at(0).toObject()
                                            .value(QStringLiteral("message")).toObject()
                                            .value(QStringLiteral("content")).toString();
                    }
                }
                if (m_accumulated.isEmpty()) {
                    emit chatFailed(tr("Пустой ответ от модели."));
                    return;
                }
                emit chatFinished(m_accumulated);
            }, Qt::QueuedConnection);
        });

    m_httpsThread = thread;
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

QUrl LmStudioClient::endpoint(const QString &path) const
{
    QString p = path;
    if (!p.startsWith(QLatin1Char('/'))) {
        p.prepend(QLatin1Char('/'));
    }
    return QUrl(m_baseUrl + p);
}

void LmStudioClient::clearReply()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_sseBuffer.clear();
    m_streaming = false;
}

void LmStudioClient::cancel()
{
    ++m_httpsGeneration;
    if (m_reply) {
        m_reply->abort();
    }
    clearReply();
}

void LmStudioClient::refreshModels()
{
    cancel();

    if (usesWinHttp()) {
        startWinHttp(QStringLiteral("GET"),
                     endpoint(QStringLiteral("/models")),
                     QByteArray(),
                     25000,
                     true);
        return;
    }

    QNetworkRequest request(endpoint(QStringLiteral("/models")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    applyAuthHeaders(request);

    m_reply = m_nam.get(request);
    auto *timer = new QTimer(m_reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, m_reply, &QNetworkReply::abort);
    timer->start(25000);
    connect(m_reply, &QNetworkReply::finished, this, &LmStudioClient::onModelsFinished);
}

void LmStudioClient::onModelsFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }

    const QByteArray body = reply->readAll();
    const QNetworkReply::NetworkError error = reply->error();
    const QString errorString = reply->errorString();
    reply->deleteLater();
    if (m_reply == reply) {
        m_reply = nullptr;
    }

    if (error != QNetworkReply::NoError) {
        const QString message = (error == QNetworkReply::OperationCanceledError)
            ? tr("Таймаут: сервер не ответил.")
            : tr("Не удалось получить список моделей: %1").arg(errorString);
        emit modelsFailed(message);
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        emit modelsFailed(tr("Некорректный ответ /v1/models."));
        return;
    }

    QStringList models;
    const QJsonArray data = doc.object().value(QStringLiteral("data")).toArray();
    for (const QJsonValue &value : data) {
        const QJsonObject obj = value.toObject();
        const QString id = obj.value(QStringLiteral("id")).toString();
        if (!id.isEmpty()) {
            models.append(id);
        }
    }
    models.removeDuplicates();
    models.sort(Qt::CaseInsensitive);
    emit modelsLoaded(models);
}

void LmStudioClient::chatCompletion(const QString &model, const QJsonArray &messages, bool stream)
{
    cancel();

    m_accumulated.clear();
    m_sseBuffer.clear();
    m_streaming = stream && !usesWinHttp();

    QJsonObject body;
    body.insert(QStringLiteral("model"), model);
    body.insert(QStringLiteral("messages"), messages);
    body.insert(QStringLiteral("stream"), m_streaming);
    body.insert(QStringLiteral("temperature"), 0.2);

    if (usesWinHttp()) {
        startWinHttp(QStringLiteral("POST"),
                     endpoint(QStringLiteral("/chat/completions")),
                     QJsonDocument(body).toJson(QJsonDocument::Compact),
                     120000,
                     false);
        return;
    }

    QNetworkRequest request(endpoint(QStringLiteral("/chat/completions")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    applyAuthHeaders(request);
    request.setRawHeader("Accept", m_streaming ? "text/event-stream" : "application/json");

    m_reply = m_nam.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    auto *timer = new QTimer(m_reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, m_reply, &QNetworkReply::abort);
    timer->start(120000);
    connect(m_reply, &QNetworkReply::readyRead, this, &LmStudioClient::onChatReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &LmStudioClient::onChatFinished);
}

void LmStudioClient::processSseBuffer()
{
    while (true) {
        const int newline = m_sseBuffer.indexOf('\n');
        if (newline < 0) {
            break;
        }

        QByteArray line = m_sseBuffer.left(newline);
        m_sseBuffer.remove(0, newline + 1);
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        if (line.isEmpty()) {
            continue;
        }

        if (!line.startsWith("data:")) {
            continue;
        }

        QByteArray payload = line.mid(5).trimmed();
        if (payload == "[DONE]") {
            continue;
        }

        // LM Studio отдаёт JSON в UTF-8
        const QJsonDocument doc = QJsonDocument::fromJson(payload);
        if (!doc.isObject()) {
            continue;
        }

        const QJsonArray choices = doc.object().value(QStringLiteral("choices")).toArray();
        if (choices.isEmpty()) {
            continue;
        }

        const QJsonObject choice = choices.at(0).toObject();
        QString delta = choice.value(QStringLiteral("delta")).toObject()
                            .value(QStringLiteral("content")).toString();
        if (delta.isEmpty()) {
            delta = choice.value(QStringLiteral("message")).toObject()
                        .value(QStringLiteral("content")).toString();
        }
        if (!delta.isEmpty()) {
            m_accumulated += delta;
            emit streamChunk(delta);
        }
    }
}

void LmStudioClient::onChatReadyRead()
{
    if (!m_reply) {
        return;
    }
    m_sseBuffer.append(m_reply->readAll());
    if (m_streaming) {
        processSseBuffer();
    }
}

void LmStudioClient::onChatFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }

    m_sseBuffer.append(reply->readAll());
    const QNetworkReply::NetworkError error = reply->error();
    const QString errorString = reply->errorString();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (m_streaming && error == QNetworkReply::NoError) {
        processSseBuffer();
    }

    reply->deleteLater();
    if (m_reply == reply) {
        m_reply = nullptr;
    }
    m_streaming = false;

    if (error == QNetworkReply::OperationCanceledError) {
        return;
    }

    if (error != QNetworkReply::NoError) {
        emit chatFailed(tr("Ошибка запроса к API (HTTP %1): %2").arg(status).arg(errorString));
        return;
    }

    if (m_accumulated.isEmpty()) {
        // Non-streaming fallback parse
        const QJsonDocument doc = QJsonDocument::fromJson(m_sseBuffer);
        if (doc.isObject()) {
            const QJsonArray choices = doc.object().value(QStringLiteral("choices")).toArray();
            if (!choices.isEmpty()) {
                m_accumulated = choices.at(0).toObject()
                                    .value(QStringLiteral("message")).toObject()
                                    .value(QStringLiteral("content")).toString();
            }
        }
    }

    if (m_accumulated.isEmpty()) {
        emit chatFailed(tr("Пустой ответ от модели."));
        return;
    }

    emit chatFinished(m_accumulated);
}
