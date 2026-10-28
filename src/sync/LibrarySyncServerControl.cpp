#include "LibrarySyncServerControl.h"

#include "../PythonInterpreterPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace LibrarySyncServerControl {

namespace {

const char kAutostartName[] = "VuzhykLibrarySync";

QString sevenZipPath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("tools/zip/7za.exe"));
}

QString vuzhykConfigDir()
{
    return QDir(qEnvironmentVariable("APPDATA")).filePath(QStringLiteral("Vuzhyk"));
}

} // namespace

QString configFilePath()
{
    return QDir(vuzhykConfigDir()).filePath(QStringLiteral("sync-server.ini"));
}

QString serverExecutablePath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("VuzhykSyncServer.exe"));
}

bool writeServerConfig(const QString &sitePackagesDir, int port, bool allowAgent, QString *errorMessage)
{
    if (sitePackagesDir.isEmpty() || !QDir(sitePackagesDir).exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Папка site-packages не найдена.");
        }
        return false;
    }

    const QString zipPath = sevenZipPath();
    if (!QFile::exists(zipPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не найден 7za.exe: %1").arg(zipPath);
        }
        return false;
    }

    const QDir configDir(vuzhykConfigDir());
    if (!configDir.exists() && !configDir.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось создать папку конфигурации: %1").arg(configDir.absolutePath());
        }
        return false;
    }

    QSettings ini(configFilePath(), QSettings::IniFormat);
    ini.beginGroup(QStringLiteral("server"));
    ini.setValue(QStringLiteral("port"), port);
    ini.setValue(QStringLiteral("site_packages"), QDir::toNativeSeparators(sitePackagesDir));
    ini.setValue(QStringLiteral("seven_zip"), QDir::toNativeSeparators(zipPath));
    ini.setValue(QStringLiteral("allow_agent"), allowAgent ? 1 : 0);
    ini.endGroup();
    ini.sync();

    if (ini.status() != QSettings::NoError) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось записать конфигурацию сервера синхронизации.");
        }
        return false;
    }

    return true;
}

#ifdef Q_OS_WIN
static bool setRunKeyValue(const QString &value, QString *errorMessage)
{
    HKEY key = nullptr;
    const LONG openResult = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        KEY_SET_VALUE,
        &key);
    if (openResult != ERROR_SUCCESS) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось открыть ключ автозагрузки Windows.");
        }
        return false;
    }

    const std::wstring name = QString::fromLatin1(kAutostartName).toStdWString();
    const std::wstring data = value.toStdWString();
    const LONG setResult = RegSetValueExW(
        key,
        name.c_str(),
        0,
        REG_SZ,
        reinterpret_cast<const BYTE *>(data.c_str()),
        static_cast<DWORD>((data.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);

    if (setResult != ERROR_SUCCESS) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось добавить сервер синхронизации в автозагрузку.");
        }
        return false;
    }
    return true;
}

static bool deleteRunKeyValue(QString *errorMessage)
{
    HKEY key = nullptr;
    const LONG openResult = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        KEY_SET_VALUE,
        &key);
    if (openResult != ERROR_SUCCESS) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось открыть ключ автозагрузки Windows.");
        }
        return false;
    }

    const std::wstring name = QString::fromLatin1(kAutostartName).toStdWString();
    const LONG deleteResult = RegDeleteValueW(key, name.c_str());
    RegCloseKey(key);

    if (deleteResult != ERROR_SUCCESS && deleteResult != ERROR_FILE_NOT_FOUND) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось удалить сервер синхронизации из автозагрузки.");
        }
        return false;
    }
    return true;
}
#endif

bool ensureAutostartEnabled(QString *errorMessage)
{
    const QString exePath = serverExecutablePath();
    if (!QFile::exists(exePath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не найден VuzhykSyncServer.exe: %1").arg(exePath);
        }
        return false;
    }

#ifdef Q_OS_WIN
    const QString quoted = QStringLiteral("\"%1\"").arg(QDir::toNativeSeparators(exePath));
    return setRunKeyValue(quoted, errorMessage);
#else
    Q_UNUSED(errorMessage);
    return false;
#endif
}

bool removeAutostart(QString *errorMessage)
{
#ifdef Q_OS_WIN
    return deleteRunKeyValue(errorMessage);
#else
    Q_UNUSED(errorMessage);
    return true;
#endif
}

bool launchServerIfNeeded(QString *errorMessage)
{
    const QString exePath = serverExecutablePath();
    if (!QFile::exists(exePath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не найден VuzhykSyncServer.exe: %1").arg(exePath);
        }
        return false;
    }

    QProcess listProcess;
    listProcess.start(QStringLiteral("tasklist"),
                      QStringList() << QStringLiteral("/FI")
                                    << QStringLiteral("IMAGENAME eq VuzhykSyncServer.exe")
                                    << QStringLiteral("/FO")
                                    << QStringLiteral("CSV")
                                    << QStringLiteral("/NH"));
    if (listProcess.waitForFinished(5000)) {
        const QByteArray output = listProcess.readAllStandardOutput();
        if (output.contains("VuzhykSyncServer.exe")) {
            stopServerProcess();
        }
    }

    const bool started = QProcess::startDetached(exePath, QStringList());
    if (!started && errorMessage) {
        *errorMessage = QStringLiteral("Не удалось запустить сервер синхронизации.");
    }
    return started;
}

bool stopServerProcess()
{
    QProcess killProcess;
    killProcess.start(QStringLiteral("taskkill"),
                      QStringList() << QStringLiteral("/IM")
                                    << QStringLiteral("VuzhykSyncServer.exe")
                                    << QStringLiteral("/F"));
    if (!killProcess.waitForFinished(10000)) {
        killProcess.kill();
        return false;
    }
    return killProcess.exitCode() == 0;
}

void applyFromSettings(QString *errorMessage)
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("sync"));
    const bool enabled = settings.value(QStringLiteral("enabled"), false).toBool();
    const int role = settings.value(QStringLiteral("role"), 1).toInt();
    const int port = settings.value(QStringLiteral("serverPort"), 8765).toInt();
    const QString pythonPath = settings.value(QStringLiteral("teacherInterpreterPath")).toString();
    const bool forbidAgent = settings.value(QStringLiteral("forbidStudentAgent"), false).toBool();
    settings.endGroup();

    if (!enabled || role != 0) {
        removeAutostart(errorMessage);
        stopServerProcess();
        return;
    }

    if (pythonPath.isEmpty() || !QFile::exists(pythonPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Интерпретатор для синхронизации не настроен.");
        }
        return;
    }

    QString siteError;
    const QString sitePackages = PythonInterpreterPaths::sitePackagesDir(pythonPath, true, &siteError);
    if (sitePackages.isEmpty()) {
        if (errorMessage) {
            *errorMessage = siteError;
        }
        return;
    }

    if (!writeServerConfig(sitePackages, port, !forbidAgent, errorMessage)) {
        return;
    }
    if (!ensureAutostartEnabled(errorMessage)) {
        return;
    }
    // Перезапуск, чтобы подхватить обновлённый exe и новый конфиг (ini читается только при старте).
    stopServerProcess();
    launchServerIfNeeded(errorMessage);
}

} // namespace LibrarySyncServerControl
