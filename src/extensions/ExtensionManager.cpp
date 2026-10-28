#include "ExtensionManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>

ExtensionManager::ExtensionManager(QObject *parent)
    : QObject(parent)
{
}

const QVector<ExtensionManager::ExtensionInfo> &ExtensionManager::extensions() const
{
    return m_extensions;
}

void ExtensionManager::loadExtensions()
{
    const QString rootPath = extensionsPath();
    QDir extensionsDir(rootPath);
    if (!extensionsDir.exists()) {
        extensionsDir.mkpath(".");
    }

    QVector<ExtensionInfo> discovered;
    const QFileInfoList entries = extensionsDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries) {
        ExtensionInfo info;
        if (loadManifest(QDir(entry.absoluteFilePath()), info)) {
            discovered.append(info);
        } else {
            qWarning() << "ExtensionManager: failed to load extension from" << entry.absoluteFilePath();
        }
    }

    m_extensions = discovered;
    emit extensionsChanged();
}

void ExtensionManager::executeAction(const ExtensionAction &action) const
{
    executeAction(action, QStringList());
}

void ExtensionManager::executeAction(const ExtensionAction &action, const QStringList &extraArguments) const
{
    if (action.executable.isEmpty()) {
        qWarning() << "ExtensionManager: action" << action.id << "has no executable target";
        return;
    }

    QStringList args = action.arguments;
    args.append(extraArguments);
    if (!QProcess::startDetached(action.executable, args)) {
        qWarning() << "ExtensionManager: unable to start" << action.executable;
    }
}

QVector<const ExtensionManager::ExtensionAction *> ExtensionManager::runActionsForFileType(const QString &fileExtension) const
{
    QVector<const ExtensionAction *> result;
    QString ext = fileExtension.trimmed().toLower();
    if (ext.startsWith('.')) {
        ext.remove(0, 1);
    }
    if (ext.isEmpty()) {
        return result;
    }

    for (const ExtensionInfo &extension : m_extensions) {
        if (!extension.enabled) {
            continue;
        }
        for (const ExtensionAction &action : extension.actions) {
            if (action.location.toLower().trimmed() != QLatin1String("run")) {
                continue;
            }
            if (action.fileTypes.isEmpty()) {
                continue;
            }
            if (action.fileTypes.contains(ext)) {
                result.append(&action);
            }
        }
    }
    return result;
}

QString ExtensionManager::extensionsPath() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    QDir candidate(appDir);
    while (candidate.exists()) {
        const QString candidatePath = candidate.filePath("extensions");
        if (QDir(candidatePath).exists()) {
            return QDir(candidatePath).absolutePath();
        }

        if (candidate == QDir(candidate.rootPath())) {
            break;
        }

        if (!candidate.cdUp()) {
            break;
        }
    }

    QDir fallback(appDir);
    fallback.mkpath("extensions");
    return fallback.filePath("extensions");
}

bool ExtensionManager::loadManifest(const QDir &extensionDir, ExtensionInfo &info) const
{
    const QString manifestPath = extensionDir.filePath("extension.json");
    QFile manifestFile(manifestPath);
    if (!manifestFile.exists()) {
        qWarning() << "ExtensionManager: missing manifest" << manifestPath;
        return false;
    }

    if (!manifestFile.open(QIODevice::ReadOnly)) {
        qWarning() << "ExtensionManager: cannot open manifest" << manifestPath;
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll());
    manifestFile.close();
    if (!document.isObject()) {
        qWarning() << "ExtensionManager: invalid manifest" << manifestPath;
        return false;
    }

    const QJsonObject root = document.object();
    info.id = root.value("id").toString(extensionDir.dirName());
    info.name = root.value("name").toString(info.id);
    info.description = root.value("description").toString();
    info.version = root.value("version").toString();
    info.enabled = root.value("enabled").toBool(true);
    info.basePath = extensionDir.absolutePath();

    const QString defaultExecutable = resolveRelativePath(extensionDir, root.value("executable").toString());
    const QJsonArray actions = root.value("actions").toArray();
    for (const QJsonValue &actionValue : actions) {
        if (!actionValue.isObject()) {
            continue;
        }

        const QJsonObject actionObject = actionValue.toObject();
        ExtensionAction action;
        action.id = actionObject.value("id").toString();
        action.label = actionObject.value("label").toString(action.id);
        action.description = actionObject.value("description").toString();
        action.location = actionObject.value("location").toString("tools").toLower().trimmed();
        action.menuTarget = actionObject.value("menu").toString();
        action.iconPath = resolveRelativePath(extensionDir, actionObject.value("icon").toString());
        QString executable = actionObject.value("executable").toString();
        if (executable.isEmpty()) {
            executable = defaultExecutable;
        }
        action.executable = resolveRelativePath(extensionDir, executable);
        action.separatorBefore = actionObject.value("separatorBefore").toBool(false);
        action.extensionId = info.id;
        for (const QJsonValue &ft : actionObject.value("fileTypes").toArray()) {
            QString ext = ft.toString().trimmed().toLower();
            if (ext.startsWith('.')) {
                ext.remove(0, 1);
            }
            if (!ext.isEmpty()) {
                action.fileTypes.append(ext);
            }
        }

        for (const QJsonValue &argument : actionObject.value("arguments").toArray()) {
            action.arguments.append(argument.toString());
        }

        if (action.executable.isEmpty()) {
            qWarning() << "ExtensionManager: action" << action.id << "has no executable";
            continue;
        }

        info.actions.append(action);
    }

    loadStoredEnabledState(info);

    return true;
}

