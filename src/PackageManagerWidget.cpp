#include "PackageManagerWidget.h"
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QCoreApplication>
#include <QProcessEnvironment>
#include <QDir>
#include <QFileInfo>
#include <QScrollBar>

PackageManagerWidget::PackageManagerWidget(QWidget *parent)
    : QWidget(parent)
    , m_currentProcess(nullptr)
{
    setupUI();
    refreshInstalledPackages();
}

PackageManagerWidget::~PackageManagerWidget()
{
    if (m_currentProcess && m_currentProcess->state() == QProcess::Running) {
        m_currentProcess->kill();
        m_currentProcess->waitForFinished();
    }
}

void PackageManagerWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // Создаем вкладки
    m_tabWidget = new QTabWidget(this);
    
    // === Вкладка "Установленные" ===
    QWidget *installedTab = new QWidget();
    QVBoxLayout *installedLayout = new QVBoxLayout(installedTab);
    
    QLabel *installedLabel = new QLabel(tr("Установленные пакеты:"), installedTab);
    installedLayout->addWidget(installedLabel);
    
    m_installedList = new QListWidget(installedTab);
    m_installedList->setAlternatingRowColors(true);
    installedLayout->addWidget(m_installedList);
    
    QHBoxLayout *installedBtnLayout = new QHBoxLayout();
    m_refreshBtn = new QPushButton(tr("Обновить список"), installedTab);
    m_updateBtn = new QPushButton(tr("Обновить пакет"), installedTab);
    m_uninstallBtn = new QPushButton(tr("Удалить"), installedTab);
    m_updateBtn->setEnabled(false);
    m_uninstallBtn->setEnabled(false);
    installedBtnLayout->addWidget(m_refreshBtn);
    installedBtnLayout->addWidget(m_updateBtn);
    installedBtnLayout->addWidget(m_uninstallBtn);
    installedBtnLayout->addStretch();
    installedLayout->addLayout(installedBtnLayout);
    
    m_installedInfoText = new QTextEdit(installedTab);
    m_installedInfoText->setReadOnly(true);
    m_installedInfoText->setMaximumHeight(100);
    m_installedInfoText->setPlaceholderText(tr("Выберите пакет для просмотра информации"));
    installedLayout->addWidget(m_installedInfoText);
    
    m_tabWidget->addTab(installedTab, tr("Установленные"));
    
    // === Вкладка "Поиск и установка" ===
    QWidget *searchTab = new QWidget();
    QVBoxLayout *searchLayout = new QVBoxLayout(searchTab);
    
    QLabel *searchLabel = new QLabel(tr("Поиск пакетов на PyPI:"), searchTab);
    searchLayout->addWidget(searchLabel);
    
    QHBoxLayout *searchBoxLayout = new QHBoxLayout();
    m_searchBox = new QLineEdit(searchTab);
    m_searchBox->setPlaceholderText(tr("Введите название пакета (например: requests, numpy)"));
    m_searchBtn = new QPushButton(tr("Найти"), searchTab);
    searchBoxLayout->addWidget(m_searchBox);
    searchBoxLayout->addWidget(m_searchBtn);
    searchLayout->addLayout(searchBoxLayout);
    
    m_searchResults = new QListWidget(searchTab);
    m_searchResults->setAlternatingRowColors(true);
    searchLayout->addWidget(m_searchResults);
    
    m_installBtn = new QPushButton(tr("Установить выбранный пакет"), searchTab);
    m_installBtn->setEnabled(false);
    searchLayout->addWidget(m_installBtn);
    
    m_searchInfoText = new QTextEdit(searchTab);
    m_searchInfoText->setReadOnly(true);
    m_searchInfoText->setMaximumHeight(100);
    m_searchInfoText->setPlaceholderText(tr("Выберите пакет для просмотра информации"));
    searchLayout->addWidget(m_searchInfoText);
    
    m_tabWidget->addTab(searchTab, tr("Поиск и установка"));
    
    mainLayout->addWidget(m_tabWidget);
    
    // === Общий вывод процесса ===
    QLabel *outputLabel = new QLabel(tr("Вывод:"), this);
    mainLayout->addWidget(outputLabel);
    
    m_outputText = new QTextEdit(this);
    m_outputText->setReadOnly(true);
    m_outputText->setMaximumHeight(150);
    m_outputText->setPlaceholderText(tr("Здесь будет отображаться вывод команд pip"));
    mainLayout->addWidget(m_outputText);
    
    // Подключаем сигналы
    connect(m_refreshBtn, &QPushButton::clicked, this, &PackageManagerWidget::refreshInstalledPackages);
    connect(m_updateBtn, &QPushButton::clicked, this, &PackageManagerWidget::onUpdateClicked);
    connect(m_uninstallBtn, &QPushButton::clicked, this, &PackageManagerWidget::onUninstallClicked);
    connect(m_searchBtn, &QPushButton::clicked, this, &PackageManagerWidget::onSearchClicked);
    connect(m_installBtn, &QPushButton::clicked, this, &PackageManagerWidget::onInstallClicked);
    connect(m_installedList, &QListWidget::itemClicked, this, &PackageManagerWidget::onInstalledPackageSelected);
    connect(m_searchResults, &QListWidget::itemClicked, this, &PackageManagerWidget::onSearchResultSelected);
    connect(m_searchBox, &QLineEdit::returnPressed, this, &PackageManagerWidget::onSearchClicked);
}

