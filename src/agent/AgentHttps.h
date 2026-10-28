#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

struct AgentHttpsResult {
    bool ok { false };
    int status { 0 };
    QByteArray body;
    QString error;
};

namespace AgentHttps {

AgentHttpsResult request(const QString &method,
                         const QUrl &url,
                         const QString &extraHeaders,
                         const QByteArray &body,
                         int timeoutMs);

} // namespace AgentHttps
