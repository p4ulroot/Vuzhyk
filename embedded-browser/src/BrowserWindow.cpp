#include "BrowserWindow.h"
#include <QApplication>
#include <QUrl>
#include <QMessageBox>
#include <QDesktopServices>
#include <QFileInfo>
#include <QWebEngineSettings>
#include <QWebEngineProfile>

BrowserWindow::BrowserWindow(const QString &initialPath, QWidget *parent)
    : QMainWindow(parent)
    , m_webView(nullptr)
    , m_urlLineEdit(nullptr)
    , m_backButton(nullptr)
    , m_forwardButton(nullptr)
    , m_reloadButton(nullptr)
    , m_goButton(nullptr)
    , m_progressBar(nullptr)
    , m_toolbar(nullptr)
    , m_initialPath(initialPath)
{
    setupUI();
    setupToolbar();
    setupWebView();
    
    loadInitialPath();
}

BrowserWindow::~BrowserWindow()
{
}

void BrowserWindow::setupUI()
{
    setWindowTitle("Embedded Browser Extension");
    resize(1200, 800);
    
    // Создаем центральный виджет
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Создаем веб-представление
    m_webView = new QWebEngineView(centralWidget);
    mainLayout->addWidget(m_webView);
    
    // Создаем прогресс-бар в статусной строке
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_progressBar->setMaximumWidth(200);
    statusBar()->addPermanentWidget(m_progressBar);
}

void BrowserWindow::loadInitialPath()
{
    if (m_initialPath.isEmpty()) {
        m_webView->load(QUrl("https://www.google.com"));
        return;
    }

    QFileInfo info(m_initialPath);
    if (info.exists() && info.isFile()) {
        m_webView->load(QUrl::fromLocalFile(info.absoluteFilePath()));
    } else {
        m_webView->load(QUrl(m_initialPath));
    }
}

void BrowserWindow::setupToolbar()
{
    m_toolbar = addToolBar("Navigation");
    m_toolbar->setMovable(false);
    
    // Кнопка "Назад"
    m_backButton = new QPushButton("←", this);
    m_backButton->setToolTip("Назад");
    m_backButton->setEnabled(false);
    connect(m_backButton, &QPushButton::clicked, this, &BrowserWindow::goBack);
    m_toolbar->addWidget(m_backButton);
    
    // Кнопка "Вперед"
    m_forwardButton = new QPushButton("→", this);
    m_forwardButton->setToolTip("Вперед");
    m_forwardButton->setEnabled(false);
    connect(m_forwardButton, &QPushButton::clicked, this, &BrowserWindow::goForward);
    m_toolbar->addWidget(m_forwardButton);
    
    // Кнопка "Обновить"
    m_reloadButton = new QPushButton("↻", this);
    m_reloadButton->setToolTip("Обновить");
    connect(m_reloadButton, &QPushButton::clicked, this, &BrowserWindow::reload);
    m_toolbar->addWidget(m_reloadButton);
    
    m_toolbar->addSeparator();
    
    // Адресная строка
    m_urlLineEdit = new QLineEdit(this);
    m_urlLineEdit->setPlaceholderText("Введите URL или поисковый запрос...");
    connect(m_urlLineEdit, &QLineEdit::returnPressed, this, &BrowserWindow::navigateToUrl);
    m_toolbar->addWidget(m_urlLineEdit);
    
    // Кнопка "Перейти"
    m_goButton = new QPushButton("Перейти", this);
    connect(m_goButton, &QPushButton::clicked, this, &BrowserWindow::navigateToUrl);
    m_toolbar->addWidget(m_goButton);
}

void BrowserWindow::setupWebView()
{
    // Настраиваем QtWebEngine для разрешения доступа к локальным файлам
    QWebEngineSettings *settings = m_webView->settings();
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    
    connect(m_webView, &QWebEngineView::urlChanged, this, &BrowserWindow::urlChanged);
    connect(m_webView, &QWebEngineView::loadStarted, this, &BrowserWindow::loadStarted);
    connect(m_webView, &QWebEngineView::loadProgress, this, &BrowserWindow::loadProgress);
    connect(m_webView, &QWebEngineView::loadFinished, this, &BrowserWindow::loadFinished);
    connect(m_webView, &QWebEngineView::titleChanged, this, &BrowserWindow::titleChanged);
}

void BrowserWindow::navigateToUrl()
{
    QString urlText = m_urlLineEdit->text().trimmed();
    
    if (urlText.isEmpty()) {
        return;
    }
    
    QUrl url;
    
    // Проверяем, является ли введенный текст URL
    if (urlText.contains("://")) {
        url = QUrl(urlText);
    } else if (urlText.contains(".") && !urlText.contains(" ")) {
        // Похоже на домен, добавляем http://
        url = QUrl("http://" + urlText);
    } else {
        // Поисковый запрос
        url = QUrl("https://www.google.com/search?q=" + QUrl::toPercentEncoding(urlText));
    }
    
    if (url.isValid()) {
        m_webView->load(url);
    } else {
        QMessageBox::warning(this, "Ошибка", "Неверный URL: " + urlText);
    }
}

void BrowserWindow::goBack()
{
    m_webView->back();
}

void BrowserWindow::goForward()
{
    m_webView->forward();
}

void BrowserWindow::reload()
{
    m_webView->reload();
}

void BrowserWindow::urlChanged(const QUrl &url)
{
    m_urlLineEdit->setText(url.toString());
    m_backButton->setEnabled(m_webView->history()->canGoBack());
    m_forwardButton->setEnabled(m_webView->history()->canGoForward());
}

void BrowserWindow::loadStarted()
{
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    statusBar()->showMessage("Загрузка...");
}

void BrowserWindow::loadProgress(int progress)
{
    m_progressBar->setValue(progress);
}

void BrowserWindow::loadFinished(bool success)
{
    m_progressBar->setVisible(false);
    if (success) {
        statusBar()->showMessage("Загрузка завершена", 2000);
    } else {
        statusBar()->showMessage("Ошибка загрузки страницы", 3000);
    }
}

void BrowserWindow::titleChanged(const QString &title)
{
    setWindowTitle(title + " - Embedded Browser Extension");
}