QString PackageManagerWidget::detectPythonExecutable()
{
    // Ищем Python в папке приложения
    QString appDir = QCoreApplication::applicationDirPath();
    QString pythonPath = appDir + "/Python38/python.exe";
    
    if (QFile::exists(pythonPath)) {
        return pythonPath;
    }
    
    // Пробуем системный Python
    return "python";
}

void PackageManagerWidget::refreshInstalledPackages()
{
    loadInstalledPackages();
}

void PackageManagerWidget::loadInstalledPackages()
{
    m_installedList->clear();
    m_installedPackages.clear();
    m_outputText->append(tr("Загрузка списка установленных пакетов..."));
    
    QString python = detectPythonExecutable();
    
    QString script = 
        "import json\n"
        "import sys\n"
        "try:\n"
        "    import subprocess\n"
        "    result = subprocess.run([sys.executable, '-m', 'pip', 'list', '--format=json'], "
        "                            capture_output=True, text=True, encoding='utf-8')\n"
        "    print(result.stdout)\n"
        "except Exception as e:\n"
        "    print('[]')\n";
    
    QProcess *process = new QProcess(this);
    process->setProgram(python);
    process->setArguments(QStringList() << "-c" << script);
    
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");
    process->setProcessEnvironment(env);
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this, process](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            QByteArray output = process->readAllStandardOutput();
            QJsonDocument doc = QJsonDocument::fromJson(output);
            
            if (doc.isArray()) {
                QJsonArray packages = doc.array();
                for (const QJsonValue &val : packages) {
                    QJsonObject pkg = val.toObject();
                    QString name = pkg["name"].toString();
                    QString version = pkg["version"].toString();
                    
                    PackageInfo info;
                    info.name = name;
                    info.version = version;
                    m_installedPackages[name] = info;
                    
                    m_installedList->addItem(QString("%1 (%2)").arg(name, version));
                }
                m_outputText->append(tr("Загружено пакетов: %1").arg(packages.size()));
            }
        } else {
            m_outputText->append(tr("Ошибка загрузки списка пакетов"));
        }
        process->deleteLater();
    });
    
    process->start();
}

void PackageManagerWidget::onSearchClicked()
{
    QString query = m_searchBox->text().trimmed();
    if (query.isEmpty()) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Введите название пакета для поиска"));
        return;
    }
    
    searchOnPyPI(query);
}

