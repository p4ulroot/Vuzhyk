#include "SettingsWidget.h"
#include "PackageManagerWidget.h"
#include "extensions/ExtensionManager.h"
#include "PythonInterpreterPaths.h"
#include "sync/LibrarySyncCrypto.h"
#include "sync/LibrarySyncServerControl.h"
#include "agent/AgentOpenRouterKeys.h"
#include "agent/LmStudioClient.h"
#include "agent/AgentHttps.h"
#include <QSettings>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QFileDialog>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QProcess>
#include <QMessageBox>
#include <QInputDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVector>
#include <QPair>
#include <QSharedPointer>
#include <QPointer>
#include <QJsonDocument>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QStringList>
#include <QKeySequenceEdit>
#include <QKeySequence>
#include <QComboBox>
#include <QAbstractSpinBox>
#include <QGroupBox>
#include <QFrame>
#include <QSizePolicy>
#include <QTimer>
#include <QSignalBlocker>
#include <QProgressDialog>
#include <QTemporaryDir>
#include <QUrl>
#include <QThread>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <atomic>
#ifdef Q_OS_WIN
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#endif

namespace {

constexpr char kPythonInstallersBaseUrl[] =
    "https://github.com/p4ulroot/python-installers/releases/download/A1";

QString pythonArchiveDownloadUrl(const QString &version)
{
    return QStringLiteral("%1/%2.7z").arg(QLatin1String(kPythonInstallersBaseUrl), version);
}

#ifdef Q_OS_WIN
struct HttpsDownloadResult {
    bool success { false };
    DWORD winError { 0 };
    QString errorMessage;
};

struct HttpsDownloadProgress {
    std::atomic<qint64> bytesDownloaded { 0 };
    std::atomic<qint64> totalBytes { 0 };
};

HttpsDownloadResult downloadHttpsFileViaWinHttp(
    const QString &urlString,
    const QString &destPath,
    const std::atomic<bool> *cancelled,
    HttpsDownloadProgress *progress = nullptr);

class PythonArchiveDownloadThread : public QThread {
public:
    explicit PythonArchiveDownloadThread(QObject *parent = nullptr)
        : QThread(parent)
    {
    }

    QString url;
    QString destPath;
    std::atomic<bool> cancelled { false };
    HttpsDownloadProgress progress;
    HttpsDownloadResult result;

protected:
    void run() override
    {
        result = downloadHttpsFileViaWinHttp(url, destPath, &cancelled, &progress);
    }
};

HttpsDownloadResult downloadHttpsFileViaWinHttp(
    const QString &urlString,
    const QString &destPath,
    const std::atomic<bool> *cancelled,
    HttpsDownloadProgress *progress)
{
    if (progress) {
        progress->bytesDownloaded.store(0);
        progress->totalBytes.store(0);
    }
    HttpsDownloadResult result;

    const QUrl url(urlString);
    if (!url.isValid() || url.scheme() != QLatin1String("https") || url.host().isEmpty()) {
        result.errorMessage = QObject::tr("Некорректный URL загрузки.");
        return result;
    }

    const QString host = url.host();
    const INTERNET_PORT port = url.port() > 0
        ? static_cast<INTERNET_PORT>(url.port())
        : INTERNET_DEFAULT_HTTPS_PORT;
    const QString objectName = url.path(QUrl::FullyEncoded)
        + (url.hasQuery() ? QLatin1Char('?') + url.query(QUrl::FullyEncoded) : QString());

    HINTERNET session = WinHttpOpen(L"Vuzhyk/1.0",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS,
                                    0);
    if (!session) {
        result.winError = GetLastError();
        result.errorMessage = QObject::tr("Не удалось инициализировать WinHTTP (код %1).")
            .arg(result.winError);
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
        result.winError = GetLastError();
        result.errorMessage = QObject::tr("Не удалось подключиться к серверу (код %1).")
            .arg(result.winError);
        WinHttpCloseHandle(session);
        return result;
    }

    HINTERNET request = WinHttpOpenRequest(connection,
                                           L"GET",
                                           reinterpret_cast<LPCWSTR>(objectName.utf16()),
                                           nullptr,
                                           WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
    if (!request) {
        result.winError = GetLastError();
        result.errorMessage = QObject::tr("Не удалось создать HTTP-запрос (код %1).")
            .arg(result.winError);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    DWORD timeoutMs = 120000;
    WinHttpSetOption(request, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    WinHttpSetOption(request, WINHTTP_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    WinHttpSetOption(request, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

    if (!WinHttpSendRequest(request,
                            WINHTTP_NO_ADDITIONAL_HEADERS,
                            0,
                            WINHTTP_NO_REQUEST_DATA,
                            0,
                            0,
                            0)) {
        result.winError = GetLastError();
        result.errorMessage = QObject::tr("Не удалось отправить HTTP-запрос (код %1).")
            .arg(result.winError);
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    if (!WinHttpReceiveResponse(request, nullptr)) {
        result.winError = GetLastError();
        result.errorMessage = QObject::tr("Не удалось получить ответ сервера (код %1).")
            .arg(result.winError);
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (WinHttpQueryHeaders(request,
                           WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                           WINHTTP_HEADER_NAME_BY_INDEX,
                           &statusCode,
                           &statusCodeSize,
                           WINHTTP_NO_HEADER_INDEX)
        && statusCode != 200) {
        result.errorMessage = QObject::tr("Сервер вернул код HTTP %1.").arg(statusCode);
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    DWORD contentLength = 0;
    DWORD contentLengthSize = sizeof(contentLength);
    if (WinHttpQueryHeaders(request,
                            WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &contentLength,
                            &contentLengthSize,
                            WINHTTP_NO_HEADER_INDEX)
        && contentLength > 0 && progress) {
        progress->totalBytes.store(static_cast<qint64>(contentLength));
    }

    QFile outFile(destPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        result.errorMessage = QObject::tr("Не удалось создать файл: %1").arg(destPath);
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    const DWORD chunkSize = 64 * 1024;
    QByteArray buffer(static_cast<int>(chunkSize), Qt::Uninitialized);

    for (;;) {
        if (cancelled && cancelled->load()) {
            result.errorMessage = QObject::tr("Загрузка отменена.");
            outFile.close();
            QFile::remove(destPath);
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return result;
        }

        DWORD downloaded = 0;
        if (!WinHttpReadData(request, buffer.data(), chunkSize, &downloaded)) {
            result.winError = GetLastError();
            result.errorMessage = QObject::tr("Ошибка чтения данных (код %1).")
                .arg(result.winError);
            outFile.close();
            QFile::remove(destPath);
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return result;
        }

        if (downloaded == 0) {
            break;
        }

        if (outFile.write(buffer.constData(), static_cast<qint64>(downloaded)) != static_cast<qint64>(downloaded)) {
            result.errorMessage = QObject::tr("Не удалось записать данные в файл.");
            outFile.close();
            QFile::remove(destPath);
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return result;
        }

        if (progress) {
            progress->bytesDownloaded.fetch_add(static_cast<qint64>(downloaded));
        }
    }

    outFile.close();
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    result.success = true;
    return result;
}
#endif

bool isValidPythonArchiveFile(const QString &path, QString *errorMessage)
{
    QFileInfo info(path);
    if (!info.exists() || !info.isReadable()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Не удалось прочитать архив: %1").arg(path);
        }
        return false;
    }

    if (info.size() < 1024 * 1024) {
        if (errorMessage) {
            *errorMessage = QObject::tr(
                "Скачанный файл слишком мал (%1 байт).\n\n"
                "Возможно, файл не найден на сервере или скачался не полностью.")
                .arg(info.size());
        }
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Не удалось прочитать архив: %1").arg(path);
        }
        return false;
    }

    const QByteArray header = file.read(6);
    file.close();
    if (header.size() < 6
        || static_cast<unsigned char>(header[0]) != 0x37
        || static_cast<unsigned char>(header[1]) != 0x7A
        || static_cast<unsigned char>(header[2]) != 0xBC
        || static_cast<unsigned char>(header[3]) != 0xAF
        || static_cast<unsigned char>(header[4]) != 0x27
        || static_cast<unsigned char>(header[5]) != 0x1C) {
        if (errorMessage) {
            *errorMessage = QObject::tr(
                "Скачанный файл не является архивом 7z.\n\n"
                "Проверьте доступность релиза на GitHub.");
        }
        return false;
    }

    return true;
}

#ifdef Q_OS_WIN
bool removeInterpreterDirectory(const QString &dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists()) {
        return true;
    }

    QDirIterator it(dirPath, QDir::NoDotAndDotDot | QDir::AllEntries, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo info(it.next());
        if (info.isFile() || info.isSymLink()) {
            QFile::setPermissions(info.absoluteFilePath(),
                                  QFile::WriteUser | QFile::ReadUser | QFile::WriteOwner | QFile::ReadOwner);
        }
    }

    return dir.removeRecursively();
}

bool isEmbeddedInterpreterPath(const QString &pythonPath)
{
    if (pythonPath.isEmpty()) {
        return false;
    }

    const QString embeddedPython =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("Python38/python.exe"));
    return QFileInfo(pythonPath).absoluteFilePath().compare(QFileInfo(embeddedPython).absoluteFilePath(),
                                                            Qt::CaseInsensitive) == 0;
}

bool isManagedInterpreterPath(const QString &pythonPath)
{
    if (pythonPath.isEmpty()) {
        return false;
    }

    const QString interpretersRoot =
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("interpreters"));
    const QString normalizedExe = QDir::toNativeSeparators(QFileInfo(pythonPath).absoluteFilePath());
    const QString normalizedRoot = QDir::toNativeSeparators(QDir(interpretersRoot).absolutePath());
    return normalizedExe.startsWith(normalizedRoot + QLatin1Char('\\'), Qt::CaseInsensitive)
        || normalizedExe.startsWith(normalizedRoot + QLatin1Char('/'), Qt::CaseInsensitive);
}

QString sevenZipExecutablePath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("tools/zip/7za.exe"));
}

void configureExternalToolProcess(QProcess &process)
{
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.setStandardOutputFile(QProcess::nullDevice());
}

bool moveDirectoryContents(const QString &sourceDir, const QString &destDir)
{
    QDir src(sourceDir);
    const QFileInfoList entries = src.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden);
    for (const QFileInfo &entry : entries) {
        const QString destPath = QDir(destDir).filePath(entry.fileName());
        if (entry.isDir()) {
            if (QDir(destPath).exists()) {
                if (!moveDirectoryContents(entry.absoluteFilePath(), destPath)) {
                    return false;
                }
                if (!QDir(entry.absoluteFilePath()).removeRecursively()) {
                    return false;
                }
            } else if (!QDir().rename(entry.absoluteFilePath(), destPath)) {
                return false;
            }
        } else {
            if (QFile::exists(destPath)) {
                QFile::setPermissions(destPath, QFile::WriteUser | QFile::ReadUser | QFile::WriteOwner | QFile::ReadOwner);
                QFile::remove(destPath);
            }
            if (!QFile::rename(entry.absoluteFilePath(), destPath)) {
                if (!QFile::copy(entry.absoluteFilePath(), destPath)) {
                    return false;
                }
                QFile::remove(entry.absoluteFilePath());
            }
        }
    }

    return src.removeRecursively();
}

bool normalizeExtractedInterpreterDir(const QString &targetDir)
{
    if (QFile::exists(QDir(targetDir).filePath(QStringLiteral("python.exe")))) {
        return true;
    }

    QDir dir(targetDir);
    const QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &sub : subdirs) {
        const QString nested = dir.filePath(sub);
        if (QFile::exists(QDir(nested).filePath(QStringLiteral("python.exe")))) {
            return moveDirectoryContents(nested, targetDir);
        }
    }

    return false;
}

bool extract7zArchive(const QString &archivePath, const QString &destDir, QString *errorMessage)
{
    const QString sevenZip = sevenZipExecutablePath();
    if (!QFile::exists(sevenZip)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Не найден архиватор tools/zip/7za.exe");
        }
        return false;
    }

    if (!QDir().mkpath(destDir)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Не удалось создать папку: %1").arg(destDir);
        }
        return false;
    }

    QString outputDir = QDir::toNativeSeparators(QDir(destDir).absolutePath());
    if (!outputDir.endsWith(QLatin1Char('\\')) && !outputDir.endsWith(QLatin1Char('/'))) {
        outputDir.append(QLatin1Char('\\'));
    }

    QProcess process;
    configureExternalToolProcess(process);
    process.setProgram(sevenZip);
    process.setArguments(QStringList()
        << QStringLiteral("x")
        << QStringLiteral("-y")
        << QStringLiteral("-aoa")
        << QStringLiteral("-o") + outputDir
        << QDir::toNativeSeparators(QFileInfo(archivePath).absoluteFilePath()));
    process.setWorkingDirectory(QFileInfo(archivePath).absolutePath());
    process.start();

    if (!process.waitForStarted(30000)) {
        if (errorMessage) {
            *errorMessage = process.errorString();
        }
        return false;
    }

    if (!process.waitForFinished(30 * 60 * 1000)) {
        process.kill();
        process.waitForFinished(5000);
        if (errorMessage) {
            *errorMessage = QObject::tr("Распаковка заняла слишком много времени.");
        }
        return false;
    }

    const int exitCode = process.exitCode();
    const QString stdErr = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();

    if (process.exitStatus() != QProcess::NormalExit) {
        if (errorMessage) {
            *errorMessage = stdErr.isEmpty()
                ? QObject::tr("Процесс распаковки завершился с ошибкой.")
                : stdErr;
        }
        return false;
    }

    if (exitCode == 0 || exitCode == 1) {
        return true;
    }

    // Код 2 часто означает пропуск нескольких ARM-файлов в pip; основной Python уже распакован.
    if (exitCode == 2) {
        if (errorMessage) {
            *errorMessage = stdErr;
        }
        return true;
    }

    if (errorMessage) {
        if (!stdErr.isEmpty()) {
            *errorMessage = stdErr;
        } else {
            *errorMessage = QObject::tr("Ошибка распаковки (код %1).").arg(exitCode);
        }
    }
    return false;
}
#endif

} // namespace

SettingsWidget::SettingsWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("SettingsWidget"));
    setupUi();
    
    // Загружаем сохраненный размер шрифта
    QSettings settings;
    int fontSize = settings.value("editor/fontSize", 10).toInt();
    m_fontSizeSpinBox->setValue(fontSize);
    
    // Загружаем настройки интерпретатора
    loadInterpreterSettings();
    
    // Загружаем настройки горячих клавиш
    loadShortcutSettings();
    
    // Инициализируем состояние раздела библиотек (без тяжелых операций)
    updatePackageUiEnabled();
}

void SettingsWidget::setTheme(const QString &theme)
{
    m_currentTheme = theme;
    applyThemeStyles();
}

