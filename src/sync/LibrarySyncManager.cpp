#include "LibrarySyncManager.h"

#include "../PythonInterpreterPaths.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>

namespace {

QString syncSettingsGroup()
{
    return QStringLiteral("sync");
}

QString normalizePackageName(const QString &name)
{
    QString normalized = name.trimmed().toLower();
    normalized.replace(QLatin1Char('-'), QLatin1Char('_'));
    return normalized;
}

} // namespace

LibrarySyncManager::LibrarySyncManager(QObject *parent)
    : QObject(parent)
{
    connect(&m_syncTimer, &QTimer::timeout, this, &LibrarySyncManager::onSyncTimer);
    reloadSettings();
}

void LibrarySyncManager::reloadSettings()
{
    QSettings settings;
    settings.beginGroup(syncSettingsGroup());
    m_enabled = settings.value(QStringLiteral("enabled"), false).toBool();
    m_role = static_cast<Role>(settings.value(QStringLiteral("role"), RoleStudent).toInt());
    m_teacherHost = settings.value(QStringLiteral("teacherHost")).toString().trimmed();
    m_intervalMinutes = settings.value(QStringLiteral("intervalMinutes"), 5).toInt();
    if (m_intervalMinutes < 1) {
        m_intervalMinutes = 1;
    }
    m_serverPort = settings.value(QStringLiteral("serverPort"), 8765).toInt();
    if (m_serverPort <= 0 || m_serverPort > 65535) {
        m_serverPort = 8765;
    }
    m_teacherInterpreterPath = settings.value(QStringLiteral("teacherInterpreterPath")).toString();
    m_showSyncTimer = settings.value(QStringLiteral("showSyncTimer"), false).toBool();
    settings.endGroup();

    m_syncTimer.stop();
    if (m_enabled && m_role == RoleStudent) {
        m_syncTimer.start(m_intervalMinutes * 60 * 1000);
    }
    emit studentAgentAccessChanged();
}

int LibrarySyncManager::secondsUntilNextSync() const
{
    if (!m_enabled || m_role != RoleStudent || !m_syncTimer.isActive()) {
        return -1;
    }

    const int remainingMs = m_syncTimer.remainingTime();
    if (remainingMs < 0) {
        return -1;
    }

    return (remainingMs + 999) / 1000;
}

void LibrarySyncManager::applyTeacherServerSettings()
{
    // Handled from SettingsWidget via LibrarySyncServerControl when teacher toggles sync.
}

QString LibrarySyncManager::resolvePythonExecutable() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("interpreter"));
    const int type = settings.value(QStringLiteral("type"), 0).toInt();
    const QString selectedPath = settings.value(QStringLiteral("selectedInterpreter")).toString();
    const QString customPath = settings.value(QStringLiteral("pythonPath")).toString();
    settings.endGroup();

    if (m_role == RoleTeacher && !m_teacherInterpreterPath.isEmpty() && QFile::exists(m_teacherInterpreterPath)) {
        return QDir::toNativeSeparators(m_teacherInterpreterPath);
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    if (type == 1 && !selectedPath.isEmpty() && QFile::exists(selectedPath)) {
        return QDir::toNativeSeparators(selectedPath);
    }
    if (type == 3 && !customPath.isEmpty() && QFile::exists(customPath)) {
        return QDir::toNativeSeparators(customPath);
    }
    if (type == 2) {
        QProcess process;
        process.start(QStringLiteral("where"), QStringList() << QStringLiteral("python"));
        if (process.waitForFinished(5000) && process.exitCode() == 0) {
            const QString path = QString::fromUtf8(process.readAllStandardOutput()).trimmed().split(QLatin1Char('\n')).value(0).trimmed();
            if (!path.isEmpty() && QFile::exists(path)) {
                return QDir::toNativeSeparators(path);
            }
        }
    }

    const QString embedded = QDir(appDir).filePath(QStringLiteral("Python38/python.exe"));
    if (QFile::exists(embedded)) {
        return QDir::toNativeSeparators(embedded);
    }

    return {};
}

QString LibrarySyncManager::resolveSitePackagesDir(QString *errorMessage) const
{
    const QString python = resolvePythonExecutable();
    if (python.isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("Интерпретатор Python не найден.");
        }
        return {};
    }
    return PythonInterpreterPaths::sitePackagesDir(python, true, errorMessage);
}