void PackageManagerWidget::searchOnPyPI(const QString &query)
{
    m_searchResults->clear();
    m_searchResultsData.clear();
    m_outputText->append(tr("Поиск пакета '%1' на PyPI...").arg(query));
    
    QString python = detectPythonExecutable();
    
    // Формируем список вариантов для поиска
    QStringList variants;
    variants << query;  // Исходный запрос
    
    // Добавляем варианты с цифрами (0-9)
    for (int i = 0; i <= 9; ++i) {
        variants << query + QString::number(i);
    }
    
    // Добавляем популярные суффиксы и префиксы
    variants << query + "-python";
    variants << "python-" + query;
    variants << "py" + query;
    variants << query + "lib";
    variants << query.toLower();
    variants << query.toUpper();
    variants << query.left(1).toUpper() + query.mid(1).toLower();  // Capitalize
    
    // Удаляем дубликаты
    variants.removeDuplicates();
    
    // Формируем Python список для поиска
    QString variantsList = "[";
    for (int i = 0; i < variants.size(); ++i) {
        if (i > 0) variantsList += ", ";
        variantsList += "'" + variants[i].replace("'", "\\'") + "'";
    }
    variantsList += "]";
    
    // Используем PyPI JSON API для получения информации о пакетах
    QString script = QString(
        "import json\n"
        "import sys\n"
        "import urllib.request\n"
        "\n"
        "variants = %1\n"
        "results = []\n"
        "seen = set()\n"
        "\n"
        "for variant in variants:\n"
        "    try:\n"
        "        url = f'https://pypi.org/pypi/{variant}/json'\n"
        "        with urllib.request.urlopen(url, timeout=3) as response:\n"
        "            data = json.loads(response.read().decode('utf-8'))\n"
        "            info = data['info']\n"
        "            name = info.get('name', '')\n"
        "            # Избегаем дубликатов (разные варианты могут вести к одному пакету)\n"
        "            if name.lower() not in seen:\n"
        "                seen.add(name.lower())\n"
        "                result = {\n"
        "                    'name': name,\n"
        "                    'version': info.get('version', ''),\n"
        "                    'summary': info.get('summary', ''),\n"
        "                    'author': info.get('author', ''),\n"
        "                    'home_page': info.get('home_page', ''),\n"
        "                    'license': info.get('license', ''),\n"
        "                    'requires_python': info.get('requires_python', '')\n"
        "                }\n"
        "                results.append(result)\n"
        "    except:\n"
        "        pass\n"
        "\n"
        "print(json.dumps(results, ensure_ascii=False))\n"
    ).arg(variantsList);
    
    QProcess *process = new QProcess(this);
    process->setProgram(python);
    process->setArguments(QStringList() << "-c" << script);
    
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");
    process->setProcessEnvironment(env);
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this, process, query](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            QByteArray output = process->readAllStandardOutput();
            QJsonDocument doc = QJsonDocument::fromJson(output);
            
            if (doc.isArray()) {
                QJsonArray results = doc.array();
                if (results.isEmpty()) {
                    m_outputText->append(tr("Пакеты по запросу '%1' не найдены на PyPI").arg(query));
                } else {
                    for (const QJsonValue &val : results) {
                        QJsonObject result = val.toObject();
                        
                        PackageInfo info;
                        info.name = result["name"].toString();
                        info.version = result["version"].toString();
                        info.summary = result["summary"].toString();
                        info.author = result["author"].toString();
                        info.homepage = result["home_page"].toString();
                        info.license = result["license"].toString();
                        info.requires_python = result["requires_python"].toString();
                        
                        m_searchResultsData[info.name] = info;
                        m_searchResults->addItem(QString("%1 (%2)").arg(info.name, info.version));
                    }
                    m_outputText->append(tr("Найдено пакетов: %1").arg(results.size()));
                }
            }
        } else {
            m_outputText->append(tr("Ошибка поиска пакетов"));
        }
        process->deleteLater();
    });
    
    process->start();
}

void PackageManagerWidget::onInstallClicked()
{
    QListWidgetItem *item = m_searchResults->currentItem();
    if (!item) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Выберите пакет для установки"));
        return;
    }
    
    QString text = item->text();
    QString packageName = text.split(" ").first();
    
    int ret = QMessageBox::question(this, tr("Подтверждение"),
        tr("Установить пакет '%1'?").arg(packageName),
        QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        installPackage(packageName);
    }
}

void PackageManagerWidget::installPackage(const QString &packageName)
{
    if (m_currentProcess && m_currentProcess->state() == QProcess::Running) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Дождитесь завершения текущей операции"));
        return;
    }
    
    m_outputText->clear();
    m_outputText->append(tr("Установка пакета '%1'...").arg(packageName));
    
    QString python = detectPythonExecutable();
    
    m_currentProcess = new QProcess(this);
    m_currentProcess->setProgram(python);
    m_currentProcess->setArguments(QStringList() 
        << "-m" << "pip" << "install" << packageName);
    
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");
    m_currentProcess->setProcessEnvironment(env);
    
    connect(m_currentProcess, &QProcess::readyReadStandardOutput, this, &PackageManagerWidget::onProcessOutput);
    connect(m_currentProcess, &QProcess::readyReadStandardError, this, &PackageManagerWidget::onProcessError);
    connect(m_currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PackageManagerWidget::onProcessFinished);
    
    m_currentProcess->start();
}

void PackageManagerWidget::onUninstallClicked()
{
    QListWidgetItem *item = m_installedList->currentItem();
    if (!item) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Выберите пакет для удаления"));
        return;
    }
    
    QString text = item->text();
    QString packageName = text.split(" ").first();
    
    int ret = QMessageBox::question(this, tr("Подтверждение"),
        tr("Удалить пакет '%1'?").arg(packageName),
        QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        uninstallPackage(packageName);
    }
}

void PackageManagerWidget::uninstallPackage(const QString &packageName)
{
    if (m_currentProcess && m_currentProcess->state() == QProcess::Running) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Дождитесь завершения текущей операции"));
        return;
    }
    
    m_outputText->clear();
    m_outputText->append(tr("Удаление пакета '%1'...").arg(packageName));
    
    QString python = detectPythonExecutable();
    
    m_currentProcess = new QProcess(this);
    m_currentProcess->setProgram(python);
    m_currentProcess->setArguments(QStringList() 
        << "-m" << "pip" << "uninstall" << "-y" << packageName);
    
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");
    m_currentProcess->setProcessEnvironment(env);
    
    connect(m_currentProcess, &QProcess::readyReadStandardOutput, this, &PackageManagerWidget::onProcessOutput);
    connect(m_currentProcess, &QProcess::readyReadStandardError, this, &PackageManagerWidget::onProcessError);
    connect(m_currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PackageManagerWidget::onProcessFinished);
    
    m_currentProcess->start();
}