void SettingsWidget::applyThemeStyles()
{
    const bool dark = (m_currentTheme == QLatin1String("dark"));

    if (m_closeButton) {
        if (dark) {
            m_closeButton->setStyleSheet(
                "QPushButton {"
                "    background: transparent;"
                "    border: none;"
                "    font-size: 20px;"
                "    font-weight: bold;"
                "    color: #b0b0b0;"
                "}"
                "QPushButton:hover {"
                "    background: rgba(255, 255, 255, 0.12);"
                "    color: #ffffff;"
                "}"
            );
        } else {
            m_closeButton->setStyleSheet(
                "QPushButton {"
                "    background: transparent;"
                "    border: none;"
                "    font-size: 20px;"
                "    font-weight: bold;"
                "    color: #666666;"
                "}"
                "QPushButton:hover {"
                "    background: rgba(0, 0, 0, 0.08);"
                "    color: #000000;"
                "}"
            );
        }
    }

    if (dark) {
        setStyleSheet(
            "SettingsWidget { background-color: #353535; color: #e8e8e8; }"
            "QLabel { color: #e8e8e8; background: transparent; }"
            "QGroupBox {"
            "    color: #e8e8e8;"
            "    border: 1px solid #555555;"
            "    border-radius: 4px;"
            "    margin-top: 10px;"
            "    padding-top: 8px;"
            "}"
            "QGroupBox::title {"
            "    subcontrol-origin: margin;"
            "    left: 10px;"
            "    padding: 0 4px;"
            "    color: #e8e8e8;"
            "}"
            "QLineEdit, QComboBox, QSpinBox, QKeySequenceEdit {"
            "    background-color: #232323;"
            "    color: #ffffff;"
            "    border: 1px solid #555555;"
            "    border-radius: 3px;"
            "    padding: 4px 6px;"
            "    selection-background-color: #2a82da;"
            "}"
            "QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled {"
            "    background-color: #2a2a2a;"
            "    color: #888888;"
            "    border-color: #444444;"
            "}"
            "QComboBox::drop-down {"
            "    border: none;"
            "    width: 20px;"
            "}"
            "QComboBox QAbstractItemView {"
            "    background-color: #353535;"
            "    color: #ffffff;"
            "    border: 1px solid #555555;"
            "    selection-background-color: #2a82da;"
            "    selection-color: #ffffff;"
            "}"
            "QSpinBox::up-button, QSpinBox::down-button {"
            "    background-color: #444444;"
            "    border: 1px solid #555555;"
            "    width: 18px;"
            "    color: #ffffff;"
            "}"
            "QSpinBox::up-button:hover, QSpinBox::down-button:hover {"
            "    background-color: #555555;"
            "}"
            "QPushButton#fontSizeIncButton, QPushButton#fontSizeDecButton {"
            "    padding: 0px;"
            "    min-width: 20px;"
            "    max-width: 20px;"
            "    font-size: 12px;"
            "    font-weight: bold;"
            "    border-radius: 0px;"
            "    margin: 0px;"
            "    border: none;"
            "    background-color: #444444;"
            "}"
            "QPushButton#fontSizeIncButton {"
            "    border-bottom: 1px solid #555555;"
            "}"
            "QPushButton#fontSizeIncButton:hover, QPushButton#fontSizeDecButton:hover {"
            "    background-color: #555555;"
            "}"
            "QPushButton#fontSizeIncButton:pressed, QPushButton#fontSizeDecButton:pressed {"
            "    background-color: #3a3a3a;"
            "}"
            "QWidget#fontSizeStepWidget {"
            "    margin: 0px;"
            "    padding: 0px;"
            "    border: 1px solid #555555;"
            "    border-left: none;"
            "    background-color: #444444;"
            "}"
            "QSpinBox#fontSizeSpinBox {"
            "    border-top-right-radius: 0px;"
            "    border-bottom-right-radius: 0px;"
            "    padding-top: 1px;"
            "    padding-bottom: 1px;"
            "}"
            "QPushButton#shortcutResetButton {"
            "    padding: 2px 5px;"
            "    min-width: 0px;"
            "    max-width: 68px;"
            "    font-size: 10px;"
            "}"
            "QListWidget {"
            "    background-color: #232323;"
            "    color: #e8e8e8;"
            "    border: 1px solid #555555;"
            "    outline: none;"
            "}"
            "QListWidget::item {"
            "    padding: 4px 6px;"
            "}"
            "QListWidget::item:selected {"
            "    background-color: #2a82da;"
            "    color: #ffffff;"
            "}"
            "QListWidget::item:hover:!selected {"
            "    background-color: #3a3a3a;"
            "}"
            "QListWidget#settingsCategoryList {"
            "    background-color: #2a2a2a;"
            "    border: none;"
            "    border-right: 1px solid #555555;"
            "}"
            "QListWidget#settingsCategoryList::item {"
            "    padding: 10px 12px;"
            "}"
            "QTableWidget {"
            "    background-color: #232323;"
            "    alternate-background-color: #2a2a2a;"
            "    color: #e8e8e8;"
            "    gridline-color: #555555;"
            "    border: 1px solid #555555;"
            "    selection-background-color: #2a82da;"
            "    selection-color: #ffffff;"
            "}"
            "QTableWidget::item {"
            "    color: #e8e8e8;"
            "    padding: 4px;"
            "}"
            "QHeaderView::section {"
            "    background-color: #2a2a2a;"
            "    color: #e8e8e8;"
            "    border: 1px solid #555555;"
            "    padding: 6px 8px;"
            "}"
            "QCheckBox, QRadioButton {"
            "    color: #e8e8e8;"
            "    spacing: 8px;"
            "}"
            "QCheckBox::indicator, QRadioButton::indicator {"
            "    width: 16px;"
            "    height: 16px;"
            "}"
            "QCheckBox::indicator:unchecked {"
            "    border: 1px solid #777777;"
            "    background-color: #232323;"
            "    border-radius: 2px;"
            "}"
            "QCheckBox::indicator:checked {"
            "    border: 1px solid #2a82da;"
            "    background-color: #2a82da;"
            "    border-radius: 2px;"
            "}"
            "QRadioButton::indicator:unchecked {"
            "    border: 1px solid #777777;"
            "    background-color: #232323;"
            "    border-radius: 8px;"
            "}"
            "QRadioButton::indicator:checked {"
            "    border: 1px solid #2a82da;"
            "    background-color: #2a82da;"
            "    border-radius: 8px;"
            "}"
            "QPushButton {"
            "    background-color: #4a4a4a;"
            "    color: #ffffff;"
            "    border: 1px solid #666666;"
            "    border-radius: 3px;"
            "    padding: 5px 12px;"
            "}"
            "QPushButton:hover {"
            "    background-color: #555555;"
            "    border-color: #777777;"
            "}"
            "QPushButton:pressed {"
            "    background-color: #3a3a3a;"
            "}"
            "QPushButton:disabled {"
            "    background-color: #333333;"
            "    color: #777777;"
            "    border-color: #444444;"
            "}"
            "QFrame {"
            "    border: 1px solid #555555;"
            "    border-radius: 3px;"
            "    background-color: #232323;"
            "}"
            "QTabWidget::pane {"
            "    background-color: #232323;"
            "    border: 1px solid #555555;"
            "    top: -1px;"
            "}"
            "QTabBar::tab {"
            "    background-color: #2a2a2a;"
            "    color: #e8e8e8;"
            "    border: 1px solid #555555;"
            "    padding: 6px 14px;"
            "    margin-right: 2px;"
            "}"
            "QTabBar::tab:selected {"
            "    background-color: #353535;"
            "    border-bottom-color: #232323;"
            "}"
            "QTabBar::tab:hover:!selected {"
            "    background-color: #3a3a3a;"
            "}"
            "QTextEdit {"
            "    background-color: #232323;"
            "    color: #e8e8e8;"
            "    border: 1px solid #555555;"
            "    selection-background-color: #2a82da;"
            "}"
            "QScrollBar:vertical {"
            "    background-color: #2a2a2a;"
            "    width: 12px;"
            "    border: none;"
            "}"
            "QScrollBar::handle:vertical {"
            "    background-color: #555555;"
            "    min-height: 20px;"
            "    border-radius: 6px;"
            "    margin: 2px;"
            "}"
            "QScrollBar::handle:vertical:hover { background-color: #666666; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
            "QScrollBar:horizontal {"
            "    background-color: #2a2a2a;"
            "    height: 12px;"
            "    border: none;"
            "}"
            "QScrollBar::handle:horizontal {"
            "    background-color: #555555;"
            "    min-width: 20px;"
            "    border-radius: 6px;"
            "    margin: 2px;"
            "}"
            "QScrollBar::handle:horizontal:hover { background-color: #666666; }"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }"
        );
    } else {
        setStyleSheet(QString());
    }

    if (m_fontSizeSpinBox && m_fontSizeStepWidget) {
        m_fontSizeSpinBox->setButtonSymbols(dark ? QAbstractSpinBox::NoButtons
                                                 : QAbstractSpinBox::UpDownArrows);
        m_fontSizeStepWidget->setVisible(dark);
        syncFontSizeControlLayout();
        if (dark) {
            QTimer::singleShot(0, this, &SettingsWidget::syncFontSizeControlLayout);
        }
    }
}

void SettingsWidget::syncFontSizeControlLayout()
{
    if (!m_fontSizeSpinBox || !m_fontSizeStepWidget || !m_fontSizeIncBtn || !m_fontSizeDecBtn) {
        return;
    }

    if (m_currentTheme != QLatin1String("dark")) {
        m_fontSizeSpinBox->setMinimumHeight(0);
        m_fontSizeSpinBox->setMaximumHeight(QWIDGETSIZE_MAX);
        m_fontSizeStepWidget->setMinimumHeight(0);
        m_fontSizeStepWidget->setMaximumHeight(QWIDGETSIZE_MAX);
        m_fontSizeIncBtn->setMinimumSize(0, 0);
        m_fontSizeDecBtn->setMinimumSize(0, 0);
        m_fontSizeIncBtn->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        m_fontSizeDecBtn->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        return;
    }

    const int controlHeight = qMax(m_fontSizeSpinBox->sizeHint().height(), 24);
    m_fontSizeSpinBox->setFixedHeight(controlHeight);
    m_fontSizeStepWidget->setFixedHeight(controlHeight);

    const int topHeight = controlHeight / 2;
    const int bottomHeight = controlHeight - topHeight;
    m_fontSizeIncBtn->setFixedSize(20, topHeight);
    m_fontSizeDecBtn->setFixedSize(20, bottomHeight);
}

void SettingsWidget::setupUi()
{
    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Контейнер для правой части (настройки + кнопка закрытия)
    auto *rightContainer = new QWidget(this);
    auto *rightContainerLayout = new QVBoxLayout(rightContainer);
    rightContainerLayout->setContentsMargins(0, 0, 0, 0);
    rightContainerLayout->setSpacing(0);
    
    // Кнопка закрытия в правом верхнем углу
    m_closeButton = new QPushButton(tr("×"), this);
    m_closeButton->setObjectName(QStringLiteral("settingsCloseButton"));
    m_closeButton->setFixedSize(30, 30);
    connect(m_closeButton, &QPushButton::clicked, this, &SettingsWidget::closeRequested);
    
    // Контейнер для кнопки закрытия (выравнивание по правому краю)
    auto *closeButtonContainer = new QWidget(rightContainer);
    auto *closeButtonLayout = new QHBoxLayout(closeButtonContainer);
    closeButtonLayout->setContentsMargins(0, 0, 5, 5);
    closeButtonLayout->setSpacing(0);
    closeButtonLayout->addStretch();
    closeButtonLayout->addWidget(m_closeButton);
    
    rightContainerLayout->addWidget(closeButtonContainer);
    
    // Левая панель с категориями
    m_categoryList = new QListWidget(this);
    m_categoryList->setObjectName(QStringLiteral("settingsCategoryList"));
    m_categoryList->setFixedWidth(150);
    m_categoryList->addItem(tr("Интерфейс"));
    m_categoryList->addItem(tr("Интерпретатор"));
    m_categoryList->addItem(tr("Синхронизация"));
    m_categoryList->addItem(tr("Менеджер пакетов"));
    m_categoryList->addItem(tr("Горячие клавиши"));
    m_categoryList->addItem(tr("Ассоциации"));
    m_categoryList->addItem(tr("Менеджер расширений"));
    m_agentCategoryItem = new QListWidgetItem(tr("Агент"));
    m_agentCategoryItem->setData(Qt::UserRole, QStringLiteral("agent"));
    m_categoryList->addItem(m_agentCategoryItem);
    m_categoryList->setCurrentRow(0);
    connect(m_categoryList, &QListWidget::currentRowChanged, this, &SettingsWidget::onCategoryChanged);
    
    // Правая панель со страницами настроек
    m_settingsStack = new QStackedWidget(rightContainer);
    
    // Страница "Интерфейс"
    m_interfacePage = new QWidget(m_settingsStack);
    auto *interfaceLayout = new QVBoxLayout(m_interfacePage);
    interfaceLayout->setContentsMargins(20, 20, 20, 20);
    interfaceLayout->setSpacing(15);
    
    // Размер текста в редакторе
    auto *fontSizeLabel = new QLabel(tr("Размер текста в редакторе:"), m_interfacePage);
    m_fontSizeSpinBox = new QSpinBox(m_interfacePage);
    m_fontSizeSpinBox->setObjectName(QStringLiteral("fontSizeSpinBox"));
    m_fontSizeSpinBox->setRange(6, 72);
    m_fontSizeSpinBox->setSuffix(tr(" пт"));
    connect(m_fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &SettingsWidget::onFontSizeChanged);

    m_fontSizeStepWidget = new QWidget(m_interfacePage);
    m_fontSizeStepWidget->setObjectName(QStringLiteral("fontSizeStepWidget"));
    auto *fontSizeStepLayout = new QVBoxLayout(m_fontSizeStepWidget);
    fontSizeStepLayout->setContentsMargins(0, 0, 0, 0);
    fontSizeStepLayout->setSpacing(0);

    m_fontSizeIncBtn = new QPushButton(QStringLiteral("+"), m_fontSizeStepWidget);
    m_fontSizeDecBtn = new QPushButton(QStringLiteral("-"), m_fontSizeStepWidget);
    m_fontSizeIncBtn->setObjectName(QStringLiteral("fontSizeIncButton"));
    m_fontSizeDecBtn->setObjectName(QStringLiteral("fontSizeDecButton"));
    fontSizeStepLayout->addWidget(m_fontSizeIncBtn);
    fontSizeStepLayout->addWidget(m_fontSizeDecBtn);

    connect(m_fontSizeIncBtn, &QPushButton::clicked, this, [this]() {
        m_fontSizeSpinBox->setValue(qMin(m_fontSizeSpinBox->maximum(), m_fontSizeSpinBox->value() + 1));
    });
    connect(m_fontSizeDecBtn, &QPushButton::clicked, this, [this]() {
        m_fontSizeSpinBox->setValue(qMax(m_fontSizeSpinBox->minimum(), m_fontSizeSpinBox->value() - 1));
    });

    auto *fontSizeControl = new QWidget(m_interfacePage);
    fontSizeControl->setObjectName(QStringLiteral("fontSizeControl"));
    auto *fontSizeControlLayout = new QHBoxLayout(fontSizeControl);
    fontSizeControlLayout->setContentsMargins(0, 0, 0, 0);
    fontSizeControlLayout->setSpacing(0);
    fontSizeControlLayout->setAlignment(Qt::AlignVCenter);
    fontSizeControlLayout->addWidget(m_fontSizeSpinBox);
    fontSizeControlLayout->addWidget(m_fontSizeStepWidget);
    
    auto *fontSizeLayout = new QHBoxLayout();
    fontSizeLayout->addWidget(fontSizeLabel);
    fontSizeLayout->addWidget(fontSizeControl);
    fontSizeLayout->addStretch();
    
    interfaceLayout->addLayout(fontSizeLayout);
    
    // Закрытие вкладок средней кнопкой мыши
    m_closeTabWithMiddleButtonCheckBox = new QCheckBox(tr("Закрытие вкладок средней кнопкой мыши"), m_interfacePage);
    QSettings settings;
    bool closeWithMiddleButton = settings.value("interface/closeTabWithMiddleButton", false).toBool();
    m_closeTabWithMiddleButtonCheckBox->setChecked(closeWithMiddleButton);
    connect(m_closeTabWithMiddleButtonCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        QSettings settings;
        settings.setValue("interface/closeTabWithMiddleButton", checked);
    });
    interfaceLayout->addWidget(m_closeTabWithMiddleButtonCheckBox);
    
    // Отображать кнопку закрытия на вкладке
    m_showCloseButtonOnTabsCheckBox = new QCheckBox(tr("Отображать кнопку закрытия на вкладке"), m_interfacePage);
    bool showCloseButton = settings.value("interface/showCloseButtonOnTabs", false).toBool();
    m_showCloseButtonOnTabsCheckBox->setChecked(showCloseButton);
    connect(m_showCloseButtonOnTabsCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        QSettings settings;
        settings.setValue("interface/showCloseButtonOnTabs", checked);
        emit showCloseButtonOnTabsChanged(checked);
    });
    interfaceLayout->addWidget(m_showCloseButtonOnTabsCheckBox);
    
    interfaceLayout->addStretch();
    
    m_settingsStack->addWidget(m_interfacePage);
    
    // Страница "Интерпретатор"
    m_interpreterPage = new QWidget(m_settingsStack);
    auto *interpreterLayout = new QVBoxLayout(m_interpreterPage);
    interpreterLayout->setContentsMargins(20, 20, 20, 20);
    interpreterLayout->setSpacing(15);
    
    // === Управление интерпретаторами ===
    auto *interpreterManagementGroup = new QGroupBox(tr("Управление интерпретаторами"), m_interpreterPage);
    auto *managementLayout = new QVBoxLayout(interpreterManagementGroup);
    
    // Выбор установленного интерпретатора
    auto *selectLayout = new QHBoxLayout();
    auto *selectLabel = new QLabel(tr("Установленные интерпретаторы:"), interpreterManagementGroup);
    m_interpreterComboBox = new QComboBox(interpreterManagementGroup);
    m_interpreterComboBox->setMinimumWidth(200);
    m_removeInterpreterBtn = new QPushButton(tr("Удалить"), interpreterManagementGroup);
    m_removeInterpreterBtn->setEnabled(false);
    selectLayout->addWidget(selectLabel);
    selectLayout->addWidget(m_interpreterComboBox, 1);
    selectLayout->addWidget(m_removeInterpreterBtn);
    managementLayout->addLayout(selectLayout);
    
    // Установка нового интерпретатора
    auto *installLayout = new QHBoxLayout();
    auto *installLabel = new QLabel(tr("Установить Python:"), interpreterManagementGroup);
    m_pythonVersionComboBox = new QComboBox(interpreterManagementGroup);
    m_pythonVersionComboBox->setMinimumWidth(150);
    m_installInterpreterBtn = new QPushButton(tr("Установить"), interpreterManagementGroup);
    installLayout->addWidget(installLabel);
    installLayout->addWidget(m_pythonVersionComboBox);
    installLayout->addWidget(m_installInterpreterBtn);
    installLayout->addStretch();
    managementLayout->addLayout(installLayout);
    
    // Текущий активный интерпретатор
    m_currentInterpreterLabel = new QLabel(interpreterManagementGroup);
    m_currentInterpreterLabel->setWordWrap(true);
    managementLayout->addWidget(m_currentInterpreterLabel);
    
    interpreterLayout->addWidget(interpreterManagementGroup);
    
    // Подключаем сигналы
    connect(m_interpreterComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsWidget::onInterpreterSelected);
    connect(m_installInterpreterBtn, &QPushButton::clicked, this, &SettingsWidget::onInstallInterpreter);
    connect(m_removeInterpreterBtn, &QPushButton::clicked, this, &SettingsWidget::onRemoveInterpreter);
    
    // Заполняем список версий Python для установки
    QStringList pythonVersions = getAvailablePythonVersions();
    m_pythonVersionComboBox->addItems(pythonVersions);
    
    // Загружаем список установленных интерпретаторов
    loadInstalledInterpreters();
    
    // Группа радио-кнопок для выбора типа интерпретатора
    auto *interpreterTypeGroup = new QGroupBox(tr("Тип интерпретатора"), m_interpreterPage);
    auto *interpreterTypeLayout = new QVBoxLayout(interpreterTypeGroup);
    
    m_interpreterTypeGroup = new QButtonGroup(m_interpreterPage);
    
    m_embeddedRadio = new QRadioButton(tr("Встроенный (Python38)"), m_interpreterPage);
    m_installedRadio = new QRadioButton(tr("Установленный (из списка выше)"), m_interpreterPage);
    m_systemRadio = new QRadioButton(tr("Системный"), m_interpreterPage);
    m_customRadio = new QRadioButton(tr("Выбрать путь вручную"), m_interpreterPage);
    
    m_interpreterTypeGroup->addButton(m_embeddedRadio, 0);
    m_interpreterTypeGroup->addButton(m_installedRadio, 1);
    m_interpreterTypeGroup->addButton(m_systemRadio, 2);
    m_interpreterTypeGroup->addButton(m_customRadio, 3);
    
    connect(m_interpreterTypeGroup, QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, &SettingsWidget::onInterpreterTypeChanged);
    
    interpreterTypeLayout->addWidget(m_embeddedRadio);
    interpreterTypeLayout->addWidget(m_installedRadio);
    interpreterTypeLayout->addWidget(m_systemRadio);
    interpreterTypeLayout->addWidget(m_customRadio);
    
    interpreterLayout->addWidget(interpreterTypeGroup);
    
    // Поле для пути к Python (только для "Выбрать путь")
    auto *pathLayout = new QHBoxLayout();
    auto *pathLabel = new QLabel(tr("Путь к интерпретатору:"), m_interpreterPage);
    m_pythonPathEdit = new QLineEdit(m_interpreterPage);
    m_pythonPathEdit->setPlaceholderText(tr("Выберите путь к python.exe"));
    m_pythonPathEdit->setEnabled(false);
    
    m_browseButton = new QPushButton(tr("Обзор..."), m_interpreterPage);
    m_browseButton->setEnabled(false);
    connect(m_browseButton, &QPushButton::clicked, this, &SettingsWidget::onBrowsePythonPath);
    
    pathLayout->addWidget(pathLabel);
    pathLayout->addWidget(m_pythonPathEdit, 1);
    pathLayout->addWidget(m_browseButton);
    
    interpreterLayout->addLayout(pathLayout);
    
    // Список установленных библиотек
    auto *packagesHeaderLayout = new QHBoxLayout();
    packagesHeaderLayout->addStretch();
    m_refreshPackagesButton = new QPushButton(tr("Обновить список"), m_interpreterPage);
    packagesHeaderLayout->addWidget(m_refreshPackagesButton);
    interpreterLayout->addLayout(packagesHeaderLayout);
    
    m_installedPackagesList = new QListWidget(m_interpreterPage);
    m_installedPackagesList->setUniformItemSizes(true);
    interpreterLayout->addWidget(m_installedPackagesList);
    
    connect(m_refreshPackagesButton, &QPushButton::clicked,
            this, &SettingsWidget::refreshInstalledPackages);

    auto *libraryButtonsLayout = new QHBoxLayout();
    m_installPyrobButton = new QPushButton(tr("Установить pyrob"), m_interpreterPage);
    m_installTurtlexyButton = new QPushButton(tr("Установить turtlexy"), m_interpreterPage);
    libraryButtonsLayout->addWidget(m_installPyrobButton);
    libraryButtonsLayout->addWidget(m_installTurtlexyButton);
    libraryButtonsLayout->addStretch();
    interpreterLayout->addLayout(libraryButtonsLayout);

    connect(m_installPyrobButton, &QPushButton::clicked, this, &SettingsWidget::onInstallPyrobFromEmbedded);
    connect(m_installTurtlexyButton, &QPushButton::clicked, this, &SettingsWidget::onInstallTurtlexyFromEmbedded);

    interpreterLayout->addStretch();
    
    m_settingsStack->addWidget(m_interpreterPage);

    setupSyncPage();
    m_settingsStack->addWidget(m_syncPage);
    
    // Страница "Менеджер пакетов"
    m_packageManagerWidget = new PackageManagerWidget(this);
    m_settingsStack->addWidget(m_packageManagerWidget);
    
    // Страница "Горячие клавиши"
    setupShortcutsPage();
    m_settingsStack->addWidget(m_shortcutsPage);
    
    // Страница "Ассоциации"
    setupAssociationsPage();
    m_settingsStack->addWidget(m_associationsPage);

    setupExtensionsPage();
    m_settingsStack->addWidget(m_extensionsPage);

    setupAgentPage();
    m_settingsStack->addWidget(m_agentPage);
    
    rightContainerLayout->addWidget(m_settingsStack, 1);
    
    mainLayout->addWidget(m_categoryList);
    mainLayout->addWidget(rightContainer, 1);

    QSettings themeSettings;
    setTheme(themeSettings.value(QStringLiteral("theme"), QStringLiteral("light")).toString());

    loadSyncSettings();
    updateSyncUi();
    updateSyncInterpreterLock();
    applyTeacherSyncServer();
    loadAgentSettings();
}