QString LibrarySyncManager::sevenZipPath() const
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("tools/zip/7za.exe"));
}

QStringList LibrarySyncManager::fetchLocalPackages(QString *errorMessage) const
{
    const QString python = resolvePythonExecutable();
    if (python.isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("Интерпретатор Python не найден.");
        }
        return {};
    }

    QProcess process;
    process.setProgram(python);
    process.setArguments(QStringList()
        << QStringLiteral("-m") << QStringLiteral("pip")
        << QStringLiteral("list") << QStringLiteral("--format=json"));
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();
    if (!process.waitForFinished(120000)) {
        process.kill();
        if (errorMessage) {
            *errorMessage = tr("Не удалось получить список локальных пакетов.");
        }
        return {};
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(process.readAllStandardError()).trimmed();
        }
        return {};
    }

    const QJsonDocument doc = QJsonDocument::fromJson(process.readAllStandardOutput());
    if (!doc.isArray()) {
        if (errorMessage) {
            *errorMessage = tr("Некорректный ответ pip list.");
        }
        return {};
    }

    QStringList packages;
    for (const QJsonValue &value : doc.array()) {
        const QJsonObject obj = value.toObject();
        const QString name = normalizePackageName(obj.value(QStringLiteral("name")).toString());
        if (!name.isEmpty()) {
            packages.append(name);
        }
    }
    packages.removeDuplicates();
    return packages;
}

QStringList LibrarySyncManager::fetchTeacherPackages(QString *errorMessage) const
{
    if (m_teacherHost.isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("IP-адрес ПК учителя не указан.");
        }
        return {};
    }

    const QUrl url(QStringLiteral("http://%1:%2/api/v1/packages")
                     .arg(m_teacherHost)
                     .arg(m_serverPort));

    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    QNetworkReply *reply = manager.get(request);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        if (errorMessage) {
            *errorMessage = tr("Не удалось связаться с сервером учителя: %1").arg(reply->errorString());
        }
        reply->deleteLater();
        return {};
    }

    const QByteArray body = reply->readAll();
    reply->deleteLater();

    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isArray()) {
        if (errorMessage) {
            *errorMessage = tr("Некорректный ответ сервера учителя.");
        }
        return {};
    }

    QStringList packages;
    for (const QJsonValue &value : doc.array()) {
        const QString name = normalizePackageName(value.toString());
        if (!name.isEmpty()) {
            packages.append(name);
        }
    }
    packages.removeDuplicates();
    return packages;
}

bool LibrarySyncManager::isStudentAgentAllowed() const
{
    if (m_role != RoleStudent) {
        return true;
    }
    QSettings settings;
    settings.beginGroup(syncSettingsGroup());
    const bool allowed = settings.value(QStringLiteral("cachedStudentAgentAllowed"), true).toBool();
    settings.endGroup();
    return allowed;
}

QJsonObject LibrarySyncManager::fetchTeacherStatusObject(QString *errorMessage) const
{
    if (m_teacherHost.isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("IP-адрес ПК учителя не указан.");
        }
        return QJsonObject();
    }

    const QUrl url(QStringLiteral("http://%1:%2/api/v1/status")
                     .arg(m_teacherHost)
                     .arg(m_serverPort));

    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    QNetworkReply *reply = manager.get(request);

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    timeoutTimer.setInterval(10000);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timeoutTimer.start();
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        if (errorMessage) {
            *errorMessage = tr("Превышено время ожидания ответа от ПК учителя.");
        }
        reply->deleteLater();
        return QJsonObject();
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (errorMessage) {
            *errorMessage = tr("Не удалось связаться с сервером учителя: %1").arg(reply->errorString());
        }
        reply->deleteLater();
        return QJsonObject();
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    reply->deleteLater();

    if (!doc.isObject() || !doc.object().value(QStringLiteral("ok")).toBool()) {
        if (errorMessage) {
            *errorMessage = tr("Сервер учителя ответил некорректно.");
        }
        return QJsonObject();
    }

    return doc.object();
}

void LibrarySyncManager::applyCachedStudentAgentPolicy(const QJsonObject &status)
{
    if (m_role != RoleStudent || status.isEmpty()) {
        return;
    }
    const bool allowed = status.contains(QStringLiteral("allowAgent"))
        ? status.value(QStringLiteral("allowAgent")).toBool()
        : true;
    QSettings settings;
    settings.beginGroup(syncSettingsGroup());
    settings.setValue(QStringLiteral("cachedStudentAgentAllowed"), allowed);
    settings.endGroup();
    emit studentAgentAccessChanged();
}