void PackageManagerWidget::onUpdateClicked()
{
    QListWidgetItem *item = m_installedList->currentItem();
    if (!item) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Выберите пакет для обновления"));
        return;
    }
    
    QString text = item->text();
    QString packageName = text.split(" ").first();
    
    int ret = QMessageBox::question(this, tr("Подтверждение"),
        tr("Обновить пакет '%1' до последней версии?").arg(packageName),
        QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        updatePackage(packageName);
    }
}

void PackageManagerWidget::updatePackage(const QString &packageName)
{
    if (m_currentProcess && m_currentProcess->state() == QProcess::Running) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Дождитесь завершения текущей операции"));
        return;
    }
    
    m_outputText->clear();
    m_outputText->append(tr("Обновление пакета '%1'...").arg(packageName));
    
    QString python = detectPythonExecutable();
    
    m_currentProcess = new QProcess(this);
    m_currentProcess->setProgram(python);
    m_currentProcess->setArguments(QStringList() 
        << "-m" << "pip" << "install" << "--upgrade" << packageName);
    
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");
    m_currentProcess->setProcessEnvironment(env);
    
    connect(m_currentProcess, &QProcess::readyReadStandardOutput, this, &PackageManagerWidget::onProcessOutput);
    connect(m_currentProcess, &QProcess::readyReadStandardError, this, &PackageManagerWidget::onProcessError);
    connect(m_currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PackageManagerWidget::onProcessFinished);
    
    m_currentProcess->start();
}

void PackageManagerWidget::onInstalledPackageSelected(QListWidgetItem *item)
{
    if (!item) return;
    
    m_updateBtn->setEnabled(true);
    m_uninstallBtn->setEnabled(true);
    
    QString text = item->text();
    QString packageName = text.split(" ").first();
    
    if (m_installedPackages.contains(packageName)) {
        PackageInfo info = m_installedPackages[packageName];
        QString infoText = QString(
            "<b>Название:</b> %1<br>"
            "<b>Версия:</b> %2<br>"
        ).arg(info.name, info.version);
        m_installedInfoText->setHtml(infoText);
    }
}

void PackageManagerWidget::onSearchResultSelected(QListWidgetItem *item)
{
    if (!item) return;
    
    m_installBtn->setEnabled(true);
    
    QString text = item->text();
    QString packageName = text.split(" ").first();
    
    if (m_searchResultsData.contains(packageName)) {
        PackageInfo info = m_searchResultsData[packageName];
        
        QString pythonReq = info.requires_python.isEmpty() ? 
            tr("не указано") : info.requires_python;
        
        QString infoText = QString(
            "<b>Название:</b> %1<br>"
            "<b>Версия:</b> %2<br>"
            "<b>Требует Python:</b> %3<br>"
            "<b>Описание:</b> %4<br>"
            "<b>Автор:</b> %5<br>"
            "<b>Лицензия:</b> %6<br>"
            "<b>Сайт:</b> <a href='%7'>%7</a>"
        ).arg(info.name, info.version, pythonReq, info.summary, info.author, info.license, info.homepage);
        m_searchInfoText->setHtml(infoText);
    }
}

void PackageManagerWidget::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
        m_outputText->append(tr("\n=== Операция успешно завершена ==="));
        QMessageBox::information(this, tr("Успех"), tr("Операция выполнена успешно"));
        // Обновляем список установленных пакетов
        refreshInstalledPackages();
    } else {
        m_outputText->append(tr("\n=== Операция завершена с ошибкой (код: %1) ===").arg(exitCode));
        QMessageBox::warning(this, tr("Ошибка"), tr("Операция завершена с ошибкой"));
    }
    
    if (m_currentProcess) {
        m_currentProcess->deleteLater();
        m_currentProcess = nullptr;
    }
}

void PackageManagerWidget::onProcessOutput()
{
    if (m_currentProcess) {
        QString output = QString::fromUtf8(m_currentProcess->readAllStandardOutput());
        m_outputText->append(output);
        // Прокручиваем вниз
        m_outputText->verticalScrollBar()->setValue(m_outputText->verticalScrollBar()->maximum());
    }
}

void PackageManagerWidget::onProcessError()
{
    if (m_currentProcess) {
        QString error = QString::fromUtf8(m_currentProcess->readAllStandardError());
        m_outputText->append(error);
        // Прокручиваем вниз
        m_outputText->verticalScrollBar()->setValue(m_outputText->verticalScrollBar()->maximum());
    }
}