void SettingsWidget::onCategoryChanged(int index)
{
    if (index >= 0 && index < m_settingsStack->count()) {
        const int previousIndex = m_settingsStack->currentIndex();

        if (index == syncCategoryIndex() && !tryUnlockSyncPage()) {
            QSignalBlocker blocker(m_categoryList);
            m_categoryList->setCurrentRow(previousIndex);
            return;
        }

        if (m_agentCategoryItem && m_agentCategoryItem->isHidden()
            && index == agentCategoryIndex()) {
            QSignalBlocker blocker(m_categoryList);
            m_categoryList->setCurrentRow(previousIndex);
            return;
        }

        if (previousIndex == syncCategoryIndex() && index != syncCategoryIndex()) {
            lockSyncTabIfPasswordProtected();
        }

        m_settingsStack->setCurrentIndex(index);
        
        if (m_settingsStack->widget(index) == m_interpreterPage) {
            ensurePackagesInitialized();
        }

        if (index == syncCategoryIndex()) {
            populateSyncInterpreterCombo();
            updateSyncUi();
        }
        
        // Обновляем список пакетов при открытии менеджера пакетов
        if (m_settingsStack->widget(index) == m_packageManagerWidget) {
            if (m_packageManagerWidget) {
                m_packageManagerWidget->refreshInstalledPackages();
            }
        }
    }
}

void SettingsWidget::setAgentFeatureVisible(bool visible)
{
    if (m_agentCategoryItem) {
        m_agentCategoryItem->setHidden(!visible);
    }
    if (!visible && m_categoryList && m_settingsStack
        && m_categoryList->currentRow() == agentCategoryIndex()) {
        QSignalBlocker blocker(m_categoryList);
        m_categoryList->setCurrentRow(0);
        m_settingsStack->setCurrentIndex(0);
    }
}

QMap<QString, QKeySequence> SettingsWidget::getShortcuts() const
{
    return m_currentShortcuts;
}

void SettingsWidget::onFontSizeChanged(int size)
{
    // Сохраняем настройку
    QSettings settings;
    settings.setValue("editor/fontSize", size);
    
    // Отправляем сигнал об изменении
    emit fontSizeChanged(size);
}

void SettingsWidget::loadInterpreterSettings()
{
    QSettings settings;
    settings.beginGroup("runtime");
    
    // Загружаем тип интерпретатора (0=встроенный, 1=установленный, 2=системный, 3=пользовательский)
    int type = settings.value("interpreterType", 0).toInt();
    
    // Загружаем путь к Python
    QString path = settings.value("pythonPath").toString();
    QString selectedInterpreter = settings.value("selectedInterpreter").toString();
    
    settings.endGroup();
    
    // Устанавливаем выбранную радио-кнопку
    if (type == 0) {
        m_embeddedRadio->setChecked(true);
    } else if (type == 1) {
        m_installedRadio->setChecked(true);
        // Выбираем интерпретатор из комбобокса
        for (int i = 0; i < m_interpreterComboBox->count(); ++i) {
            if (m_interpreterComboBox->itemData(i).toString() == selectedInterpreter) {
                m_interpreterComboBox->setCurrentIndex(i);
                break;
            }
        }
    } else if (type == 2) {
        m_systemRadio->setChecked(true);
    } else {
        m_customRadio->setChecked(true);
    }
    
    // Устанавливаем путь
    m_pythonPathEdit->setText(path);
    
    // Включаем/выключаем поле пути в зависимости от выбора
    onInterpreterTypeChanged();
}

void SettingsWidget::saveInterpreterSettings()
{
    QSettings settings;
    settings.beginGroup("runtime");
    
    // Сохраняем тип интерпретатора (0=встроенный, 1=установленный, 2=системный, 3=пользовательский)
    int type = 0;
    if (m_embeddedRadio->isChecked()) {
        type = 0;
    } else if (m_installedRadio->isChecked()) {
        type = 1;
    } else if (m_systemRadio->isChecked()) {
        type = 2;
    } else {
        type = 3;
    }
    settings.setValue("interpreterType", type);
    
    // Сохраняем выбранный установленный интерпретатор
    if (type == 1) {
        QString selectedPath = m_interpreterComboBox->currentData().toString();
        settings.setValue("selectedInterpreter", selectedPath);
    }
    
    // Сохраняем путь к Python (только если выбран пользовательский)
    if (type == 3) {
        settings.setValue("pythonPath", m_pythonPathEdit->text());
    }
    
    settings.endGroup();
}

void SettingsWidget::onInterpreterTypeChanged()
{
    // Включаем/выключаем поле пути и кнопку "Обзор" в зависимости от выбора
    bool isCustom = m_customRadio->isChecked();
    bool isInstalled = m_installedRadio->isChecked();
    
    m_pythonPathEdit->setEnabled(isCustom);
    m_browseButton->setEnabled(isCustom);
    m_interpreterComboBox->setEnabled(isInstalled);
    
    // Сохраняем настройки
    saveInterpreterSettings();
    
    // Отправляем сигнал об изменении
    emit interpreterChanged();

    m_currentInterpreterLabel->setText(tr("Текущий интерпретатор: %1").arg(resolvePythonExecutable()));
    updatePackageUiEnabled();
    schedulePackagesRefresh();
}

void SettingsWidget::onBrowsePythonPath()
{
    QString startDir;
    if (!m_pythonPathEdit->text().isEmpty()) {
        QFileInfo info(m_pythonPathEdit->text());
        startDir = info.absolutePath();
    } else {
        // Пытаемся найти Python в стандартных местах
        const QString appDir = QCoreApplication::applicationDirPath();
        const QString embeddedPython = QDir(appDir).filePath("python/python.exe");
        if (QFile::exists(embeddedPython)) {
            startDir = QFileInfo(embeddedPython).absolutePath();
        } else {
            startDir = QDir::homePath();
        }
    }
    
    QString path = QFileDialog::getOpenFileName(
        this, tr("Выбрать интерпретатор Python"), startDir,
        tr("Исполняемые (*.exe);;Все файлы (*.*)"));
    
    if (!path.isEmpty()) {
        m_pythonPathEdit->setText(path);
        saveInterpreterSettings();
        emit interpreterChanged();
        updatePackageUiEnabled();
        schedulePackagesRefresh();
    }
}

void SettingsWidget::refreshInstalledPackages()
{
    m_packagesInitialized = true;
    m_packagesNeedsRefresh = false;
    
    const QString python = resolvePythonExecutable();
    if (python.isEmpty() || !QFile::exists(python)) {
        updatePackageUiEnabled();
        return;
    }
    
    m_installedPackagesList->clear();
    
    QByteArray stdOut;
    QByteArray stdErr;
    if (!runPythonProcess({"-m", "pip", "list", "--format=json"}, stdOut, stdErr)) {
        return;
    }
    
    populateInstalledPackagesList(stdOut);
}

void SettingsWidget::ensurePackagesInitialized()
{
    if (!m_packagesInitialized || m_packagesNeedsRefresh) {
        refreshInstalledPackages();
    }
}

void SettingsWidget::schedulePackagesRefresh()
{
    if (isInterpreterPageVisible()) {
        refreshInstalledPackages();
    } else {
        m_packagesNeedsRefresh = true;
    }
}

bool SettingsWidget::isInterpreterPageVisible() const
{
    return m_settingsStack && m_settingsStack->currentWidget() == m_interpreterPage;
}

QString SettingsWidget::resolvePythonExecutable() const
{
    if (m_embeddedRadio) {
        if (m_embeddedRadio->isChecked()) {
            const QString appDir = QCoreApplication::applicationDirPath();
            const QString embedded = QDir(appDir).filePath("Python38/python.exe");
            if (QFile::exists(embedded)) {
                return embedded;
            }
            return QStringLiteral("python.exe");
        }

        if (m_installedRadio->isChecked() && m_interpreterComboBox) {
            const QString path = m_interpreterComboBox->currentData().toString();
            if (!path.isEmpty() && QFile::exists(path)) {
                return path;
            }
        }

        if (m_systemRadio->isChecked()) {
            return QStringLiteral("python.exe");
        }

        if (m_customRadio->isChecked() && m_pythonPathEdit) {
            const QString path = m_pythonPathEdit->text();
            if (QFile::exists(path)) {
                return path;
            }
            return {};
        }
    }

    QSettings settings;
    settings.beginGroup("runtime");
    const int type = settings.value("interpreterType", 0).toInt();
    const QString customPath = settings.value("pythonPath").toString();
    const QString selectedInterpreter = settings.value("selectedInterpreter").toString();
    settings.endGroup();

    if (type == 3) {
        if (QFile::exists(customPath)) {
            return customPath;
        }
        return {};
    }

    if (type == 1) {
        if (!selectedInterpreter.isEmpty() && QFile::exists(selectedInterpreter)) {
            return selectedInterpreter;
        }

        const QString interpretersDir = QCoreApplication::applicationDirPath() + "/interpreters";
        QDir dir(interpretersDir);
        if (dir.exists()) {
            const QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            if (!subdirs.isEmpty()) {
                const QString pythonExe = dir.filePath(subdirs.first() + "/python.exe");
                if (QFile::exists(pythonExe)) {
                    return pythonExe;
                }
            }
        }
    }

    if (type == 0 || type == 1) {
        const QString appDir = QCoreApplication::applicationDirPath();
        const QString embedded = QDir(appDir).filePath("Python38/python.exe");
        if (QFile::exists(embedded)) {
            return embedded;
        }
    }

    if (type == 2) {
        return QStringLiteral("python.exe");
    }

    return QStringLiteral("python.exe");
}

bool SettingsWidget::runPythonProcess(const QStringList &arguments, QByteArray &stdOutput, QByteArray &stdError, bool showErrors)
{
    const QString python = resolvePythonExecutable();
    if (python.isEmpty() || !QFile::exists(python)) {
        if (showErrors) {
            QMessageBox::warning(this, tr("Python не найден"),
                                 tr("Не удалось найти интерпретатор Python. Проверьте настройки интерпретатора."));
        }
        updatePackageUiEnabled();
        return false;
    }
    
    QProcess process;
    process.setProgram(python);
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    
    process.start();
    if (!process.waitForFinished(60000)) {
        process.kill();
        if (showErrors) {
            QMessageBox::warning(this, tr("Таймаут"),
                                 tr("Команда pip выполняется слишком долго и была остановлена."));
        }
        return false;
    }
    
    stdOutput = process.readAllStandardOutput();
    stdError = process.readAllStandardError();
    
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QString errorText = QString::fromUtf8(stdError).trimmed();
        if (errorText.isEmpty())
            errorText = tr("Неизвестная ошибка при выполнении команды pip.");
        if (showErrors) {
            QMessageBox::warning(this, tr("Ошибка pip"), errorText);
        }
        return false;
    }
    
    return true;
}