QString ExtensionManager::resolveRelativePath(const QDir &baseDir, const QString &value) const
{
    if (value.isEmpty()) {
        return QString();
    }

    QFileInfo info(value);
    if (info.isAbsolute()) {
        return info.absoluteFilePath();
    }

    return baseDir.filePath(value);
}

bool ExtensionManager::setExtensionEnabled(const QString &extensionId, bool enabled)
{
    ExtensionInfo *extension = findExtension(extensionId);
    if (!extension) {
        return false;
    }

    saveEnabledState(extensionId, enabled);
    loadExtensions();
    emit extensionEnableStateChanged(extensionId, enabled);
    return true;
}

bool ExtensionManager::removeExtension(const QString &extensionId)
{
    ExtensionInfo *extension = findExtension(extensionId);
    if (!extension) {
        return false;
    }

    QDir extensionDir(extension->basePath);
    if (!extensionDir.exists()) {
        removeStoredState(extensionId);
        loadExtensions();
        emit extensionEnableStateChanged(extensionId, false);
        return true;
    }

    const bool removed = extensionDir.removeRecursively();
    if (removed) {
        removeStoredState(extensionId);
        loadExtensions();
        emit extensionEnableStateChanged(extensionId, false);
    }
    return removed;
}

bool ExtensionManager::extensionEnabled(const QString &extensionId) const
{
    const ExtensionInfo *extension = findExtension(extensionId);
    if (!extension) {
        return false;
    }

    return extension->enabled;
}

ExtensionManager::ExtensionInfo *ExtensionManager::findExtension(const QString &extensionId)
{
    for (ExtensionInfo &extension : m_extensions) {
        if (extension.id == extensionId) {
            return &extension;
        }
    }
    return nullptr;
}

const ExtensionManager::ExtensionInfo *ExtensionManager::findExtension(const QString &extensionId) const
{
    for (const ExtensionInfo &extension : m_extensions) {
        if (extension.id == extensionId) {
            return &extension;
        }
    }
    return nullptr;
}

bool ExtensionManager::loadStoredEnabledState(ExtensionInfo &info) const
{
    QSettings settings;
    settings.beginGroup("extensions");
    settings.beginGroup(info.id);
    if (settings.contains("enabled")) {
        info.enabled = settings.value("enabled").toBool();
    }
    settings.endGroup();
    settings.endGroup();
    return info.enabled;
}

void ExtensionManager::saveEnabledState(const QString &extensionId, bool enabled) const
{
    QSettings settings;
    settings.beginGroup("extensions");
    settings.beginGroup(extensionId);
    settings.setValue("enabled", enabled);
    settings.endGroup();
    settings.endGroup();
}

void ExtensionManager::removeStoredState(const QString &extensionId) const
{
    QSettings settings;
    settings.beginGroup("extensions");
    settings.remove(extensionId);
    settings.endGroup();
}

const ExtensionManager::ExtensionAction *ExtensionManager::findActionById(const QString &actionId) const
{
    if (actionId.isEmpty()) {
        return nullptr;
    }

    for (const ExtensionInfo &extension : m_extensions) {
        if (!extension.enabled) {
            continue;
        }

        for (const ExtensionAction &action : extension.actions) {
            if (action.id == actionId) {
                return &action;
            }
        }
    }

    return nullptr;
}

