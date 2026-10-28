#ifndef BROWSERWINDOW_H
#define BROWSERWINDOW_H

#include <QMainWindow>
#include <QWebEngineView>
#include <QWebEngineHistory>
#include <QLineEdit>
#include <QPushButton>
#include <QToolBar>
#include <QProgressBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QUrl>

class BrowserWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit BrowserWindow(const QString &initialPath = QString(), QWidget *parent = nullptr);
    ~BrowserWindow();

private slots:
    void navigateToUrl();
    void goBack();
    void goForward();
    void reload();
    void urlChanged(const QUrl &url);
    void loadStarted();
    void loadProgress(int progress);
    void loadFinished(bool success);
    void titleChanged(const QString &title);

private:
    void setupUI();
    void setupToolbar();
    void setupWebView();
    void loadInitialPath();

    QWebEngineView *m_webView;
    QLineEdit *m_urlLineEdit;
    QPushButton *m_backButton;
    QPushButton *m_forwardButton;
    QPushButton *m_reloadButton;
    QPushButton *m_goButton;
    QProgressBar *m_progressBar;
    QToolBar *m_toolbar;
    QString m_initialPath;
};

#endif // BROWSERWINDOW_H