void SettingsWidget::updatePackageUiEnabled()
{
    const QString python = resolvePythonExecutable();
    const bool available = !python.isEmpty() && QFile::exists(python);
    const bool locked = isStudentSyncEnabled();
    
    if (m_refreshPackagesButton) m_refreshPackagesButton->setEnabled(available && !locked);
    if (m_installedPackagesList) m_installedPackagesList->setEnabled(available && !locked);
    if (m_installPyrobButton) m_installPyrobButton->setEnabled(available && !locked);
    if (m_installTurtlexyButton) m_installTurtlexyButton->setEnabled(available && !locked);
    
    if (!available) {
        if (m_installedPackagesList) m_installedPackagesList->clear();
    }

    updateSyncInterpreterLock();
}

void SettingsWidget::populateInstalledPackagesList(const QByteArray &jsonData)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        QMessageBox::warning(this, tr("Ошибка разбора"),
                             tr("Не удалось разобрать вывод pip list."));
        return;
    }
    
    const QJsonArray array = doc.array();
    QStringList packages;
    packages.reserve(array.size());
    for (const QJsonValue &value : array) {
        if (!value.isObject())
            continue;
        const QJsonObject obj = value.toObject();
        const QString name = obj.value(QStringLiteral("name")).toString();
        const QString version = obj.value(QStringLiteral("version")).toString();
        if (!name.isEmpty()) {
            if (!version.isEmpty())
                packages << QString("%1 %2").arg(name, version);
            else
                packages << name;
        }
    }
    packages.sort(Qt::CaseInsensitive);
    
    for (const QString &pkg : packages) {
        m_installedPackagesList->addItem(pkg);
    }
}

QString SettingsWidget::embeddedSitePackagesDir() const
{
    return PythonInterpreterPaths::embeddedSitePackagesDir(QCoreApplication::applicationDirPath());
}

QString SettingsWidget::querySitePackagesDir(const QString &pythonExe, QString *errorMessage) const
{
    return PythonInterpreterPaths::sitePackagesDir(pythonExe, false, errorMessage);
}

bool SettingsWidget::queryPythonVersion(const QString &pythonExe, int *major, int *minor, QString *errorMessage) const
{
    QProcess process;
    process.setProgram(pythonExe);
    process.setArguments(QStringList()
        << QStringLiteral("-c")
        << QStringLiteral("import sys; print(sys.version_info[0]); print(sys.version_info[1])"));
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();

    if (!process.waitForFinished(30000)) {
        process.kill();
        if (errorMessage) {
            *errorMessage = tr("Не удалось определить версию Python.");
        }
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(process.readAllStandardError()).trimmed();
            if (errorMessage->isEmpty()) {
                *errorMessage = tr("Не удалось определить версию Python.");
            }
        }
        return false;
    }

    const QStringList lines = QString::fromUtf8(process.readAllStandardOutput())
                                  .split(QLatin1Char('\n'), QString::SkipEmptyParts);
    if (lines.size() < 2) {
        if (errorMessage) {
            *errorMessage = tr("Не удалось определить версию Python.");
        }
        return false;
    }

    bool majorOk = false;
    bool minorOk = false;
    const int parsedMajor = lines[0].trimmed().toInt(&majorOk);
    const int parsedMinor = lines[1].trimmed().toInt(&minorOk);
    if (!majorOk || !minorOk) {
        if (errorMessage) {
            *errorMessage = tr("Не удалось определить версию Python.");
        }
        return false;
    }

    if (major) {
        *major = parsedMajor;
    }
    if (minor) {
        *minor = parsedMinor;
    }
    return true;
}

bool SettingsWidget::copyPathRecursively(const QString &srcPath, const QString &dstPath) const
{
    const QFileInfo srcInfo(srcPath);
    if (!srcInfo.exists()) {
        return false;
    }

    if (srcInfo.isDir()) {
        QDir dstDir(dstPath);
        if (!dstDir.exists() && !QDir().mkpath(dstPath)) {
            return false;
        }

        const QDir srcDir(srcPath);
        const QFileInfoList entries = srcDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
        for (const QFileInfo &entry : entries) {
            if (!copyPathRecursively(entry.absoluteFilePath(), dstDir.filePath(entry.fileName()))) {
                return false;
            }
        }
        return true;
    }

    if (QFile::exists(dstPath)) {
        QFile::setPermissions(dstPath, QFile::WriteUser | QFile::ReadUser | QFile::WriteOwner | QFile::ReadOwner);
        if (!QFile::remove(dstPath)) {
            return false;
        }
    }

    return QFile::copy(srcPath, dstPath);
}

