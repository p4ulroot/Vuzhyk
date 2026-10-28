#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QTextBrowser>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
class HelpWidget : public QWidget {
    Q_OBJECT

public:
    explicit HelpWidget(QWidget *parent = nullptr);
    void setTheme(const QString &theme);

    QTreeWidget *treeWidget() const { return m_treeWidget; }
    QTextBrowser *contentBrowser() const { return m_contentBrowser; }
    QTreeWidgetItem *findItemByKey(const QString &key) const;
    void showItem(QTreeWidgetItem *item);

private slots:
    void onItemClicked(QTreeWidgetItem *item, int column);

private:
    void setupUi();
    void createTreeWidget();
    void addTreeItems();
    void setupHelpBrowserPaths();
    QString loadExternalHelp(const QString &key) const;
    
    QString getHelpContent(const QString &key);
    QString wrapWithStyles(const QString &content);
    QString highlightPythonCode(const QString &code);
    
    QSplitter *m_splitter {nullptr};
    QTreeWidget *m_treeWidget {nullptr};
    QTextBrowser *m_contentBrowser {nullptr};
    QString m_theme { "light" };
};