bool LibrarySyncManager::checkTeacherConnection(QString *message)
{
    QString statusError;
    const QJsonObject status = fetchTeacherStatusObject(&statusError);
    if (status.isEmpty()) {
        if (message) {
            *message = statusError;
        }
        return false;
    }

    applyCachedStudentAgentPolicy(status);

    QString packagesError;
    const QStringList packages = fetchTeacherPackages(&packagesError);
    if (message) {
        if (packagesError.isEmpty()) {
            *message = tr("Соединение с ПК учителя установлено. Доступно пакетов: %1.")
                           .arg(packages.size());
        } else {
            *message = tr("Соединение установлено, но не удалось получить список пакетов: %1")
                           .arg(packagesError);
        }
    }
    return true;
}

bool LibrarySyncManager::extractZipToSitePackages(const QString &zipPath, const QString &sitePackagesDir, QString *errorMessage) const
{
    const QString zipTool = sevenZipPath();
    if (!QFile::exists(zipTool)) {
        if (errorMessage) {
            *errorMessage = tr("Не найден 7za.exe.");
        }
        return false;
    }

    QString outDir = QDir(sitePackagesDir).absolutePath();
    while (outDir.endsWith(QLatin1Char('\\')) || outDir.endsWith(QLatin1Char('/'))) {
        outDir.chop(1);
    }
    QDir().mkpath(outDir);

    const QString nativeZipPath = QDir::toNativeSeparators(zipPath);

    QProcess testProcess;
    testProcess.setProgram(zipTool);
    testProcess.setArguments(QStringList()
        << QStringLiteral("t")
        << QStringLiteral("-tzip")
        << nativeZipPath);
    testProcess.setProcessChannelMode(QProcess::SeparateChannels);
    testProcess.start();
    if (!testProcess.waitForFinished(120000)) {
        testProcess.kill();
        if (errorMessage) {
            *errorMessage = tr("Превышено время ожидания проверки архива.");
        }
        return false;
    }
    if (testProcess.exitStatus() != QProcess::NormalExit || testProcess.exitCode() > 1) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(testProcess.readAllStandardError()).trimmed();
            if (errorMessage->isEmpty()) {
                *errorMessage = tr("Загруженный архив повреждён или имеет неверный формат.");
            }
        }
        return false;
    }

    QProcess process;
    process.setProgram(zipTool);
    process.setArguments(QStringList()
        << QStringLiteral("x")
        << QStringLiteral("-tzip")
        << QStringLiteral("-y")
        << QStringLiteral("-o") + QDir::toNativeSeparators(outDir)
        << nativeZipPath);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();
    if (!process.waitForFinished(300000)) {
        process.kill();
        if (errorMessage) {
            *errorMessage = tr("Превышено время ожидания распаковки пакета.");
        }
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() > 1) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(process.readAllStandardError()).trimmed();
            if (errorMessage->isEmpty()) {
                *errorMessage = tr("Ошибка распаковки пакета.");
            }
        }
        return false;
    }

    return true;
}