void SettingsWidget::installEmbeddedLibrary(const QString &packagePrefix, const QString &displayName)
{
    const QString pythonExe = resolvePythonExecutable();
    if (pythonExe.isEmpty() || !QFile::exists(pythonExe)) {
        QMessageBox::warning(this, tr("Python не найден"),
                             tr("Не удалось найти интерпретатор Python. Проверьте настройки интерпретатора."));
        return;
    }

    int major = 0;
    int minor = 0;
    QString versionError;
    if (!queryPythonVersion(pythonExe, &major, &minor, &versionError)) {
        QMessageBox::warning(this, tr("Ошибка"), versionError);
        return;
    }

    if (major < 3 || (major == 3 && minor < 8)) {
        QMessageBox::warning(this, tr("Версия Python не подходит"),
                             tr("Для установки %1 требуется Python 3.8 или выше.\n\n"
                                "Текущая версия: Python %2.%3")
                                 .arg(displayName)
                                 .arg(major)
                                 .arg(minor));
        return;
    }

    const QDir sourceDir(embeddedSitePackagesDir());
    if (!sourceDir.exists()) {
        QMessageBox::critical(this, tr("Ошибка"),
                              tr("Не найдена папка site-packages во встроенном интерпретаторе."));
        return;
    }

    QString sitePackagesError;
    const QString targetSitePackages = PythonInterpreterPaths::sitePackagesDir(pythonExe, true, &sitePackagesError);
    if (targetSitePackages.isEmpty()) {
        QMessageBox::critical(this, tr("Ошибка"), sitePackagesError);
        return;
    }

    if (QDir(sourceDir.absolutePath()).absolutePath().compare(QDir(targetSitePackages).absolutePath(),
                                                              Qt::CaseInsensitive) == 0) {
        QMessageBox::information(this, tr("Уже установлено"),
                                 tr("%1 уже доступен во встроенном интерпретаторе.").arg(displayName));
        refreshInstalledPackages();
        return;
    }

    const QStringList entries = sourceDir.entryList(
        QStringList() << packagePrefix + QStringLiteral("*"),
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    if (entries.isEmpty()) {
        QMessageBox::critical(this, tr("Ошибка"),
                              tr("Пакет %1 не найден во встроенном интерпретаторе.").arg(displayName));
        return;
    }

    if (!QDir().mkpath(targetSitePackages)) {
        QMessageBox::critical(this, tr("Ошибка"),
                              tr("Не удалось создать папку site-packages:\n%1").arg(targetSitePackages));
        return;
    }

    QDir targetDir(targetSitePackages);
    const QStringList oldEntries = targetDir.entryList(
        QStringList() << packagePrefix + QStringLiteral("*"),
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    for (const QString &oldEntry : oldEntries) {
        const QString oldPath = targetDir.filePath(oldEntry);
        const QFileInfo oldInfo(oldPath);
        if (oldInfo.isDir()) {
            QDir(oldPath).removeRecursively();
        } else {
            QFile::setPermissions(oldPath, QFile::WriteUser | QFile::ReadUser | QFile::WriteOwner | QFile::ReadOwner);
            QFile::remove(oldPath);
        }
    }

    for (const QString &entry : entries) {
        const QString srcPath = sourceDir.filePath(entry);
        const QString dstPath = targetDir.filePath(entry);

        if (!copyPathRecursively(srcPath, dstPath)) {
            QMessageBox::critical(this, tr("Ошибка"),
                                  tr("Не удалось скопировать %1 в выбранный интерпретатор.").arg(entry));
            return;
        }
    }

    QMessageBox::information(this, tr("Успех"),
                             tr("%1 успешно установлен в выбранный интерпретатор.").arg(displayName));
    refreshInstalledPackages();
}

void SettingsWidget::onInstallPyrobFromEmbedded()
{
    installEmbeddedLibrary(QStringLiteral("pyrob"), tr("pyrob"));
}

void SettingsWidget::onInstallTurtlexyFromEmbedded()
{
    installEmbeddedLibrary(QStringLiteral("turtlexy"), tr("turtlexy"));
}

void SettingsWidget::setupShortcutsPage()
{
    m_shortcutsPage = new QWidget(m_settingsStack);
    auto *shortcutsLayout = new QVBoxLayout(m_shortcutsPage);
    shortcutsLayout->setContentsMargins(20, 20, 20, 20);
    shortcutsLayout->setSpacing(15);
    
    // Заголовок
    auto *titleLabel = new QLabel(tr("Горячие клавиши"), m_shortcutsPage);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    shortcutsLayout->addWidget(titleLabel);
    
    // Таблица горячих клавиш
    m_shortcutsTable = new QTableWidget(m_shortcutsPage);
    m_shortcutsTable->setColumnCount(3);
    m_shortcutsTable->setHorizontalHeaderLabels({tr("Действие"), tr("Горячая клавиша"), tr("")});
    m_shortcutsTable->horizontalHeader()->setStretchLastSection(false);
    m_shortcutsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_shortcutsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_shortcutsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_shortcutsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_shortcutsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_shortcutsTable->verticalHeader()->setVisible(false);
    m_shortcutsTable->setFrameShape(QFrame::NoFrame);
    m_shortcutsTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_shortcutsTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // Определяем список действий с их дефолтными горячими клавишами
    struct ShortcutInfo {
        QString actionName;
        QString displayName;
        QKeySequence defaultSequence;
    };
    
    QList<ShortcutInfo> shortcuts = {
        {"new", tr("Новый файл"), QKeySequence::New},
        {"open", tr("Открыть файл"), QKeySequence::Open},
        {"save", tr("Сохранить"), QKeySequence::Save},
        {"saveAs", tr("Сохранить как..."), QKeySequence::SaveAs},
        {"saveAll", tr("Сохранить все"), QKeySequence(QStringLiteral("Ctrl+Shift+S"))},
        {"run", tr("Запустить скрипт"), QKeySequence(Qt::Key_F5)},
        {"runInTerminal", tr("Запуск в отдельном окне консоли"), QKeySequence(Qt::Key_F6)},
        {"debug", tr("Запуск с отладкой"), QKeySequence(Qt::Key_F8)},
        {"commentLines", tr("Закомментировать строки"), QKeySequence(QStringLiteral("Alt+3"))},
        {"uncommentLines", tr("Раскомментировать строки"), QKeySequence(QStringLiteral("Alt+Ctrl+3"))},
    };
    
    m_shortcutsTable->setRowCount(shortcuts.size());
    
    for (int i = 0; i < shortcuts.size(); ++i) {
        const auto &info = shortcuts[i];
        
        // Название действия
        auto *nameItem = new QTableWidgetItem(info.displayName);
        nameItem->setData(Qt::UserRole, info.actionName); // Сохраняем имя действия
        m_shortcutsTable->setItem(i, 0, nameItem);
        
        // Редактор горячей клавиши
        QKeySequenceEdit *keyEdit = new QKeySequenceEdit(m_shortcutsPage);
        QKeySequence currentSeq = m_currentShortcuts.value(info.actionName, info.defaultSequence);
        keyEdit->setKeySequence(currentSeq);
        m_shortcutsTable->setCellWidget(i, 1, keyEdit);
        
        // Сохраняем дефолтное значение
        m_defaultShortcuts[info.actionName] = info.defaultSequence;
        
        // Кнопка сброса
        auto *resetButton = new QPushButton(tr("Сбросить"), m_shortcutsPage);
        resetButton->setObjectName(QStringLiteral("shortcutResetButton"));
        resetButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        resetButton->setProperty("row", i);
        resetButton->setProperty("actionName", info.actionName);
        connect(resetButton, &QPushButton::clicked, this, [this, resetButton]() {
            onResetShortcut(resetButton->property("row").toInt());
        });
        m_shortcutsTable->setCellWidget(i, 2, resetButton);
        
        // Подключаем сигнал изменения горячей клавиши
        connect(keyEdit, &QKeySequenceEdit::keySequenceChanged, this, [this, i, info]() {
            onShortcutChanged(i);
        });
    }
    
    auto *shortcutsTableFrame = new QFrame(m_shortcutsPage);
    shortcutsTableFrame->setFrameShape(QFrame::StyledPanel);
    shortcutsTableFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *shortcutsTableFrameLayout = new QVBoxLayout(shortcutsTableFrame);
    shortcutsTableFrameLayout->setContentsMargins(0, 0, 0, 0);
    shortcutsTableFrameLayout->addWidget(m_shortcutsTable);

    shortcutsLayout->addWidget(shortcutsTableFrame, 1);
}

void SettingsWidget::loadShortcutSettings()
{
    QSettings settings;
    settings.beginGroup("shortcuts");
    
    // Загружаем сохраненные горячие клавиши
    QStringList keys = settings.allKeys();
    for (const QString &key : keys) {
        QString sequenceStr = settings.value(key).toString();
        if (!sequenceStr.isEmpty()) {
            m_currentShortcuts[key] = QKeySequence(sequenceStr);
        }
    }
    
    settings.endGroup();
}

void SettingsWidget::saveShortcutSettings()
{
    QSettings settings;
    settings.beginGroup("shortcuts");
    
    // Сохраняем все текущие горячие клавиши
    for (auto it = m_currentShortcuts.begin(); it != m_currentShortcuts.end(); ++it) {
        if (it.value() != m_defaultShortcuts.value(it.key())) {
            // Сохраняем только если отличается от дефолтного
            settings.setValue(it.key(), it.value().toString());
        } else {
            // Удаляем из настроек, если вернулось к дефолтному
            settings.remove(it.key());
        }
    }
    
    settings.endGroup();
}

void SettingsWidget::onShortcutChanged(int row)
{
    QKeySequenceEdit *keyEdit = qobject_cast<QKeySequenceEdit*>(m_shortcutsTable->cellWidget(row, 1));
    if (!keyEdit) return;
    
    QTableWidgetItem *nameItem = m_shortcutsTable->item(row, 0);
    if (!nameItem) return;
    
    QString actionName = nameItem->data(Qt::UserRole).toString();
    QKeySequence newSequence = keyEdit->keySequence();
    
    // Обновляем текущую горячую клавишу
    m_currentShortcuts[actionName] = newSequence;
    
    // Сохраняем настройки
    saveShortcutSettings();
    
    // Отправляем сигнал об изменении
    emit shortcutChanged(actionName, newSequence);
}

void SettingsWidget::onResetShortcut(int row)
{
    QTableWidgetItem *nameItem = m_shortcutsTable->item(row, 0);
    if (!nameItem) return;
    
    QString actionName = nameItem->data(Qt::UserRole).toString();
    QKeySequence defaultSeq = m_defaultShortcuts.value(actionName);
    
    // Обновляем редактор
    QKeySequenceEdit *keyEdit = qobject_cast<QKeySequenceEdit*>(m_shortcutsTable->cellWidget(row, 1));
    if (keyEdit) {
        keyEdit->setKeySequence(defaultSeq);
    }
    
    // Обновляем текущую горячую клавишу
    m_currentShortcuts[actionName] = defaultSeq;
    
    // Сохраняем настройки
    saveShortcutSettings();
    
    // Отправляем сигнал об изменении
    emit shortcutChanged(actionName, defaultSeq);
}

void SettingsWidget::setupAssociationsPage()
{
    m_associationsPage = new QWidget(m_settingsStack);
    auto *associationsLayout = new QVBoxLayout(m_associationsPage);
    associationsLayout->setContentsMargins(20, 20, 20, 20);
    associationsLayout->setSpacing(15);
    
    // Заголовок
    auto *titleLabel = new QLabel(tr("Ассоциации файлов"), m_associationsPage);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    associationsLayout->addWidget(titleLabel);
    
    // Описание
    auto *descriptionLabel = new QLabel(
        tr("Выберите типы файлов, которые будут открываться в Vuzhyk по умолчанию."),
        m_associationsPage);
    descriptionLabel->setWordWrap(true);
    associationsLayout->addWidget(descriptionLabel);
    
    // Чекбокс для Python файлов
    m_pythonAssociationCheckBox = new QCheckBox(tr("Python (.py)"), m_associationsPage);
    
    // Загружаем текущее состояние ассоциации
    QSettings settings;
    bool pythonAssociated = settings.value("associations/python", false).toBool();
    m_pythonAssociationCheckBox->setChecked(pythonAssociated);
    
    connect(m_pythonAssociationCheckBox, &QCheckBox::toggled, 
            this, &SettingsWidget::onPythonAssociationChanged);
    
    associationsLayout->addWidget(m_pythonAssociationCheckBox);
    associationsLayout->addStretch();
}

void SettingsWidget::setupExtensionsPage()
{
    m_extensionsPage = new QWidget(m_settingsStack);
    auto *extensionsLayout = new QVBoxLayout(m_extensionsPage);
    extensionsLayout->setContentsMargins(20, 20, 20, 20);
    extensionsLayout->setSpacing(12);

    auto *headerLayout = new QHBoxLayout();
    auto *titleLabel = new QLabel(tr("Менеджер расширений"), m_extensionsPage);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    m_refreshExtensionsButton = new QPushButton(tr("Обновить"), m_extensionsPage);
    m_refreshExtensionsButton->setEnabled(false);
    connect(m_refreshExtensionsButton, &QPushButton::clicked, this, &SettingsWidget::onRefreshExtensions);
    headerLayout->addWidget(m_refreshExtensionsButton);
    extensionsLayout->addLayout(headerLayout);

    m_extensionsTable = new QTableWidget(m_extensionsPage);
    m_extensionsTable->setColumnCount(6);
    m_extensionsTable->setHorizontalHeaderLabels({
        tr("Название"),
        tr("Описание"),
        tr("Интерфейс"),
        tr("Путь"),
        tr("Включено"),
        tr("Действия")
    });
    m_extensionsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_extensionsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_extensionsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_extensionsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_extensionsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_extensionsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_extensionsTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_extensionsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_extensionsTable->setFocusPolicy(Qt::NoFocus);
    m_extensionsTable->verticalHeader()->setVisible(false);
    extensionsLayout->addWidget(m_extensionsTable, 1);
}

void SettingsWidget::onRefreshExtensions()
{
    if (m_extensionManager) {
        m_extensionManager->loadExtensions();
    }
}

void SettingsWidget::refreshExtensionsPage()
{
    if (!m_extensionsTable) {
        return;
    }

    m_extensionsTable->clearContents();
    if (!m_extensionManager) {
        m_extensionsTable->setRowCount(0);
        return;
    }

    const auto &extensions = m_extensionManager->extensions();
    m_extensionsTable->setRowCount(extensions.count());
    for (int row = 0; row < extensions.count(); ++row) {
        const ExtensionManager::ExtensionInfo &extension = extensions.at(row);
        QStringList uiItems;
        for (const ExtensionManager::ExtensionAction &action : extension.actions) {
            uiItems.append(QString("%1 (%2)").arg(action.label, action.location));
        }

        auto *nameItem = new QTableWidgetItem(!extension.name.isEmpty() ? extension.name : extension.id);
        auto *descriptionItem = new QTableWidgetItem(extension.description);
        auto *interfaceItem = new QTableWidgetItem(uiItems.join(", "));
        auto *pathItem = new QTableWidgetItem(extension.basePath);

        m_extensionsTable->setItem(row, 0, nameItem);
        m_extensionsTable->setItem(row, 1, descriptionItem);
        m_extensionsTable->setItem(row, 2, interfaceItem);
        m_extensionsTable->setItem(row, 3, pathItem);

        auto *enabledCheck = new QCheckBox(m_extensionsTable);
        const QString extensionId = extension.id;
        enabledCheck->blockSignals(true);
        enabledCheck->setChecked(extension.enabled);
        enabledCheck->blockSignals(false);
        connect(enabledCheck, &QCheckBox::toggled, this, [this, extensionId](bool checked) {
            if (m_extensionManager) {
                m_extensionManager->setExtensionEnabled(extensionId, checked);
            }
        });
        m_extensionsTable->setCellWidget(row, 4, enabledCheck);

        auto *deleteButton = new QPushButton(tr("Удалить"), m_extensionsTable);
        deleteButton->setProperty("extensionId", extension.id);
        connect(deleteButton, &QPushButton::clicked, this, [this, extensionId]() {
            if (m_extensionManager) {
                m_extensionManager->removeExtension(extensionId);
            }
        });
        m_extensionsTable->setCellWidget(row, 5, deleteButton);
    }
}

void SettingsWidget::setExtensionManager(ExtensionManager *manager)
{
    if (m_extensionManager == manager) {
        return;
    }

    if (m_extensionManager) {
        disconnect(m_extensionManager, &ExtensionManager::extensionsChanged,
                this, &SettingsWidget::refreshExtensionsPage);
    }

    m_extensionManager = manager;

    if (m_extensionManager) {
        connect(m_extensionManager, &ExtensionManager::extensionsChanged,
                this, &SettingsWidget::refreshExtensionsPage);
        refreshExtensionsPage();
    } else if (m_extensionsTable) {
        m_extensionsTable->setRowCount(0);
    }

    if (m_refreshExtensionsButton) {
        m_refreshExtensionsButton->setEnabled(m_extensionManager != nullptr);
    }
}

void SettingsWidget::onPythonAssociationChanged(bool checked)
{
    QSettings settings;
    settings.setValue("associations/python", checked);
    
    const QString extension = ".py";
    const QString progId = "Vuzhyk.Python";
    const QString description = tr("Файл Python");
    QString appPath = getApplicationPath();
    
    // Нормализуем путь к приложению (убираем лишние слеши)
    appPath = QDir::toNativeSeparators(QDir::cleanPath(appPath));
    
    if (checked) {
        // Устанавливаем ассоциацию
        // Формируем команду как в 7zip: "путь_к_exe" "%1"
        // Важно: путь к exe должен быть в кавычках, и %1 тоже в кавычках
        QString command = QString("\"%1\" \"%2\"").arg(appPath, "%1");
        if (setFileAssociation(extension, progId, description, command)) {
            emit showStatusMessage(
                tr("Ассоциация для .py файлов установлена. Vuzhyk появится в списке 'Открыть с помощью' после обновления проводника."), 
                5000);
        } else {
            m_pythonAssociationCheckBox->setChecked(false);
            settings.setValue("associations/python", false);
            emit showStatusMessage(tr("Не удалось установить ассоциацию. Требуются права администратора."), 5000);
        }
    } else {
        // Удаляем ассоциацию
        if (removeFileAssociation(extension)) {
            emit showStatusMessage(tr("Ассоциация для .py файлов удалена"), 3000);
        } else {
            emit showStatusMessage(tr("Не удалось удалить ассоциацию. Требуются права администратора."), 5000);
        }
    }
}

bool SettingsWidget::setFileAssociation(const QString &extension, const QString &progId, const QString &description, const QString &command)
{
#ifdef Q_OS_WIN
    // Реализация по образцу 7zip
    // Используем HKEY_CURRENT_USER\Software\Classes (не требует прав администратора)
    
    QString baseKey = QString("HKEY_CURRENT_USER\\Software\\Classes");
    QString appPath = getApplicationPath();
    
    // 1. Удаляем старую ассоциацию (если есть) - как в 7zip
    removeFileAssociation(extension);
    
    // 2. Создаем ключ расширения и связываем его с ProgID
    // .py -> Default = "Vuzhyk.Python"
    QSettings extSettings(baseKey + "\\" + extension, QSettings::NativeFormat);
    extSettings.setValue("Default", progId);
    
    // 2.1. Устанавливаем иконку для расширения .py
    // Извлекаем иконку из ресурсов во временный файл
    QString iconFilePath = extractIconFromResources();
    QString extIconPath = QString("\"%1\",0").arg(iconFilePath);
    QSettings extIconSettings(baseKey + "\\" + extension + "\\DefaultIcon", QSettings::NativeFormat);
    extIconSettings.setValue("Default", extIconPath);
    extIconSettings.sync();
    
    extSettings.sync();
    
    // 3. Создаем ProgID ключ с полной структурой (как в 7zip)
    QString progIdKey = baseKey + "\\" + progId;
    
    // 3.1. Устанавливаем описание программы
    QSettings progIdSettings(progIdKey, QSettings::NativeFormat);
    progIdSettings.setValue("Default", description);
    
    // 3.2. Устанавливаем иконку (используем ту же, что и для расширения)
    QString iconPath = QString("\"%1\",0").arg(iconFilePath);
    QSettings iconSettings(progIdKey + "\\DefaultIcon", QSettings::NativeFormat);
    iconSettings.setValue("Default", iconPath);
    iconSettings.sync();
    
    // 3.3. Создаем структуру shell\open\command (как в 7zip)
    QSettings shellSettings(progIdKey + "\\shell", QSettings::NativeFormat);
    shellSettings.setValue("Default", ""); // Пустое значение для shell
    shellSettings.sync();
    
    QSettings openSettings(progIdKey + "\\shell\\open", QSettings::NativeFormat);
    openSettings.setValue("Default", ""); // Пустое значение для open
    openSettings.sync();
    
    QSettings commandSettings(progIdKey + "\\shell\\open\\command", QSettings::NativeFormat);
    commandSettings.setValue("Default", command);
    commandSettings.sync();
    
    progIdSettings.sync();
    
    // 4. Регистрируем приложение в Applications для появления в "Открыть с помощью"
    QFileInfo appInfo(appPath);
    QString appName = appInfo.fileName();
    QString appKey = baseKey + "\\Applications\\" + appName;
    QSettings appSettings(appKey, QSettings::NativeFormat);
    appSettings.setValue("FriendlyAppName", "Vuzhyk");
    appSettings.setValue("shell/open/command/Default", command);
    appSettings.setValue("DefaultIcon/Default", iconPath);
    
    // 5. Добавляем расширение в список поддерживаемых расширений приложения
    QSettings appSupportedExtSettings(appKey + "\\SupportedTypes", QSettings::NativeFormat);
    appSupportedExtSettings.setValue(extension, "");
    appSupportedExtSettings.sync();
    
    // 6. Добавляем в OpenWithProgids для появления в "Открыть с помощью"
    QSettings openWithSettings(baseKey + "\\" + extension + "\\OpenWithProgids", QSettings::NativeFormat);
    openWithSettings.setValue(progId, "");
    openWithSettings.sync();
    
    appSettings.sync();
    
    // Уведомляем Windows об изменении ассоциаций файлов
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    
    return true;
#else
    Q_UNUSED(extension);
    Q_UNUSED(progId);
    Q_UNUSED(description);
    Q_UNUSED(command);
    return false;
#endif
}

bool SettingsWidget::removeFileAssociation(const QString &extension)
{
#ifdef Q_OS_WIN
    QString baseKey = QString("HKEY_CURRENT_USER\\Software\\Classes");
    const QString progId = "Vuzhyk.Python";
    QString appPath = getApplicationPath();
    QFileInfo appInfo(appPath);
    QString appName = appInfo.fileName();
    
    // 1. Удаляем ключ расширения полностью (как в 7zip)
    QSettings extSettings(baseKey + "\\" + extension, QSettings::NativeFormat);
    extSettings.clear(); // Удаляем весь ключ
    extSettings.sync();
    
    // 2. Удаляем ProgID ключ полностью (как в 7zip)
    QSettings progIdSettings(baseKey + "\\" + progId, QSettings::NativeFormat);
    progIdSettings.clear(); // Удаляем весь ключ
    progIdSettings.sync();
    
    // 3. Удаляем из OpenWithProgids
    QSettings openWithSettings(baseKey + "\\" + extension + "\\OpenWithProgids", QSettings::NativeFormat);
    openWithSettings.remove(progId);
    openWithSettings.sync();
    
    // 4. Удаляем расширение из списка поддерживаемых приложением
    QString appKey = baseKey + "\\Applications\\" + appName + "\\SupportedTypes";
    QSettings appSupportedExtSettings(appKey, QSettings::NativeFormat);
    appSupportedExtSettings.remove(extension);
    appSupportedExtSettings.sync();
    
    // Уведомляем Windows об изменении ассоциаций файлов
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    
    return true;
#else
    Q_UNUSED(extension);
    return false;
#endif
}

QString SettingsWidget::getApplicationPath() const
{
    return QCoreApplication::applicationFilePath();
}

QString SettingsWidget::extractIconFromResources() const
{
    // Извлекаем иконку из ресурсов во временный файл
    QString tempDir = QDir::tempPath();
    QString iconPath = QDir(tempDir).filePath("Vuzhyk_py_icon.ico");
    
    // Если файл уже существует, используем его
    if (QFile::exists(iconPath)) {
        return iconPath;
    }
    
    // Пытаемся загрузить иконку из ресурсов
    QFile resourceFile(":/icons/icons/logo/py.ico");
    if (resourceFile.open(QIODevice::ReadOnly)) {
        QByteArray iconData = resourceFile.readAll();
        resourceFile.close();
        
        // Сохраняем во временный файл
        QFile tempFile(iconPath);
        if (tempFile.open(QIODevice::WriteOnly)) {
            tempFile.write(iconData);
            tempFile.close();
            return iconPath;
        }
    }
    
    // Если не удалось извлечь из ресурсов, используем иконку из exe
    return getApplicationPath();
}

QString SettingsWidget::getInterpretersDir() const
{
    QString appDir = QCoreApplication::applicationDirPath();
    return QDir(appDir).filePath("interpreters");
}

QStringList SettingsWidget::getAvailablePythonVersions() const
{
    // Версии из релиза https://github.com/p4ulroot/python-installers/releases/tag/A1
    return QStringList()
        << "3.13.12"
        << "3.12.10"
        << "3.11.9"
        << "3.10.11"
        << "3.9.13"
        << "3.8.10"
        << "3.7.9"
        << "3.6.8"
        << "3.5.4";
}

void SettingsWidget::loadInstalledInterpreters()
{
    m_interpreterComboBox->blockSignals(true);
    m_interpreterComboBox->clear();
    
    QString interpretersDir = getInterpretersDir();
    QDir dir(interpretersDir);
    
    if (!dir.exists()) {
        dir.mkpath(interpretersDir);
    }
    
    // Добавляем встроенный интерпретатор
    QString embeddedPython = QCoreApplication::applicationDirPath() + "/Python38/python.exe";
    if (QFile::exists(embeddedPython)) {
        m_interpreterComboBox->addItem(tr("Python 3.8 (встроенный)"), embeddedPython);
    }
    
    // Ищем установленные интерпретаторы в папке interpreters
    QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &subdir : subdirs) {
        QString pythonExe = dir.filePath(subdir + "/python.exe");
        if (QFile::exists(pythonExe)) {
            m_interpreterComboBox->addItem(QString("Python %1").arg(subdir), pythonExe);
        }
    }
    
    // Если ничего не найдено, показываем сообщение
    if (m_interpreterComboBox->count() == 0) {
        m_interpreterComboBox->addItem(tr("Интерпретаторы не найдены"), "");
    }
    
    // Обновляем текущий интерпретатор
    QSettings settings;
    settings.beginGroup("runtime");
    const QString selectedInterpreter = settings.value("selectedInterpreter").toString();
    settings.endGroup();

    if (!selectedInterpreter.isEmpty()) {
        for (int i = 0; i < m_interpreterComboBox->count(); ++i) {
            if (m_interpreterComboBox->itemData(i).toString() == selectedInterpreter) {
                m_interpreterComboBox->setCurrentIndex(i);
                break;
            }
        }
    }

    m_interpreterComboBox->blockSignals(false);

    if (m_currentInterpreterLabel) {
        m_currentInterpreterLabel->setText(tr("Текущий интерпретатор: %1").arg(resolvePythonExecutable()));
    }

    onInterpreterSelected(m_interpreterComboBox->currentIndex());
}

void SettingsWidget::onInterpreterSelected(int index)
{
    Q_UNUSED(index);

    const QString pythonPath =
        m_interpreterComboBox ? m_interpreterComboBox->currentData().toString() : QString();

    if (m_removeInterpreterBtn) {
#ifdef Q_OS_WIN
        m_removeInterpreterBtn->setEnabled(!pythonPath.isEmpty()
                                           && !isEmbeddedInterpreterPath(pythonPath)
                                           && isManagedInterpreterPath(pythonPath));
#else
        m_removeInterpreterBtn->setEnabled(!pythonPath.isEmpty());
#endif
    }

    if (!m_installedRadio || !m_installedRadio->isChecked()) {
        return;
    }

    saveInterpreterSettings();
    m_currentInterpreterLabel->setText(tr("Текущий интерпретатор: %1").arg(resolvePythonExecutable()));
    emit interpreterChanged();
    updatePackageUiEnabled();
    schedulePackagesRefresh();
}

void SettingsWidget::onInstallInterpreter()
{
    QString version = m_pythonVersionComboBox->currentText();
    if (version.isEmpty()) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Выберите версию Python"));
        return;
    }
    
    int ret = QMessageBox::question(this, tr("Подтверждение"),
        tr("Установить Python %1?\n\nБудет скачан архив (~15-40 МБ) и распакован в папку interpreters.").arg(version),
        QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        downloadAndInstallPython(version);
    }
}

