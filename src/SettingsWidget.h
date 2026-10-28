#pragma once

#include <QWidget>
#include <QListWidget>
#include <QStackedWidget>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QFileDialog>
#include <QListWidget>
#include <QTableWidget>
#include <QKeySequenceEdit>
#include <QMap>
#include <QKeySequence>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>

class ExtensionManager;
class PackageManagerWidget;

class SettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit SettingsWidget(QWidget *parent = nullptr);
    void setTheme(const QString &theme);
    void resetSyncTabAccessOnOpen();
    QMap<QString, QKeySequence> getShortcuts() const;
    void setExtensionManager(ExtensionManager *manager);
    void setAgentFeatureVisible(bool visible);

signals:
    void fontSizeChanged(int size);
    void interpreterChanged();
    void showStatusMessage(const QString &message, int timeoutMs = 3000);
    void closeRequested();
    void shortcutChanged(const QString &actionName, const QKeySequence &sequence);
    void showCloseButtonOnTabsChanged(bool show);
    void syncSettingsChanged();
    void syncCheckConnectionRequested();
    void syncForceRequested();
    void agentSettingsChanged();

private slots:
    void onCategoryChanged(int index);
    void onFontSizeChanged(int size);
    void onInterpreterTypeChanged();
    void onBrowsePythonPath();
    void onInstallInterpreter();
    void onRemoveInterpreter();
    void onInterpreterSelected(int index);
    void refreshInstalledPackages();
    void onInstallPyrobFromEmbedded();
    void onInstallTurtlexyFromEmbedded();
    void onShortcutChanged(int row);
    void onResetShortcut(int row);
    void onRefreshExtensions();
    void refreshExtensionsPage();
    void onSyncSettingsChanged();
    void onSetSyncPassword();
    void onRemoveSyncPassword();
    void onSyncCheckConnection();
    void onSyncForceSync();
    void onAgentSettingsChanged();
    void onAgentTestConnection();
    void onAddOpenRouterKey();
    void onRemoveOpenRouterKey();
    void onAddOpenRouterModel();
    void onRemoveOpenRouterModel();