bool LibrarySyncManager::downloadAndInstallPackage(const QString &packageName, QString *errorMessage)
{
    if (m_teacherHost.isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("IP-адрес ПК учителя не указан.");
        }
        return false;
    }

    QUrl url(QStringLiteral("http://%1:%2/api/v1/package").arg(m_teacherHost).arg(m_serverPort));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("name"), packageName);
    url.setQuery(query);

    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    QNetworkReply *reply = manager.get(request);

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    timeoutTimer.setInterval(300000);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timeoutTimer.start();
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        if (errorMessage) {
            *errorMessage = tr("Ошибка загрузки %1: превышено время ожидания.").arg(packageName);
        }
        reply->deleteLater();
        return false;
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (errorMessage) {
            *errorMessage = tr("Ошибка загрузки %1: %2").arg(packageName, reply->errorString());
        }
        reply->deleteLater();
        return false;
    }

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatus != 200) {
        if (errorMessage) {
            *errorMessage = tr("Ошибка загрузки %1: сервер вернул код %2.")
                                .arg(packageName)
                                .arg(httpStatus);
        }
        reply->deleteLater();
        return false;
    }

    const QByteArray data = reply->readAll();
    const qint64 expectedSize = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
    reply->deleteLater();

    if (expectedSize > 0 && data.size() != expectedSize) {
        if (errorMessage) {
            *errorMessage = tr("Ошибка загрузки %1: получено %2 из %3 байт.")
                                .arg(packageName)
                                .arg(data.size())
                                .arg(expectedSize);
        }
        return false;
    }

    if (data.size() < 4 || data.at(0) != 'P' || data.at(1) != 'K') {
        if (errorMessage) {
            *errorMessage = tr("Ошибка загрузки %1: получен некорректный архив.").arg(packageName);
        }
        return false;
    }

    const QString zipPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                .filePath(QStringLiteral("vuzhyk_sync_%1.zip")
                                              .arg(QString::number(QDateTime::currentMSecsSinceEpoch())));

    QFile zipFile(zipPath);
    if (!zipFile.open(QIODevice::WriteOnly)) {
        if (errorMessage) {
            *errorMessage = tr("Не удалось создать временный файл.");
        }
        return false;
    }
    if (zipFile.write(data) != data.size()) {
        zipFile.close();
        QFile::remove(zipPath);
        if (errorMessage) {
            *errorMessage = tr("Не удалось сохранить загруженный архив.");
        }
        return false;
    }
    zipFile.close();

    QString siteError;
    const QString sitePackages = resolveSitePackagesDir(&siteError);
    if (sitePackages.isEmpty()) {
        QFile::remove(zipPath);
        if (errorMessage) {
            *errorMessage = siteError;
        }
        return false;
    }

    const bool extracted = extractZipToSitePackages(zipPath, sitePackages, errorMessage);
    QFile::remove(zipPath);
    return extracted;
}

void LibrarySyncManager::triggerSyncNow()
{
    performSync(false);
}

void LibrarySyncManager::triggerForceSyncNow()
{
    performSync(true);
}

void LibrarySyncManager::onSyncTimer()
{
    performSync(false);
}

void LibrarySyncManager::performSync(bool force)
{
    if (m_role != RoleStudent) {
        if (force) {
            emit syncFinished(false, tr("Принудительная синхронизация доступна только на ПК ученика."));
        }
        return;
    }

    if (m_syncInProgress) {
        if (force) {
            emit syncFinished(false, tr("Синхронизация уже выполняется."));
        }
        return;
    }

    if (!force && !m_enabled) {
        return;
    }

    if (m_teacherHost.isEmpty()) {
        if (force) {
            emit syncFinished(false, tr("IP-адрес ПК учителя не указан."));
        }
        return;
    }

    const QJsonObject status = fetchTeacherStatusObject(nullptr);
    if (!status.isEmpty()) {
        applyCachedStudentAgentPolicy(status);
    }

    m_syncInProgress = true;
    emit syncStarted();

    QString errorMessage;
    const QStringList localPackages = fetchLocalPackages(&errorMessage);
    if (localPackages.isEmpty() && !errorMessage.isEmpty()) {
        m_syncInProgress = false;
        emit syncFinished(false, errorMessage);
        return;
    }

    const QStringList teacherPackages = fetchTeacherPackages(&errorMessage);
    if (teacherPackages.isEmpty()) {
        m_syncInProgress = false;
        emit syncFinished(false, errorMessage.isEmpty()
            ? tr("На ПК учителя нет пакетов для синхронизации.")
            : errorMessage);
        return;
    }

    QStringList missing;
    for (const QString &pkg : teacherPackages) {
        if (!localPackages.contains(pkg)) {
            missing.append(pkg);
        }
    }

    if (missing.isEmpty()) {
        m_syncInProgress = false;
        emit syncFinished(true, tr("Все библиотеки уже синхронизированы."));
        return;
    }

    const int total = missing.size();
    int current = 0;
    for (const QString &pkg : missing) {
        ++current;
        emit syncProgress(current, total, pkg);

        QString pkgError;
        if (!downloadAndInstallPackage(pkg, &pkgError)) {
            m_syncInProgress = false;
            emit syncFinished(false, pkgError);
            return;
        }
    }

    m_syncInProgress = false;
    emit syncFinished(true, tr("Синхронизация завершена: установлено пакетов — %1.").arg(total));
}