void SettingsWidget::downloadAndInstallPython(const QString &version)
{
#ifndef Q_OS_WIN
    QMessageBox::critical(this, tr("Ошибка"), tr("Установка интерпретаторов поддерживается только в Windows."));
    return;
#else
    const QString url = pythonArchiveDownloadUrl(version);

    const QString interpretersDir = getInterpretersDir();
    QDir().mkpath(interpretersDir);

    const QString targetDir = QDir(interpretersDir).filePath(version);
    if (QDir(targetDir).exists()) {
        const int reinstallAnswer = QMessageBox::question(
            this,
            tr("Подтверждение"),
            tr("Python %1 уже установлен.\n\nВы хотите переустановить?").arg(version),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (reinstallAnswer != QMessageBox::Yes) {
            return;
        }
        removeInterpreterDirectory(targetDir);
    }

    const QString archivePath = QDir(interpretersDir).filePath(version + QStringLiteral(".7z"));
    if (QFile::exists(archivePath)) {
        QFile::remove(archivePath);
    }

    const QString progressBaseText =
        tr("Скачивание Python %1...\n\nИсточник: GitHub (python-installers)").arg(version);

    auto *progress = new QProgressDialog(progressBaseText, tr("Отмена"), 0, 100, this);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->setValue(0);

    auto *downloadThread = new PythonArchiveDownloadThread(this);
    downloadThread->url = url;
    downloadThread->destPath = archivePath;

    auto *progressTimer = new QTimer(progress);
    connect(progressTimer, &QTimer::timeout, this, [progress, progressBaseText, downloadThread]() {
        const qint64 downloaded = downloadThread->progress.bytesDownloaded.load();
        const qint64 total = downloadThread->progress.totalBytes.load();
        const double downloadedMb = downloaded / (1024.0 * 1024.0);

        if (total > 0) {
            const int percent = static_cast<int>(qMin<qint64>(100, (downloaded * 100) / total));
            progress->setValue(percent);
            const double totalMb = total / (1024.0 * 1024.0);
            progress->setLabelText(progressBaseText + QLatin1Char('\n') +
                tr("Скачано: %1 из %2 МБ (%3%)")
                    .arg(downloadedMb, 0, 'f', 1)
                    .arg(totalMb, 0, 'f', 1)
                    .arg(percent));
        } else if (downloaded > 0) {
            progress->setValue(0);
            progress->setLabelText(progressBaseText + QLatin1Char('\n') +
                tr("Скачано: %1 МБ").arg(downloadedMb, 0, 'f', 1));
        }
    });
    progressTimer->start(200);

    connect(progress, &QProgressDialog::canceled, downloadThread, [downloadThread]() {
        downloadThread->cancelled.store(true);
    });

    connect(downloadThread, &QThread::finished, this,
            [this, downloadThread, progress, progressTimer, version, targetDir, archivePath]() {
        progressTimer->stop();
        progress->setValue(100);
        progress->close();
        progress->deleteLater();

        const HttpsDownloadResult result = downloadThread->result;
        downloadThread->deleteLater();

        if (!result.success) {
            QString errorText = tr("Не удалось скачать Python %1.").arg(version);
            if (!result.errorMessage.isEmpty()) {
                errorText += QLatin1Char('\n') + result.errorMessage;
            } else if (result.winError != 0) {
                errorText += QLatin1Char('\n') + tr("Код ошибки Windows: %1").arg(result.winError);
            }
            QMessageBox::critical(this, tr("Ошибка"), errorText);
            QFile::remove(archivePath);
            removeInterpreterDirectory(targetDir);
            return;
        }

        QString validationError;
        if (!isValidPythonArchiveFile(archivePath, &validationError)) {
            QMessageBox::critical(this, tr("Ошибка"), validationError);
            QFile::remove(archivePath);
            removeInterpreterDirectory(targetDir);
            return;
        }

        extractDownloadedPythonArchive(version, targetDir, archivePath);
    });

    progress->show();
    downloadThread->start();
#endif
}

void SettingsWidget::extractDownloadedPythonArchive(const QString &version,
                                                    const QString &targetDir,
                                                    const QString &archivePath)
{
#ifndef Q_OS_WIN
    Q_UNUSED(version);
    Q_UNUSED(targetDir);
    Q_UNUSED(archivePath);
    return;
#else
    QProgressDialog extractProgress(
        tr("Распаковка Python %1...\n\nЭто может занять несколько минут.").arg(version),
        QString(), 0, 0, this);
    extractProgress.setWindowModality(Qt::WindowModal);
    extractProgress.setCancelButton(nullptr);
    extractProgress.setMinimumDuration(0);
    extractProgress.show();
    QCoreApplication::processEvents();

    QString extractError;
    removeInterpreterDirectory(targetDir);
    if (!extract7zArchive(archivePath, getInterpretersDir(), &extractError)) {
        extractProgress.close();
        QString errorText = tr("Не удалось распаковать Python %1.").arg(version);
        if (!extractError.isEmpty()) {
            errorText += QLatin1Char('\n') + extractError;
        }
        QMessageBox::critical(this, tr("Ошибка"), errorText);
        QFile::remove(archivePath);
        removeInterpreterDirectory(targetDir);
        return;
    }

    QCoreApplication::processEvents();

    if (!normalizeExtractedInterpreterDir(targetDir)) {
        extractProgress.close();
        QMessageBox::critical(this, tr("Ошибка"),
            tr("Не удалось найти python.exe после распаковки.\n\nПроверьте содержимое архива."));
        QFile::remove(archivePath);
        removeInterpreterDirectory(targetDir);
        return;
    }

    extractProgress.close();
    QFile::remove(archivePath);

    QMessageBox::information(this, tr("Успех"),
        tr("Python %1 успешно установлен!").arg(version));
    loadInstalledInterpreters();
#endif
}

void SettingsWidget::onRemoveInterpreter()
{
    const QString pythonPath = m_interpreterComboBox->currentData().toString();
    if (pythonPath.isEmpty()) {
        return;
    }

#ifdef Q_OS_WIN
    if (isEmbeddedInterpreterPath(pythonPath)) {
        QMessageBox::warning(this, tr("Ошибка"),
            tr("Нельзя удалить встроенный интерпретатор"));
        return;
    }

    if (!isManagedInterpreterPath(pythonPath)) {
        QMessageBox::warning(this, tr("Ошибка"),
            tr("Можно удалять только интерпретаторы из папки interpreters."));
        return;
    }
#endif

    const QString dirPath = QFileInfo(pythonPath).absolutePath();

    int ret = QMessageBox::question(this, tr("Подтверждение"),
        tr("Удалить интерпретатор?\n\n%1").arg(dirPath),
        QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes) {
#ifdef Q_OS_WIN
        const QString version = QDir(dirPath).dirName();
        const QString archivePath = QDir(getInterpretersDir()).filePath(version + QStringLiteral(".7z"));
        removeInterpreterDirectory(dirPath);
        QFile::remove(archivePath);

        QSettings settings;
        settings.beginGroup("runtime");
        const QString selectedInterpreter = settings.value("selectedInterpreter").toString();
        if (QFileInfo(selectedInterpreter).absoluteFilePath().compare(QFileInfo(pythonPath).absoluteFilePath(),
                                                                       Qt::CaseInsensitive) == 0) {
            settings.remove("selectedInterpreter");
        }
        settings.endGroup();

        if (!QDir(dirPath).exists()) {
#else
        if (QDir(dirPath).removeRecursively()) {
#endif
            QMessageBox::information(this, tr("Успех"), tr("Интерпретатор удален"));
            loadInstalledInterpreters();
            emit interpreterChanged();
        } else {
            QMessageBox::critical(this, tr("Ошибка"),
                tr("Не удалось удалить интерпретатор"));
        }
    }
}

void SettingsWidget::setupSyncPage()
{
    m_syncPage = new QWidget(m_settingsStack);
    auto *pageLayout = new QVBoxLayout(m_syncPage);
    pageLayout->setContentsMargins(20, 20, 20, 20);
    pageLayout->setSpacing(15);

    m_syncContent = new QWidget(m_syncPage);
    auto *contentLayout = new QVBoxLayout(m_syncContent);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(15);

    m_syncEnabledCheckBox = new QCheckBox(tr("Включить синхронизацию библиотек"), m_syncContent);
    contentLayout->addWidget(m_syncEnabledCheckBox);

    auto *roleGroup = new QGroupBox(tr("Роль этого компьютера"), m_syncContent);
    auto *roleLayout = new QVBoxLayout(roleGroup);
    m_syncTeacherRadio = new QRadioButton(tr("ПК учителя"), roleGroup);
    m_syncStudentRadio = new QRadioButton(tr("ПК ученика"), roleGroup);
    roleLayout->addWidget(m_syncTeacherRadio);
    roleLayout->addWidget(m_syncStudentRadio);
    contentLayout->addWidget(roleGroup);

    auto *teacherGroup = new QGroupBox(tr("Настройки учителя"), m_syncContent);
    auto *teacherLayout = new QVBoxLayout(teacherGroup);
    auto *teacherInterpreterLayout = new QHBoxLayout();
    teacherInterpreterLayout->addWidget(new QLabel(tr("Интерпретатор для синхронизации:"), teacherGroup));
    m_syncTeacherInterpreterCombo = new QComboBox(teacherGroup);
    m_syncTeacherInterpreterCombo->setMinimumWidth(260);
    teacherInterpreterLayout->addWidget(m_syncTeacherInterpreterCombo, 1);
    teacherLayout->addLayout(teacherInterpreterLayout);

    auto *portLayout = new QHBoxLayout();
    portLayout->addWidget(new QLabel(tr("Порт сервера:"), teacherGroup));
    m_syncPortSpinBox = new QSpinBox(teacherGroup);
    m_syncPortSpinBox->setRange(1024, 65535);
    m_syncPortSpinBox->setValue(8765);
    portLayout->addWidget(m_syncPortSpinBox);
    portLayout->addStretch();
    teacherLayout->addLayout(portLayout);

    m_syncForbidAgentCheckBox = new QCheckBox(tr("Запретить агента ученикам"), teacherGroup);
    teacherLayout->addWidget(m_syncForbidAgentCheckBox);
    auto *forbidHint = new QLabel(
        tr("Если включено, у учеников пункт «Агент» скрывается в меню и в настройках. "
           "Запрет сохраняется на ПК ученика до следующего разрешения с ПК учителя."),
        teacherGroup);
    forbidHint->setWordWrap(true);
    teacherLayout->addWidget(forbidHint);
    contentLayout->addWidget(teacherGroup);

    auto *studentGroup = new QGroupBox(tr("Настройки ученика"), m_syncContent);
    auto *studentLayout = new QVBoxLayout(studentGroup);
    auto *ipLayout = new QHBoxLayout();
    ipLayout->addWidget(new QLabel(tr("IP-адрес ПК учителя:"), studentGroup));
    m_syncTeacherIpEdit = new QLineEdit(studentGroup);
    m_syncTeacherIpEdit->setPlaceholderText(tr("Например: 192.168.1.10"));
    ipLayout->addWidget(m_syncTeacherIpEdit, 1);
    studentLayout->addLayout(ipLayout);

    auto *intervalLayout = new QHBoxLayout();
    intervalLayout->addWidget(new QLabel(tr("Интервал проверки (мин.):"), studentGroup));
    m_syncIntervalSpinBox = new QSpinBox(studentGroup);
    m_syncIntervalSpinBox->setRange(1, 1440);
    m_syncIntervalSpinBox->setValue(5);
    intervalLayout->addWidget(m_syncIntervalSpinBox);
    intervalLayout->addStretch();
    studentLayout->addLayout(intervalLayout);

    m_syncShowTimerCheckBox = new QCheckBox(tr("Отображать таймер синхронизации в статус-баре"), studentGroup);
    studentLayout->addWidget(m_syncShowTimerCheckBox);

    auto *actionsLayout = new QHBoxLayout();
    m_syncCheckConnectionButton = new QPushButton(tr("Проверить соединение с ПК учителя"), studentGroup);
    m_syncForceSyncButton = new QPushButton(tr("Принудительная синхронизация"), studentGroup);
    actionsLayout->addWidget(m_syncCheckConnectionButton);
    actionsLayout->addWidget(m_syncForceSyncButton);
    actionsLayout->addStretch();
    studentLayout->addLayout(actionsLayout);
    contentLayout->addWidget(studentGroup);

    auto *passwordGroup = new QGroupBox(tr("Защита вкладки паролем"), m_syncContent);
    auto *passwordLayout = new QVBoxLayout(passwordGroup);
    m_syncPasswordStatusLabel = new QLabel(passwordGroup);
    passwordLayout->addWidget(m_syncPasswordStatusLabel);
    auto *passwordButtonsLayout = new QHBoxLayout();
    m_syncSetPasswordButton = new QPushButton(tr("Установить пароль"), passwordGroup);
    m_syncRemovePasswordButton = new QPushButton(tr("Удалить пароль"), passwordGroup);
    passwordButtonsLayout->addWidget(m_syncSetPasswordButton);
    passwordButtonsLayout->addWidget(m_syncRemovePasswordButton);
    passwordButtonsLayout->addStretch();
    passwordLayout->addLayout(passwordButtonsLayout);
    contentLayout->addWidget(passwordGroup);

    contentLayout->addStretch();
    pageLayout->addWidget(m_syncContent);

    connect(m_syncEnabledCheckBox, &QCheckBox::toggled, this, &SettingsWidget::onSyncSettingsChanged);
    connect(m_syncTeacherRadio, &QRadioButton::toggled, this, &SettingsWidget::onSyncSettingsChanged);
    connect(m_syncStudentRadio, &QRadioButton::toggled, this, &SettingsWidget::onSyncSettingsChanged);
    connect(m_syncTeacherInterpreterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsWidget::onSyncSettingsChanged);
    connect(m_syncTeacherIpEdit, &QLineEdit::editingFinished, this, &SettingsWidget::onSyncSettingsChanged);
    connect(m_syncIntervalSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SettingsWidget::onSyncSettingsChanged);
    connect(m_syncShowTimerCheckBox, &QCheckBox::toggled, this, &SettingsWidget::onSyncSettingsChanged);
    connect(m_syncPortSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SettingsWidget::onSyncSettingsChanged);
    connect(m_syncForbidAgentCheckBox, &QCheckBox::toggled, this, &SettingsWidget::onSyncSettingsChanged);
    connect(m_syncSetPasswordButton, &QPushButton::clicked, this, &SettingsWidget::onSetSyncPassword);
    connect(m_syncRemovePasswordButton, &QPushButton::clicked, this, &SettingsWidget::onRemoveSyncPassword);
    connect(m_syncCheckConnectionButton, &QPushButton::clicked, this, &SettingsWidget::onSyncCheckConnection);
    connect(m_syncForceSyncButton, &QPushButton::clicked, this, &SettingsWidget::onSyncForceSync);
}

void SettingsWidget::loadSyncSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("sync"));

    QSignalBlocker blockEnabled(m_syncEnabledCheckBox);
    QSignalBlocker blockTeacher(m_syncTeacherRadio);
    QSignalBlocker blockStudent(m_syncStudentRadio);
    QSignalBlocker blockCombo(m_syncTeacherInterpreterCombo);
    QSignalBlocker blockIp(m_syncTeacherIpEdit);
    QSignalBlocker blockInterval(m_syncIntervalSpinBox);
    QSignalBlocker blockShowTimer(m_syncShowTimerCheckBox);
    QSignalBlocker blockPort(m_syncPortSpinBox);
    QSignalBlocker blockForbid(m_syncForbidAgentCheckBox);

    m_syncEnabledCheckBox->setChecked(settings.value(QStringLiteral("enabled"), false).toBool());
    const int role = settings.value(QStringLiteral("role"), 1).toInt();
    if (role == 0) {
        m_syncTeacherRadio->setChecked(true);
    } else {
        m_syncStudentRadio->setChecked(true);
    }
    m_syncTeacherIpEdit->setText(settings.value(QStringLiteral("teacherHost")).toString());
    m_syncIntervalSpinBox->setValue(settings.value(QStringLiteral("intervalMinutes"), 5).toInt());
    m_syncShowTimerCheckBox->setChecked(settings.value(QStringLiteral("showSyncTimer"), false).toBool());
    m_syncPortSpinBox->setValue(settings.value(QStringLiteral("serverPort"), 8765).toInt());
    if (m_syncForbidAgentCheckBox) {
        m_syncForbidAgentCheckBox->setChecked(settings.value(QStringLiteral("forbidStudentAgent"), false).toBool());
    }

    populateSyncInterpreterCombo();
    const QString savedInterpreter = settings.value(QStringLiteral("teacherInterpreterPath")).toString();
    if (!savedInterpreter.isEmpty()) {
        const int idx = m_syncTeacherInterpreterCombo->findData(savedInterpreter);
        if (idx >= 0) {
            m_syncTeacherInterpreterCombo->setCurrentIndex(idx);
        }
    }

    const bool hasPassword = hasSyncTabPassword();
    m_syncPasswordStatusLabel->setText(hasPassword
        ? tr("Пароль установлен. Для доступа к вкладке потребуется ввод пароля.")
        : tr("Пароль не установлен."));
    m_syncRemovePasswordButton->setEnabled(hasPassword);
    m_syncTabUnlocked = !hasPassword;
    if (m_syncContent) {
        m_syncContent->setEnabled(m_syncTabUnlocked);
    }

    settings.endGroup();
}

