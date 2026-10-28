#ifndef PACKAGEMANAGERWIDGET_H
#define PACKAGEMANAGERWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QSplitter>
#include <QProcess>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QTabWidget>

class PackageManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PackageManagerWidget(QWidget *parent = nullptr);
    ~PackageManagerWidget();

public slots:
    void refreshInstalledPackages();

private slots:
    void onSearchClicked();
    void onInstallClicked();
    void onUninstallClicked();
    void onUpdateClicked();
    void onInstalledPackageSelected(QListWidgetItem *item);
    void onSearchResultSelected(QListWidgetItem *item);
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessOutput();
    void onProcessError();

private:
    // UI элементы
    QTabWidget *m_tabWidget;
    
    // Вкладка "Установленные"
    QListWidget *m_installedList;
    QPushButton *m_updateBtn;
    QPushButton *m_uninstallBtn;
    QPushButton *m_refreshBtn;
    QTextEdit *m_installedInfoText;
    
    // Вкладка "Поиск"
    QLineEdit *m_searchBox;
    QPushButton *m_searchBtn;
    QListWidget *m_searchResults;
    QPushButton *m_installBtn;
    QTextEdit *m_searchInfoText;
    
    // Общие элементы
    QTextEdit *m_outputText;
    QProcess *m_currentProcess;
    
    // Вспомогательные функции
    void setupUI();
    void loadInstalledPackages();
    void searchOnPyPI(const QString &query);
    void installPackage(const QString &packageName);
    void uninstallPackage(const QString &packageName);
    void updatePackage(const QString &packageName);
    void showPackageInfo(const QString &packageName, bool installed);
    QString detectPythonExecutable();
    
    // Данные
    struct PackageInfo {
        QString name;
        QString version;
        QString summary;
        QString author;
        QString homepage;
        QString license;
        QString requires_python;  // Требуемая версия Python
    };
    
    QMap<QString, PackageInfo> m_installedPackages;
    QMap<QString, PackageInfo> m_searchResultsData;
};

#endif // PACKAGEMANAGERWIDGET_H
