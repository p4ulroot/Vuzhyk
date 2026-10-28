#pragma once

#include <QObject>
#include <QTimer>
#include <QStringList>
#include <QJsonObject>

class LibrarySyncManager : public QObject {
    Q_OBJECT
public:
    enum Role {
        RoleTeacher = 0,
        RoleStudent = 1
    };

    explicit LibrarySyncManager(QObject *parent = nullptr);

    void reloadSettings();
    void applyTeacherServerSettings();

    bool isSyncEnabled() const { return m_enabled; }
    bool isStudentMode() const { return m_role == RoleStudent; }
    bool isTeacherMode() const { return m_role == RoleTeacher; }
    bool showSyncTimer() const { return m_showSyncTimer; }
    bool isSyncInProgress() const { return m_syncInProgress; }
    int secondsUntilNextSync() const;
    bool checkTeacherConnection(QString *message = nullptr);

    bool isStudentAgentAllowed() const;

signals:
    void syncProgress(int current, int total, const QString &packageName);
    void syncFinished(bool success, const QString &message);
    void syncStarted();
    void studentAgentAccessChanged();

public slots:
    void triggerSyncNow();
    void triggerForceSyncNow();

private slots:
    void onSyncTimer();

private:
    void performSync(bool force);
    QString resolvePythonExecutable() const;
    QString resolveSitePackagesDir(QString *errorMessage = nullptr) const;
    QStringList fetchLocalPackages(QString *errorMessage = nullptr) const;
    QStringList fetchTeacherPackages(QString *errorMessage = nullptr) const;
    QJsonObject fetchTeacherStatusObject(QString *errorMessage = nullptr) const;
    void applyCachedStudentAgentPolicy(const QJsonObject &status);
    bool downloadAndInstallPackage(const QString &packageName, QString *errorMessage);
    bool extractZipToSitePackages(const QString &zipPath, const QString &sitePackagesDir, QString *errorMessage) const;
    QString sevenZipPath() const;

    QTimer m_syncTimer;
    bool m_enabled { false };
    Role m_role { RoleStudent };
    QString m_teacherHost;
    int m_intervalMinutes { 5 };
    int m_serverPort { 8765 };
    QString m_teacherInterpreterPath;
    bool m_showSyncTimer { false };
    bool m_syncInProgress { false };
};