bool SettingsWidget::hasSyncTabPassword() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("sync"));
    const QByteArray encrypted = settings.value(QStringLiteral("tabPassword")).toByteArray();
    settings.endGroup();
    return !encrypted.isEmpty();
}

void SettingsWidget::lockSyncTabIfPasswordProtected()
{
    if (!hasSyncTabPassword()) {
        m_syncTabUnlocked = true;
        return;
    }

    m_syncTabUnlocked = false;
    if (m_syncContent) {
        m_syncContent->setEnabled(false);
    }
}

void SettingsWidget::resetSyncTabAccessOnOpen()
{
    const bool hasPassword = hasSyncTabPassword();
    m_syncTabUnlocked = !hasPassword;

    if (m_syncPasswordStatusLabel) {
        m_syncPasswordStatusLabel->setText(hasPassword
            ? tr("Пароль установлен. Для доступа к вкладке потребуется ввод пароля.")
            : tr("Пароль не установлен."));
    }
    if (m_syncRemovePasswordButton) {
        m_syncRemovePasswordButton->setEnabled(hasPassword);
    }
    if (m_syncContent) {
        m_syncContent->setEnabled(m_syncTabUnlocked);
    }

    if (hasPassword && !m_syncTabUnlocked && m_settingsStack && m_categoryList
        && m_settingsStack->currentIndex() == syncCategoryIndex()) {
        QSignalBlocker blocker(m_categoryList);
        m_categoryList->setCurrentRow(0);
        m_settingsStack->setCurrentIndex(0);
    }
}

void SettingsWidget::saveSyncSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("sync"));
    settings.setValue(QStringLiteral("enabled"), m_syncEnabledCheckBox->isChecked());
    settings.setValue(QStringLiteral("role"), m_syncTeacherRadio->isChecked() ? 0 : 1);
    settings.setValue(QStringLiteral("teacherHost"), m_syncTeacherIpEdit->text().trimmed());
    settings.setValue(QStringLiteral("intervalMinutes"), m_syncIntervalSpinBox->value());
    settings.setValue(QStringLiteral("showSyncTimer"), m_syncShowTimerCheckBox->isChecked());
    settings.setValue(QStringLiteral("serverPort"), m_syncPortSpinBox->value());
    settings.setValue(QStringLiteral("forbidStudentAgent"),
                     m_syncForbidAgentCheckBox && m_syncForbidAgentCheckBox->isChecked());
    settings.setValue(QStringLiteral("teacherInterpreterPath"),
                     m_syncTeacherInterpreterCombo->currentData().toString());
    settings.endGroup();
}

bool SettingsWidget::isStudentSyncEnabled() const
{
    return m_syncEnabledCheckBox && m_syncEnabledCheckBox->isChecked()
        && m_syncStudentRadio && m_syncStudentRadio->isChecked();
}

void SettingsWidget::updateSyncUi()
{
    if (!m_syncContent) {
        return;
    }

    const bool enabled = m_syncEnabledCheckBox->isChecked();
    const bool teacher = m_syncTeacherRadio->isChecked();

    m_syncTeacherInterpreterCombo->setEnabled(enabled && teacher);
    m_syncPortSpinBox->setEnabled(enabled && teacher);
    if (m_syncForbidAgentCheckBox) {
        m_syncForbidAgentCheckBox->setEnabled(enabled && teacher);
    }
    m_syncTeacherIpEdit->setEnabled(enabled && !teacher);
    m_syncIntervalSpinBox->setEnabled(enabled && !teacher);
    if (m_syncShowTimerCheckBox) {
        m_syncShowTimerCheckBox->setEnabled(enabled && !teacher);
    }
    if (m_syncCheckConnectionButton) {
        m_syncCheckConnectionButton->setEnabled(!teacher);
    }
    if (m_syncForceSyncButton) {
        m_syncForceSyncButton->setEnabled(!teacher);
    }
}

void SettingsWidget::updateSyncInterpreterLock()
{
    const bool lock = isStudentSyncEnabled();

    if (m_embeddedRadio) m_embeddedRadio->setEnabled(!lock);
    if (m_installedRadio) m_installedRadio->setEnabled(!lock);
    if (m_systemRadio) m_systemRadio->setEnabled(!lock);
    if (m_customRadio) m_customRadio->setEnabled(!lock);
    if (m_pythonPathEdit) m_pythonPathEdit->setEnabled(!lock && m_customRadio && m_customRadio->isChecked());
    if (m_browseButton) m_browseButton->setEnabled(!lock && m_customRadio && m_customRadio->isChecked());
    if (m_interpreterComboBox) m_interpreterComboBox->setEnabled(!lock && m_installedRadio && m_installedRadio->isChecked());
    if (m_installInterpreterBtn) m_installInterpreterBtn->setEnabled(!lock);
    if (m_removeInterpreterBtn) m_removeInterpreterBtn->setEnabled(!lock && m_interpreterComboBox && m_interpreterComboBox->currentIndex() >= 0);
    if (m_pythonVersionComboBox) m_pythonVersionComboBox->setEnabled(!lock);
}

void SettingsWidget::populateSyncInterpreterCombo()
{
    if (!m_syncTeacherInterpreterCombo) {
        return;
    }

    const QString current = m_syncTeacherInterpreterCombo->currentData().toString();
    m_syncTeacherInterpreterCombo->clear();

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString embedded = QDir(appDir).filePath(QStringLiteral("Python38/python.exe"));
    if (QFile::exists(embedded)) {
        m_syncTeacherInterpreterCombo->addItem(tr("Встроенный (Python38)"), embedded);
    }

    QDir interpretersDir(getInterpretersDir());
    if (interpretersDir.exists()) {
        const QStringList entries = interpretersDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &entry : entries) {
            const QString pythonPath = interpretersDir.filePath(entry + QStringLiteral("/python.exe"));
            if (QFile::exists(pythonPath)) {
                m_syncTeacherInterpreterCombo->addItem(entry, pythonPath);
            }
        }
    }

    const int idx = m_syncTeacherInterpreterCombo->findData(current);
    if (idx >= 0) {
        m_syncTeacherInterpreterCombo->setCurrentIndex(idx);
    }
}

bool SettingsWidget::tryUnlockSyncPage()
{
    if (!hasSyncTabPassword()) {
        m_syncTabUnlocked = true;
        if (m_syncContent) {
            m_syncContent->setEnabled(true);
        }
        return true;
    }

    if (m_syncTabUnlocked) {
        if (m_syncContent) {
            m_syncContent->setEnabled(true);
        }
        return true;
    }

    bool ok = false;
    const QString password = QInputDialog::getText(
        this,
        tr("Доступ к синхронизации"),
        tr("Введите пароль:"),
        QLineEdit::Password,
        QString(),
        &ok);
    if (!ok) {
        return false;
    }

    QString errorMessage;
    QSettings settings;
    settings.beginGroup(QStringLiteral("sync"));
    const QByteArray encrypted = settings.value(QStringLiteral("tabPassword")).toByteArray();
    settings.endGroup();
    if (!LibrarySyncCrypto::verifyPassword(encrypted, password, &errorMessage)) {
        QMessageBox::warning(this, tr("Доступ запрещён"),
                             errorMessage.isEmpty() ? tr("Неверный пароль.") : errorMessage);
        return false;
    }

    m_syncTabUnlocked = true;
    if (m_syncContent) {
        m_syncContent->setEnabled(true);
    }
    return true;
}

void SettingsWidget::applyTeacherSyncServer()
{
    if (!m_syncEnabledCheckBox->isChecked() || !m_syncTeacherRadio->isChecked()) {
        QString errorMessage;
        LibrarySyncServerControl::removeAutostart(&errorMessage);
        LibrarySyncServerControl::stopServerProcess();
        return;
    }

    const QString pythonPath = m_syncTeacherInterpreterCombo->currentData().toString();
    if (pythonPath.isEmpty()) {
        emit showStatusMessage(tr("Выберите интерпретатор для синхронизации."), 5000);
        return;
    }

    QString siteError;
    const QString sitePackages = querySitePackagesDir(pythonPath, &siteError);
    if (sitePackages.isEmpty()) {
        emit showStatusMessage(siteError, 5000);
        return;
    }

    QString errorMessage;
    const bool allowAgent = !(m_syncForbidAgentCheckBox && m_syncForbidAgentCheckBox->isChecked());
    if (!LibrarySyncServerControl::writeServerConfig(sitePackages, m_syncPortSpinBox->value(),
                                                     allowAgent, &errorMessage)) {
        emit showStatusMessage(errorMessage, 5000);
        return;
    }
    if (!LibrarySyncServerControl::ensureAutostartEnabled(&errorMessage)) {
        emit showStatusMessage(errorMessage, 5000);
        return;
    }
    if (!LibrarySyncServerControl::launchServerIfNeeded(&errorMessage)) {
        emit showStatusMessage(errorMessage, 5000);
        return;
    }

    emit showStatusMessage(tr("Сервер синхронизации запущен."), 4000);
}

void SettingsWidget::onSyncSettingsChanged()
{
    saveSyncSettings();
    updateSyncUi();
    updateSyncInterpreterLock();
    updatePackageUiEnabled();
    applyTeacherSyncServer();
    emit syncSettingsChanged();
}

void SettingsWidget::onSyncCheckConnection()
{
    if (!m_syncStudentRadio || !m_syncStudentRadio->isChecked()) {
        QMessageBox::information(this, tr("Синхронизация"),
                               tr("Проверка соединения доступна только на ПК ученика."));
        return;
    }

    if (m_syncTeacherIpEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Синхронизация"), tr("Укажите IP-адрес ПК учителя."));
        return;
    }

    saveSyncSettings();
    emit syncCheckConnectionRequested();
}

void SettingsWidget::onSyncForceSync()
{
    if (!m_syncStudentRadio || !m_syncStudentRadio->isChecked()) {
        QMessageBox::information(this, tr("Синхронизация"),
                               tr("Принудительная синхронизация доступна только на ПК ученика."));
        return;
    }

    if (m_syncTeacherIpEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Синхронизация"), tr("Укажите IP-адрес ПК учителя."));
        return;
    }

    saveSyncSettings();
    emit syncForceRequested();
}

void SettingsWidget::onSetSyncPassword()
{
    bool ok = false;
    const QString password = QInputDialog::getText(
        this, tr("Установить пароль"), tr("Новый пароль:"), QLineEdit::Password, QString(), &ok);
    if (!ok || password.isEmpty()) {
        return;
    }

    const QString confirm = QInputDialog::getText(
        this, tr("Подтверждение"), tr("Повторите пароль:"), QLineEdit::Password, QString(), &ok);
    if (!ok || confirm != password) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Пароли не совпадают."));
        return;
    }

    QByteArray encrypted;
    QString errorMessage;
    if (!LibrarySyncCrypto::encryptString(password, &encrypted, &errorMessage)) {
        QMessageBox::critical(this, tr("Ошибка"), errorMessage);
        return;
    }

    QSettings settings;
    settings.beginGroup(QStringLiteral("sync"));
    settings.setValue(QStringLiteral("tabPassword"), encrypted);
    settings.endGroup();

    m_syncTabUnlocked = true;
    m_syncPasswordStatusLabel->setText(tr("Пароль установлен. Для доступа к вкладке потребуется ввод пароля."));
    m_syncRemovePasswordButton->setEnabled(true);
    if (m_syncContent) {
        m_syncContent->setEnabled(true);
    }
    emit showStatusMessage(tr("Пароль для вкладки синхронизации установлен."), 3000);
}

void SettingsWidget::onRemoveSyncPassword()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("sync"));
    settings.remove(QStringLiteral("tabPassword"));
    settings.endGroup();

    m_syncTabUnlocked = true;
    m_syncPasswordStatusLabel->setText(tr("Пароль не установлен."));
    m_syncRemovePasswordButton->setEnabled(false);
    if (m_syncContent) {
        m_syncContent->setEnabled(true);
    }
    emit showStatusMessage(tr("Пароль для вкладки синхронизации удалён."), 3000);
}

void SettingsWidget::setupAgentPage()
{
    m_agentPage = new QWidget(m_settingsStack);
    auto *layout = new QVBoxLayout(m_agentPage);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(15);

    auto *title = new QLabel(tr("Настройки агента"), m_agentPage);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto *providerGroup = new QGroupBox(tr("Провайдер"), m_agentPage);
    auto *providerLayout = new QVBoxLayout(providerGroup);

    auto *providerRow = new QHBoxLayout();
    providerRow->addWidget(new QLabel(tr("Провайдер:"), providerGroup));
    m_agentProviderCombo = new QComboBox(providerGroup);
    m_agentProviderCombo->addItem(tr("LM Studio"), QStringLiteral("lmstudio"));
    m_agentProviderCombo->addItem(QStringLiteral("OpenRouter"), QStringLiteral("openrouter"));
    providerRow->addWidget(m_agentProviderCombo, 1);
    providerLayout->addLayout(providerRow);

    m_agentLmStudioUrlRow = new QWidget(providerGroup);
    auto *urlRow = new QHBoxLayout(m_agentLmStudioUrlRow);
    urlRow->setContentsMargins(0, 0, 0, 0);
    urlRow->addWidget(new QLabel(tr("URL LM Studio:"), m_agentLmStudioUrlRow));
    m_agentLmStudioUrlEdit = new QLineEdit(m_agentLmStudioUrlRow);
    m_agentLmStudioUrlEdit->setPlaceholderText(QStringLiteral("http://127.0.0.1:1234/v1"));
    urlRow->addWidget(m_agentLmStudioUrlEdit, 1);
    providerLayout->addWidget(m_agentLmStudioUrlRow);

    auto *testRow = new QHBoxLayout();
    m_agentTestConnectionBtn = new QPushButton(tr("Проверить соединение"), providerGroup);
    m_agentConnectionStatusLabel = new QLabel(providerGroup);
    m_agentConnectionStatusLabel->setWordWrap(true);
    testRow->addWidget(m_agentTestConnectionBtn);
    testRow->addWidget(m_agentConnectionStatusLabel, 1);
    providerLayout->addLayout(testRow);

    layout->addWidget(providerGroup);

    m_agentOpenRouterGroup = new QGroupBox(tr("OpenRouter"), m_agentPage);
    auto *openRouterLayout = new QVBoxLayout(m_agentOpenRouterGroup);

    openRouterLayout->addWidget(new QLabel(tr("API-ключи:"), m_agentOpenRouterGroup));
    m_agentOpenRouterKeysList = new QListWidget(m_agentOpenRouterGroup);
    m_agentOpenRouterKeysList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_agentOpenRouterKeysList->setMaximumHeight(90);
    openRouterLayout->addWidget(m_agentOpenRouterKeysList);

    auto *openRouterButtons = new QHBoxLayout();
    m_agentAddOpenRouterKeyBtn = new QPushButton(tr("Добавить ключ"), m_agentOpenRouterGroup);
    m_agentRemoveOpenRouterKeyBtn = new QPushButton(tr("Удалить ключ"), m_agentOpenRouterGroup);
    openRouterButtons->addWidget(m_agentAddOpenRouterKeyBtn);
    openRouterButtons->addWidget(m_agentRemoveOpenRouterKeyBtn);
    openRouterButtons->addStretch();
    openRouterLayout->addLayout(openRouterButtons);

    openRouterLayout->addWidget(new QLabel(tr("Модели:"), m_agentOpenRouterGroup));
    m_agentOpenRouterModelsList = new QListWidget(m_agentOpenRouterGroup);
    m_agentOpenRouterModelsList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_agentOpenRouterModelsList->setMaximumHeight(120);
    openRouterLayout->addWidget(m_agentOpenRouterModelsList);

    auto *modelButtons = new QHBoxLayout();
    m_agentAddOpenRouterModelBtn = new QPushButton(tr("Добавить модель"), m_agentOpenRouterGroup);
    m_agentRemoveOpenRouterModelBtn = new QPushButton(tr("Удалить модель"), m_agentOpenRouterGroup);
    modelButtons->addWidget(m_agentAddOpenRouterModelBtn);
    modelButtons->addWidget(m_agentRemoveOpenRouterModelBtn);
    modelButtons->addStretch();
    openRouterLayout->addLayout(modelButtons);

    layout->addWidget(m_agentOpenRouterGroup);

    m_agentHintLabel = new QLabel(m_agentPage);
    m_agentHintLabel->setWordWrap(true);
    layout->addWidget(m_agentHintLabel);
    layout->addStretch();

    connect(m_agentProviderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsWidget::onAgentSettingsChanged);
    connect(m_agentLmStudioUrlEdit, &QLineEdit::editingFinished,
            this, &SettingsWidget::onAgentSettingsChanged);
    connect(m_agentTestConnectionBtn, &QPushButton::clicked,
            this, &SettingsWidget::onAgentTestConnection);
    connect(m_agentAddOpenRouterKeyBtn, &QPushButton::clicked,
            this, &SettingsWidget::onAddOpenRouterKey);
    connect(m_agentRemoveOpenRouterKeyBtn, &QPushButton::clicked,
            this, &SettingsWidget::onRemoveOpenRouterKey);
    connect(m_agentAddOpenRouterModelBtn, &QPushButton::clicked,
            this, &SettingsWidget::onAddOpenRouterModel);
    connect(m_agentRemoveOpenRouterModelBtn, &QPushButton::clicked,
            this, &SettingsWidget::onRemoveOpenRouterModel);
}

