#include "AgentHttps.h"

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#endif

#include <QUrl>

namespace AgentHttps {

#ifdef Q_OS_WIN

AgentHttpsResult request(const QString &method,
                         const QUrl &url,
                         const QString &extraHeaders,
                         const QByteArray &body,
                         int timeoutMs)
{
    AgentHttpsResult result;

    if (!url.isValid() || url.host().isEmpty()) {
        result.error = QStringLiteral("Некорректный URL.");
        return result;
    }

    const bool https = (url.scheme() == QLatin1String("https"));
    const QString host = url.host();
    const INTERNET_PORT port = url.port() > 0
        ? static_cast<INTERNET_PORT>(url.port())
        : (https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT);
    const QString objectName = url.path(QUrl::FullyEncoded)
        + (url.hasQuery() ? QLatin1Char('?') + url.query(QUrl::FullyEncoded) : QString());
    const QString verb = method.isEmpty() ? QStringLiteral("GET") : method.toUpper();

    HINTERNET session = WinHttpOpen(L"Vuzhyk/1.0",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS,
                                    0);
    if (!session) {
        result.error = QStringLiteral("Не удалось инициализировать WinHTTP (код %1).")
                           .arg(GetLastError());
        return result;
    }

    DWORD secureProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1
        | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1
        | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &secureProtocols, sizeof(secureProtocols));

    HINTERNET connection = WinHttpConnect(session,
                                          reinterpret_cast<LPCWSTR>(host.utf16()),
                                          port,
                                          0);
    if (!connection) {
        result.error = QStringLiteral("Не удалось подключиться к серверу (код %1).")
                           .arg(GetLastError());
        WinHttpCloseHandle(session);
        return result;
    }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(connection,
                                            reinterpret_cast<LPCWSTR>(verb.utf16()),
                                            reinterpret_cast<LPCWSTR>(objectName.utf16()),
                                            nullptr,
                                            WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            flags);
    if (!hRequest) {
        result.error = QStringLiteral("Не удалось создать HTTP-запрос (код %1).")
                           .arg(GetLastError());
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    DWORD timeout = timeoutMs > 0 ? static_cast<DWORD>(timeoutMs) : 20000;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hRequest, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    if (!extraHeaders.trimmed().isEmpty()) {
        const QString headers = extraHeaders;
        WinHttpAddRequestHeaders(hRequest,
                                 reinterpret_cast<LPCWSTR>(headers.utf16()),
                                 static_cast<DWORD>(-1),
                                 WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }

    LPVOID bodyPtr = body.isEmpty()
        ? WINHTTP_NO_REQUEST_DATA
        : static_cast<LPVOID>(const_cast<char *>(body.constData()));
    const DWORD bodySize = static_cast<DWORD>(body.size());

    if (!WinHttpSendRequest(hRequest,
                            WINHTTP_NO_ADDITIONAL_HEADERS,
                            0,
                            bodyPtr,
                            bodySize,
                            bodySize,
                            0)) {
        result.error = QStringLiteral("Не удалось отправить HTTP-запрос (код %1).")
                           .arg(GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        result.error = QStringLiteral("Не удалось получить ответ сервера (код %1).")
                           .arg(GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (WinHttpQueryHeaders(hRequest,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &statusCode,
                            &statusCodeSize,
                            WINHTTP_NO_HEADER_INDEX)) {
        result.status = static_cast<int>(statusCode);
    }

    const DWORD chunkSize = 64 * 1024;
    QByteArray buffer(static_cast<int>(chunkSize), Qt::Uninitialized);
    for (;;) {
        DWORD downloaded = 0;
        if (!WinHttpReadData(hRequest, buffer.data(), chunkSize, &downloaded)) {
            result.error = QStringLiteral("Ошибка чтения данных (код %1).").arg(GetLastError());
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return result;
        }
        if (downloaded == 0) {
            break;
        }
        result.body.append(buffer.constData(), static_cast<int>(downloaded));
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    result.ok = (result.status >= 200 && result.status < 300);
    if (!result.ok && result.error.isEmpty()) {
        result.error = QStringLiteral("Сервер вернул код HTTP %1.").arg(result.status);
    }
    return result;
}

#else

AgentHttpsResult request(const QString &,
                         const QUrl &,
                         const QString &,
                         const QByteArray &,
                         int)
{
    AgentHttpsResult result;
    result.error = QStringLiteral("HTTPS через WinHTTP доступен только в Windows.");
    return result;
}

#endif

} // namespace AgentHttps
