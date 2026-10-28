#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <QStringList>
#include <QDir>

class ExtensionManager : public QObject {
    Q_OBJECT
public:
    struct ExtensionAction {
        QString id;
        QString label;
        QString description;
        QString location;
        QString menuTarget;
        QString iconPath;
        QString executable;
        QStringList arguments;
        QStringList fileTypes;  // расширения файлов, при которых действие доступно (например ["html","htm"])
        bool separatorBefore { false };
        QString extensionId;
    };

    struct ExtensionInfo {
        QString id;
        QString name;
        QString description;
        QString version;
        QString basePath;
        bool enabled { true };
        QVector<ExtensionAction> actions;
    };

    explicit ExtensionManager(QObject *parent = nullptr);

    const QVector<ExtensionInfo> &extensions() const;
    void loadExtensions();
    void executeAction(const ExtensionAction &action) const;
    void executeAction(const ExtensionAction &action, const QStringList &extraArguments) const;
    bool setExtensionEnabled(const QString &extensionId, bool enabled);
    bool removeExtension(const QString &extensionId);
    bool extensionEnabled(const QString &extensionId) const;
    const ExtensionAction *findActionById(const QString &actionId) const;
    QVector<const ExtensionAction *> runActionsForFileType(const QString &fileExtension) const;

signals:
    void extensionsChanged();
    void extensionEnableStateChanged(const QString &extensionId, bool enabled);

private:
    ExtensionInfo *findExtension(const QString &extensionId);
    const ExtensionInfo *findExtension(const QString &extensionId) const;
    QString extensionsPath() const;
    bool loadManifest(const QDir &extensionDir, ExtensionInfo &info) const;
    QString resolveRelativePath(const QDir &baseDir, const QString &value) const;
    bool loadStoredEnabledState(ExtensionInfo &info) const;
    void saveEnabledState(const QString &extensionId, bool enabled) const;
    void removeStoredState(const QString &extensionId) const;

    QVector<ExtensionInfo> m_extensions;
};