void SettingsWidget::updateAgentProviderUi()
{
    if (!m_agentProviderCombo) {
        return;
    }

    const bool openRouter = (m_agentProviderCombo->currentData().toString()
                             == QLatin1String("openrouter"));

    if (m_agentLmStudioUrlRow) {
        m_agentLmStudioUrlRow->setVisible(!openRouter);
    }
    if (m_agentOpenRouterGroup) {
        m_agentOpenRouterGroup->setVisible(openRouter);
    }
    if (m_agentHintLabel) {
        m_agentHintLabel->setText(openRouter
            ? tr("Добавьте API-ключи и нужные модели. В панели агента будут только модели из этого списка.")
            : tr("Запустите LM Studio, загрузите модель и включите Local Server "
                 "(обычно http://127.0.0.1:1234). Список моделей появится в панели агента."));
    }
}

void SettingsWidget::refreshOpenRouterKeysList()
{
    if (!m_agentOpenRouterKeysList) {
        return;
    }

    m_agentOpenRouterKeysList->clear();
    const QVector<OpenRouterKeyEntry> keys = AgentOpenRouterKeys::load();
    for (const OpenRouterKeyEntry &entry : keys) {
        m_agentOpenRouterKeysList->addItem(entry.name);
    }
    if (m_agentRemoveOpenRouterKeyBtn) {
        m_agentRemoveOpenRouterKeyBtn->setEnabled(!keys.isEmpty());
    }
}

void SettingsWidget::refreshOpenRouterModelsList()
{
    if (!m_agentOpenRouterModelsList) {
        return;
    }

    m_agentOpenRouterModelsList->clear();
    const QStringList models = AgentOpenRouterKeys::loadModels();
    for (const QString &id : models) {
        m_agentOpenRouterModelsList->addItem(id);
    }
    if (m_agentRemoveOpenRouterModelBtn) {
        m_agentRemoveOpenRouterModelBtn->setEnabled(!models.isEmpty());
    }
}

void SettingsWidget::loadAgentSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("agent"));
    const QString provider = settings.value(QStringLiteral("provider"), QStringLiteral("lmstudio")).toString();
    const QString baseUrl = settings.value(QStringLiteral("lmstudioBaseUrl"),
                                           QStringLiteral("http://127.0.0.1:1234/v1")).toString();
    settings.endGroup();

    if (m_agentProviderCombo) {
        const int idx = m_agentProviderCombo->findData(provider);
        QSignalBlocker blocker(m_agentProviderCombo);
        m_agentProviderCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    if (m_agentLmStudioUrlEdit) {
        QSignalBlocker blocker(m_agentLmStudioUrlEdit);
        m_agentLmStudioUrlEdit->setText(baseUrl);
    }

    refreshOpenRouterKeysList();
    refreshOpenRouterModelsList();
    updateAgentProviderUi();
}

void SettingsWidget::saveAgentSettings()
{
    if (!m_agentProviderCombo || !m_agentLmStudioUrlEdit) {
        return;
    }

    QString url = m_agentLmStudioUrlEdit->text().trimmed();
    while (url.endsWith(QLatin1Char('/'))) {
        url.chop(1);
    }
    if (url.isEmpty()) {
        url = QStringLiteral("http://127.0.0.1:1234/v1");
        m_agentLmStudioUrlEdit->setText(url);
    }

    QSettings settings;
    settings.beginGroup(QStringLiteral("agent"));
    settings.setValue(QStringLiteral("provider"), m_agentProviderCombo->currentData().toString());
    settings.setValue(QStringLiteral("lmstudioBaseUrl"), url);
    settings.endGroup();
}

void SettingsWidget::onAgentSettingsChanged()
{
    updateAgentProviderUi();
    saveAgentSettings();
    emit agentSettingsChanged();
}

void SettingsWidget::onAgentTestConnection()
{
    saveAgentSettings();

    const bool openRouter = m_agentProviderCombo
        && m_agentProviderCombo->currentData().toString() == QLatin1String("openrouter");

    QString url;
    QString apiKey;
    if (openRouter) {
        url = QStringLiteral("https://openrouter.ai/api/v1");
        const QVector<OpenRouterKeyEntry> keys = AgentOpenRouterKeys::load();
        if (keys.isEmpty()) {
            m_agentConnectionStatusLabel->setText(tr("Добавьте API-ключ OpenRouter."));
            return;
        }
        apiKey = keys.first().key;
    } else {
        url = m_agentLmStudioUrlEdit->text().trimmed();
        while (url.endsWith(QLatin1Char('/'))) {
            url.chop(1);
        }
        if (url.isEmpty()) {
            url = QStringLiteral("http://127.0.0.1:1234/v1");
        }
    }

    m_agentConnectionStatusLabel->setText(tr("Проверка…"));
    m_agentTestConnectionBtn->setEnabled(false);

    const QString path = openRouter ? QStringLiteral("/key") : QStringLiteral("/models");
    const QUrl requestUrl(url + path);
    const QString headers = LmStudioClient::openRouterHeaders(apiKey);

    if (openRouter || requestUrl.scheme() == QLatin1String("https")) {
        auto *thread = QThread::create([this, requestUrl, headers, openRouter]() {
            const AgentHttpsResult result = AgentHttps::request(
                QStringLiteral("GET"), requestUrl, headers, QByteArray(), 20000);
            QMetaObject::invokeMethod(this, [this, result, openRouter]() {
                m_agentTestConnectionBtn->setEnabled(true);
                if (!result.ok) {
                    QString details = result.error;
                    const QJsonDocument errDoc = QJsonDocument::fromJson(result.body);
                    const QString apiMsg = errDoc.object()
                        .value(QStringLiteral("error")).toObject()
                        .value(QStringLiteral("message")).toString();
                    if (!apiMsg.isEmpty()) {
                        details = apiMsg;
                    }
                    m_agentConnectionStatusLabel->setText(
                        tr("Ошибка (HTTP %1): %2").arg(result.status).arg(details));
                } else if (openRouter) {
                    m_agentConnectionStatusLabel->setText(tr("OK. Ключ OpenRouter принят."));
                } else {
                    const QJsonDocument doc = QJsonDocument::fromJson(result.body);
                    int count = 0;
                    if (doc.isObject()) {
                        count = doc.object().value(QStringLiteral("data")).toArray().size();
                    }
                    m_agentConnectionStatusLabel->setText(
                        tr("OK. Моделей: %1").arg(count));
                }
                emit agentSettingsChanged();
            }, Qt::QueuedConnection);
        });
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        thread->start();
        return;
    }

    auto *nam = new QNetworkAccessManager(this);
    QNetworkRequest request(requestUrl);
    request.setRawHeader("Accept", "application/json");
    QNetworkReply *reply = nam->get(request);
    auto *timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    timer->start(20000);

    connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
        m_agentTestConnectionBtn->setEnabled(true);
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            m_agentConnectionStatusLabel->setText(tr("Таймаут: LM Studio не ответил за 20 с."));
        } else if (reply->error() != QNetworkReply::NoError) {
            m_agentConnectionStatusLabel->setText(
                tr("Ошибка: %1").arg(reply->errorString()));
        } else {
            const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            int count = 0;
            if (doc.isObject()) {
                count = doc.object().value(QStringLiteral("data")).toArray().size();
            }
            m_agentConnectionStatusLabel->setText(
                tr("OK. Моделей в LM Studio: %1").arg(count));
        }
        reply->deleteLater();
        nam->deleteLater();
        emit agentSettingsChanged();
    });
}

void SettingsWidget::onAddOpenRouterKey()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Добавить API-ключ OpenRouter"));

    auto *formLayout = new QFormLayout(&dialog);
    auto *nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText(tr("Например: Рабочий"));
    auto *keyEdit = new QLineEdit(&dialog);
    keyEdit->setEchoMode(QLineEdit::Password);
    keyEdit->setPlaceholderText(QStringLiteral("sk-or-..."));
    formLayout->addRow(tr("Название:"), nameEdit);
    formLayout->addRow(tr("API-ключ:"), keyEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    formLayout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString name = nameEdit->text().trimmed();
    const QString key = keyEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Укажите название ключа (минимум 1 символ)."));
        return;
    }
    if (key.isEmpty()) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Укажите API-ключ."));
        return;
    }
    if (AgentOpenRouterKeys::nameExists(name)) {
        QMessageBox::warning(this, tr("Ошибка"),
                             tr("Ключ с таким названием уже существует. Выберите другое название."));
        return;
    }

    QVector<OpenRouterKeyEntry> keys = AgentOpenRouterKeys::load();
    keys.append({name, key});
    AgentOpenRouterKeys::save(keys);
    refreshOpenRouterKeysList();
    saveAgentSettings();
    emit agentSettingsChanged();
    emit showStatusMessage(tr("API-ключ OpenRouter добавлен."), 3000);
}

void SettingsWidget::onRemoveOpenRouterKey()
{
    if (!m_agentOpenRouterKeysList) {
        return;
    }

    const QListWidgetItem *item = m_agentOpenRouterKeysList->currentItem();
    if (!item) {
        QMessageBox::information(this, tr("OpenRouter"), tr("Выберите ключ для удаления."));
        return;
    }

    const QString name = item->text();
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        tr("Удалить ключ"),
        tr("Удалить API-ключ «%1»?").arg(name));
    if (answer != QMessageBox::Yes) {
        return;
    }

    QVector<OpenRouterKeyEntry> keys = AgentOpenRouterKeys::load();
    for (int i = keys.size() - 1; i >= 0; --i) {
        if (keys.at(i).name.compare(name, Qt::CaseInsensitive) == 0) {
            keys.removeAt(i);
            break;
        }
    }
    AgentOpenRouterKeys::save(keys);
    refreshOpenRouterKeysList();
    saveAgentSettings();
    emit agentSettingsChanged();
    emit showStatusMessage(tr("API-ключ OpenRouter удалён."), 3000);
}

void SettingsWidget::onAddOpenRouterModel()
{
    const QVector<OpenRouterKeyEntry> keys = AgentOpenRouterKeys::load();
    if (keys.isEmpty()) {
        QMessageBox::warning(this, tr("OpenRouter"),
                             tr("Сначала добавьте API-ключ — каталог моделей загружается с OpenRouter."));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Добавить модель OpenRouter"));
    dialog.resize(520, 420);

    auto *layout = new QVBoxLayout(&dialog);
    auto *status = new QLabel(tr("Загрузка каталога моделей…"), &dialog);
    status->setWordWrap(true);
    layout->addWidget(status);

    auto *search = new QLineEdit(&dialog);
    search->setPlaceholderText(tr("Поиск по названию или id…"));
    search->setEnabled(false);
    layout->addWidget(search);

    auto *list = new QListWidget(&dialog);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(list, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Добавить"));
    buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, &dialog, &QDialog::accept);

    auto catalog = QSharedPointer<QVector<QPair<QString, QString>>>::create();
    auto applyFilter = [list, search, catalog]() {
        const QString query = search->text().trimmed();
        list->clear();
        int shown = 0;
        for (const auto &entry : *catalog) {
            const QString &id = entry.first;
            const QString &name = entry.second;
            if (!query.isEmpty()
                && !id.contains(query, Qt::CaseInsensitive)
                && !name.contains(query, Qt::CaseInsensitive)) {
                continue;
            }
            auto *item = new QListWidgetItem(id);
            if (!name.isEmpty() && name.compare(id, Qt::CaseInsensitive) != 0) {
                item->setToolTip(name);
            }
            list->addItem(item);
            if (++shown >= 250) {
                break;
            }
        }
    };
    connect(search, &QLineEdit::textChanged, &dialog, applyFilter);

    QPointer<QLabel> statusPtr(status);
    QPointer<QLineEdit> searchPtr(search);
    QPointer<QDialogButtonBox> buttonsPtr(buttons);
    const QString headers = LmStudioClient::openRouterHeaders(keys.first().key);
    auto *thread = QThread::create([this, catalog, headers, statusPtr, searchPtr, buttonsPtr, applyFilter]() {
        const AgentHttpsResult result = AgentHttps::request(
            QStringLiteral("GET"),
            QUrl(QStringLiteral("https://openrouter.ai/api/v1/models")),
            headers,
            QByteArray(),
            45000);
        QMetaObject::invokeMethod(this, [catalog, result, statusPtr, searchPtr, buttonsPtr, applyFilter]() {
            if (!statusPtr) {
                return;
            }
            if (!result.ok) {
                QString details = result.error;
                const QJsonDocument errDoc = QJsonDocument::fromJson(result.body);
                const QString apiMsg = errDoc.object()
                    .value(QStringLiteral("error")).toObject()
                    .value(QStringLiteral("message")).toString();
                if (!apiMsg.isEmpty()) {
                    details = apiMsg;
                }
                statusPtr->setText(tr("Не удалось загрузить каталог: %1").arg(details));
                return;
            }

            const QJsonDocument doc = QJsonDocument::fromJson(result.body);
            const QJsonArray data = doc.object().value(QStringLiteral("data")).toArray();
            catalog->reserve(data.size());
            for (const QJsonValue &value : data) {
                const QJsonObject obj = value.toObject();
                const QString id = obj.value(QStringLiteral("id")).toString().trimmed();
                if (id.isEmpty()) {
                    continue;
                }
                const QString name = obj.value(QStringLiteral("name")).toString().trimmed();
                catalog->append(qMakePair(id, name));
            }
            statusPtr->setText(tr("Найдено моделей: %1. Введите запрос для поиска.").arg(catalog->size()));
            if (searchPtr) {
                searchPtr->setEnabled(true);
                searchPtr->setFocus();
            }
            if (buttonsPtr && buttonsPtr->button(QDialogButtonBox::Ok)) {
                buttonsPtr->button(QDialogButtonBox::Ok)->setEnabled(true);
            }
            applyFilter();
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();

    const int code = dialog.exec();
    if (code != QDialog::Accepted) {
        return;
    }

    const QListWidgetItem *selected = list->currentItem();
    if (!selected) {
        QMessageBox::information(this, tr("OpenRouter"), tr("Выберите модель из списка."));
        return;
    }

    const QString modelId = selected->text().trimmed();
    if (AgentOpenRouterKeys::modelExists(modelId)) {
        QMessageBox::information(this, tr("OpenRouter"),
                                 tr("Модель «%1» уже добавлена.").arg(modelId));
        return;
    }

    QStringList models = AgentOpenRouterKeys::loadModels();
    models.append(modelId);
    AgentOpenRouterKeys::saveModels(models);
    refreshOpenRouterModelsList();
    emit agentSettingsChanged();
    emit showStatusMessage(tr("Модель OpenRouter добавлена."), 3000);
}

void SettingsWidget::onRemoveOpenRouterModel()
{
    if (!m_agentOpenRouterModelsList) {
        return;
    }

    const QListWidgetItem *item = m_agentOpenRouterModelsList->currentItem();
    if (!item) {
        QMessageBox::information(this, tr("OpenRouter"), tr("Выберите модель для удаления."));
        return;
    }

    const QString modelId = item->text();
    QStringList models = AgentOpenRouterKeys::loadModels();
    for (int i = models.size() - 1; i >= 0; --i) {
        if (models.at(i).compare(modelId, Qt::CaseInsensitive) == 0) {
            models.removeAt(i);
            break;
        }
    }
    AgentOpenRouterKeys::saveModels(models);
    refreshOpenRouterModelsList();
    emit agentSettingsChanged();
    emit showStatusMessage(tr("Модель OpenRouter удалена."), 3000);
}
