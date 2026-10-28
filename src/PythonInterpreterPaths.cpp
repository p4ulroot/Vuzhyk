#include "PythonInterpreterPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

namespace PythonInterpreterPaths {

namespace {

bool pyrobPackageExistsAt(const QString &packageDir)
{
    return QFile::exists(QDir(packageDir).filePath(QStringLiteral("api.py")))
        || QFile::exists(QDir(packageDir).filePath(QStringLiteral("__init__.py")));
}

bool isManagedInterpreterLayout(const QString &pythonExe)
{
    const QString pythonDir = QFileInfo(pythonExe).absolutePath();
    const QDir libDir(QDir(pythonDir).filePath(QStringLiteral("Lib")));
    if (libDir.exists()) {
        return true;
    }

    const QString interpretersRoot =
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("interpreters"));
    const QString normalizedExe = QDir::toNativeSeparators(QFileInfo(pythonExe).absoluteFilePath());
    const QString normalizedRoot = QDir::toNativeSeparators(QDir(interpretersRoot).absolutePath());
    return normalizedExe.startsWith(normalizedRoot, Qt::CaseInsensitive);
}

QString sitePackagesViaPython(const QString &pythonExe, QString *errorMessage)
{
    QProcess process;
    process.setProgram(pythonExe);
    process.setArguments(QStringList()
        << QStringLiteral("-c")
        << QStringLiteral("import site; paths = site.getsitepackages(); print(paths[0] if paths else '')"));
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();

    if (!process.waitForFinished(30000)) {
        process.kill();
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось определить папку site-packages.");
        }
        return {};
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(process.readAllStandardError()).trimmed();
            if (errorMessage->isEmpty()) {
                *errorMessage = QStringLiteral("Не удалось определить папку site-packages.");
            }
        }
        return {};
    }

    const QString sitePackages = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    if (sitePackages.isEmpty() || !QDir(sitePackages).exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Папка site-packages не найдена.");
        }
        return {};
    }

    return QDir(sitePackages).absolutePath();
}

} // namespace

QString embeddedSitePackagesDir(const QString &appDir)
{
    return QDir(appDir).filePath(QStringLiteral("Python38/Lib/site-packages"));
}

QString sitePackagesDir(const QString &pythonExe, bool createIfMissing, QString *errorMessage)
{
    if (pythonExe.isEmpty() || !QFile::exists(pythonExe)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Интерпретатор Python не найден.");
        }
        return {};
    }

    const QString pythonDir = QFileInfo(pythonExe).absolutePath();
    const QStringList localCandidates = {
        QDir(pythonDir).filePath(QStringLiteral("Lib/site-packages")),
        QDir(pythonDir).filePath(QStringLiteral("lib/site-packages")),
    };

    for (const QString &candidate : localCandidates) {
        if (QDir(candidate).exists()) {
            return QDir(candidate).absolutePath();
        }
    }

    if (isManagedInterpreterLayout(pythonExe)) {
        const QString preferred = QDir(pythonDir).filePath(QStringLiteral("Lib/site-packages"));
        if (createIfMissing) {
            QDir().mkpath(preferred);
        }
        if (QDir(preferred).exists()) {
            return QDir(preferred).absolutePath();
        }
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось создать папку Lib/site-packages.");
        }
        return {};
    }

    const QDir libDir(QDir(pythonDir).filePath(QStringLiteral("Lib")));
    if (libDir.exists()) {
        const QString preferred = libDir.filePath(QStringLiteral("site-packages"));
        if (createIfMissing) {
            QDir().mkpath(preferred);
        }
        if (QDir(preferred).exists()) {
            return QDir(preferred).absolutePath();
        }
    }

    return sitePackagesViaPython(pythonExe, errorMessage);
}

bool isPyrobInstalled(const QString &pythonExe, QString *tasksDir)
{
    const QString sitePackages = sitePackagesDir(pythonExe, false, nullptr);
    if (sitePackages.isEmpty()) {
        return false;
    }

    const QString pyrobDir = QDir(sitePackages).filePath(QStringLiteral("pyrob"));
    if (!pyrobPackageExistsAt(pyrobDir)) {
        return false;
    }

    if (tasksDir) {
        *tasksDir = QDir(pyrobDir).filePath(QStringLiteral("tasks"));
    }
    return true;
}

QString pyrobTasksDir(const QString &pythonExe, bool *installed)
{
    QString tasksDir;
    const bool hasPyrob = isPyrobInstalled(pythonExe, &tasksDir);
    if (installed) {
        *installed = hasPyrob;
    }
    return hasPyrob ? tasksDir : QString();
}

} // namespace PythonInterpreterPaths