private:
    void setupUi();
    void loadInterpreterSettings();
    void saveInterpreterSettings();
    void loadShortcutSettings();
    void saveShortcutSettings();
    void setupShortcutsPage();
    QString resolvePythonExecutable() const;
    bool runPythonProcess(const QStringList &arguments, QByteArray &stdOutput, QByteArray &stdError, bool showErrors = true);
    void updatePackageUiEnabled();
    void ensurePackagesInitialized();
    void schedulePackagesRefresh();
    bool isInterpreterPageVisible() const;
    void populateInstalledPackagesList(const QByteArray &jsonData);
    void installEmbeddedLibrary(const QString &packagePrefix, const QString &displayName);
    bool copyPathRecursively(const QString &srcPath, const QString &dstPath) const;
    QString embeddedSitePackagesDir() const;
    QString querySitePackagesDir(const QString &pythonExe, QString *errorMessage) const;
    bool queryPythonVersion(const QString &pythonExe, int *major, int *minor, QString *errorMessage) const;
    
    QListWidget *m_categoryList { nullptr };
    QPushButton *m_closeButton { nullptr };
    QStackedWidget *m_settingsStack { nullptr };
    QString m_currentTheme;
    
    // Interface section
    QWidget *m_interfacePage { nullptr };
    QSpinBox *m_fontSizeSpinBox { nullptr };
    QWidget *m_fontSizeStepWidget { nullptr };
    QPushButton *m_fontSizeIncBtn { nullptr };
    QPushButton *m_fontSizeDecBtn { nullptr };
    QCheckBox *m_closeTabWithMiddleButtonCheckBox { nullptr };
    QCheckBox *m_showCloseButtonOnTabsCheckBox { nullptr };
    
    // Interpreter section
    QWidget *m_interpreterPage { nullptr };
    QButtonGroup *m_interpreterTypeGroup { nullptr };
    QRadioButton *m_embeddedRadio { nullptr };
    QRadioButton *m_installedRadio { nullptr };
    QRadioButton *m_systemRadio { nullptr };
    QRadioButton *m_customRadio { nullptr };
    QLineEdit *m_pythonPathEdit { nullptr };
    QPushButton *m_browseButton { nullptr };
    
    // Interpreter management
    QComboBox *m_interpreterComboBox { nullptr };
    QPushButton *m_installInterpreterBtn { nullptr };
    QPushButton *m_removeInterpreterBtn { nullptr };
    QComboBox *m_pythonVersionComboBox { nullptr };
    QLabel *m_currentInterpreterLabel { nullptr };
    
    QListWidget *m_installedPackagesList { nullptr };
    QPushButton *m_refreshPackagesButton { nullptr };
    QPushButton *m_installPyrobButton { nullptr };
    QPushButton *m_installTurtlexyButton { nullptr };
    
    // Shortcuts section
    QWidget *m_shortcutsPage { nullptr };
    QTableWidget *m_shortcutsTable { nullptr };
    QMap<QString, QKeySequence> m_defaultShortcuts; // Дефолтные горячие клавиши
    QMap<QString, QKeySequence> m_currentShortcuts; // Текущие горячие клавиши
    
    // File associations section
    QWidget *m_associationsPage { nullptr };
    QCheckBox *m_pythonAssociationCheckBox { nullptr };
    
    QWidget *m_extensionsPage { nullptr };
    QTableWidget *m_extensionsTable { nullptr };
    QPushButton *m_refreshExtensionsButton { nullptr };
    ExtensionManager *m_extensionManager { nullptr };
    
    // Package Manager section
    PackageManagerWidget *m_packageManagerWidget { nullptr };

    // Sync section
    QWidget *m_syncPage { nullptr };
    QWidget *m_syncContent { nullptr };
    QCheckBox *m_syncEnabledCheckBox { nullptr };
    QRadioButton *m_syncTeacherRadio { nullptr };
    QRadioButton *m_syncStudentRadio { nullptr };
    QComboBox *m_syncTeacherInterpreterCombo { nullptr };
    QLineEdit *m_syncTeacherIpEdit { nullptr };
    QSpinBox *m_syncIntervalSpinBox { nullptr };
    QCheckBox *m_syncShowTimerCheckBox { nullptr };
    QSpinBox *m_syncPortSpinBox { nullptr };
    QCheckBox *m_syncForbidAgentCheckBox { nullptr };
    QPushButton *m_syncCheckConnectionButton { nullptr };
    QPushButton *m_syncForceSyncButton { nullptr };
    QPushButton *m_syncSetPasswordButton { nullptr };
    QPushButton *m_syncRemovePasswordButton { nullptr };
    QLabel *m_syncPasswordStatusLabel { nullptr };
    bool m_syncTabUnlocked { false };

    // Agent section
    QWidget *m_agentPage { nullptr };
    QComboBox *m_agentProviderCombo { nullptr };
    QWidget *m_agentLmStudioUrlRow { nullptr };
    QLineEdit *m_agentLmStudioUrlEdit { nullptr };
    QGroupBox *m_agentOpenRouterGroup { nullptr };
    QListWidget *m_agentOpenRouterKeysList { nullptr };
    QPushButton *m_agentAddOpenRouterKeyBtn { nullptr };
    QPushButton *m_agentRemoveOpenRouterKeyBtn { nullptr };
    QListWidget *m_agentOpenRouterModelsList { nullptr };
    QPushButton *m_agentAddOpenRouterModelBtn { nullptr };
    QPushButton *m_agentRemoveOpenRouterModelBtn { nullptr };
    QPushButton *m_agentTestConnectionBtn { nullptr };
    QLabel *m_agentConnectionStatusLabel { nullptr };
    QLabel *m_agentHintLabel { nullptr };
    QListWidgetItem *m_agentCategoryItem { nullptr };
    
    bool m_packagesInitialized { false };
    bool m_packagesNeedsRefresh { false };
    
    void setupAssociationsPage();
    void setupExtensionsPage();
    void setupSyncPage();
    void setupAgentPage();
    void loadSyncSettings();
    void saveSyncSettings();
    void loadAgentSettings();
    void saveAgentSettings();
    void updateAgentProviderUi();
    void refreshOpenRouterKeysList();
    void refreshOpenRouterModelsList();
    void updateSyncUi();
    void updateSyncInterpreterLock();
    bool hasSyncTabPassword() const;
    void lockSyncTabIfPasswordProtected();
    bool tryUnlockSyncPage();
    void populateSyncInterpreterCombo();
    void applyTeacherSyncServer();
    bool isStudentSyncEnabled() const;
    int syncCategoryIndex() const { return 2; }
    int agentCategoryIndex() const { return 7; }
    void applyThemeStyles();
    void syncFontSizeControlLayout();
    void onPythonAssociationChanged(bool checked);
    bool setFileAssociation(const QString &extension, const QString &progId, const QString &description, const QString &command);
    bool removeFileAssociation(const QString &extension);
    QString getApplicationPath() const;
    QString extractIconFromResources() const;
    
    // Interpreter management
    void loadInstalledInterpreters();
    void downloadAndInstallPython(const QString &version);
    void extractDownloadedPythonArchive(const QString &version, const QString &targetDir, const QString &archivePath);
    QString getInterpretersDir() const;
    QStringList getAvailablePythonVersions() const;
    
    friend class MainWindow; // Для доступа к m_fontSizeSpinBox
};

