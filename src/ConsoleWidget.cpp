#include "ConsoleWidget.h"
#include <QApplication>
#include <QKeyEvent>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QScrollBar>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QThread>
#include <QTimer>
#include <QTextCodec>
#ifdef _WIN32
#include <windows.h>
#endif

ConsoleWidget::ConsoleWidget(QWidget *parent)
    : QWidget(parent)
    , m_output(new QPlainTextEdit(this))
    , m_input(new QLineEdit(this))
    , m_isRunningCommand(false)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    m_output->setReadOnly(true);
    m_output->setFont(QFont("Consolas", 10));
    m_output->setStyleSheet(
        "QPlainTextEdit {"
        "    background-color: #1e1e1e;"
        "    color: #d4d4d4;"
        "    border: none;"
        "    selection-background-color: #264f78;"
        "}"
    );
    
    m_input->setFont(QFont("Consolas", 10));
    m_input->setStyleSheet(
        "QLineEdit {"
        "    background-color: #252526;"
        "    color: #cccccc;"
        "    border: 1px solid #3e3e42;"
        "    padding: 4px;"
        "}"
        "QLineEdit:focus {"
        "    border: 1px solid #007acc;"
        "}"
    );
    m_input->setPlaceholderText(tr("Введите команду..."));
    
    connect(m_input, &QLineEdit::returnPressed, this, &ConsoleWidget::onInputReturnPressed);
    m_input->installEventFilter(this);
    
    layout->addWidget(m_output);
    layout->addWidget(m_input);
    
    m_currentDirectory = QDir::currentPath();
    
    // Консоль будет запущена по требованию при первом открытии вкладки
}

ConsoleWidget::~ConsoleWidget() {
    if (m_process) {
        m_process->terminate();
        m_process->waitForFinished(1000);
    }
}

void ConsoleWidget::setPythonPath(const QString &pythonPath) {
    // Не перезапускаем консоль, если путь не изменился и консоль уже запущена
    if (m_pythonPath == pythonPath && m_process && m_process->state() == QProcess::Running) {
        return;
    }
    
    bool pathChanged = (m_pythonPath != pythonPath);
    m_pythonPath = pythonPath;
    
    // Если путь изменился и консоль запущена, перезапускаем её для обновления PATH
    if (pathChanged && m_process && m_process->state() == QProcess::Running) {
        // Сохраняем состояние команды
        bool wasRunning = m_isRunningCommand;
        m_isRunningCommand = false;
        
        // Завершаем старый процесс
        m_process->disconnect();
        m_process->terminate();
        m_process->waitForFinished(1000);
        m_process->deleteLater();
        m_process = nullptr;
        
        // Перезапускаем консоль с новым PATH
        QTimer::singleShot(100, this, &ConsoleWidget::startConsole);
    }
    // Если консоль еще не запущена, просто сохраняем путь - консоль запустится при открытии вкладки
}

void ConsoleWidget::setWorkingDirectory(const QString &dir) {
    m_currentDirectory = dir;
    // Обновляем рабочую директорию процесса, если он запущен
    if (m_process && m_process->state() == QProcess::Running) {
        // Не можем изменить рабочую директорию запущенного процесса
        // Используем cd в командах
    }
}

void ConsoleWidget::ensureStarted() {
    // Запускаем консоль только если она еще не запущена
    if (!m_process || m_process->state() != QProcess::Running) {
        startConsole();
    }
}

void ConsoleWidget::clear() {
    m_output->clear();
}

void ConsoleWidget::write(const QString &text) {
    appendOutput(text, false);
}

void ConsoleWidget::writeCommand(const QString &command) {
    // Если консоль не запущена, запускаем и ждем готовности
    if (!m_process || m_process->state() != QProcess::Running) {
        startConsole();
        // Ждем запуска консоли (максимум 3 секунды)
        int waitTime = 0;
        while ((!m_process || m_process->state() != QProcess::Running) && waitTime < 3000) {
            QApplication::processEvents();
            QThread::msleep(50);
            waitTime += 50;
        }
    }
    
    if (m_process && m_process->state() == QProcess::Running) {
        // НЕ выводим команду вручную - cmd.exe сам эхо-выведет её
        // appendInput(command); // Убрано - cmd.exe сам выводит команды
        m_isRunningCommand = true;
        // Отправляем команду в консоль
        // Используем UTF-8 для правильной обработки путей с кириллицей
        QByteArray cmdBytes = (command + "\r\n").toUtf8();
        m_process->write(cmdBytes);
        m_process->waitForBytesWritten(1000);
        
        // Даем консоли время обработать команду
        QApplication::processEvents();
    } else {
        // Если консоль не запустилась, выводим ошибку
        appendOutput(tr("Ошибка: не удалось запустить консоль\n"), true);
        emit outputReceived(tr("Ошибка: не удалось запустить консоль\n"), true);
    }
}

bool ConsoleWidget::isRunning() const {
    return m_process && m_process->state() == QProcess::Running && m_isRunningCommand;
}

void ConsoleWidget::terminate() {
    if (m_process && m_process->state() == QProcess::Running) {
        // Отправляем Ctrl+C в консоль
        m_process->write("\x03");
        m_isRunningCommand = false;
    }
}

void ConsoleWidget::startConsole() {
    if (m_process) {
        m_process->disconnect();
        m_process->deleteLater();
    }
    
    m_process = new QProcess(this);
    
    // Добавляем флаг CREATE_NO_WINDOW для предотвращения создания conhost.exe
#ifdef _WIN32
    m_process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#endif
    
    // Настраиваем окружение
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    
    // Добавляем директорию Python и Scripts в начало PATH (чтобы выбранный Python имел приоритет)
    QString pythonDir = getPythonDirectory();
    if (!pythonDir.isEmpty()) {
        QString systemPath = env.value("PATH");
        QString newPath;
        
        // Добавляем директорию Python в начало
        newPath = pythonDir;
        
        // Также добавляем директорию Scripts (для pip и других утилит)
        QFileInfo pythonInfo(m_pythonPath);
        QString scriptsDir = pythonInfo.absolutePath() + "/Scripts";
        QDir scriptsDirObj(scriptsDir);
        if (scriptsDirObj.exists()) {
            // Добавляем Scripts после директории Python
            newPath += ";" + scriptsDir;
        }
        
        // Добавляем остальной системный PATH
        if (!systemPath.isEmpty()) {
            newPath += ";" + systemPath;
        }
        
        env.insert("PATH", newPath);
    }
    
    m_process->setProcessEnvironment(env);
    m_process->setWorkingDirectory(m_currentDirectory);
    
    // Запускаем cmd.exe с /K для интерактивного режима
    m_process->setProgram("cmd.exe");
    m_process->setArguments(QStringList() << "/K" << "echo.");
    
    // Объединяем stdout и stderr для точного отображения всего вывода cmd
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    
    // Подключаемся к readyRead для чтения всех данных (объединенных)
    connect(m_process, &QProcess::readyRead, this, &ConsoleWidget::onProcessReadyRead);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ConsoleWidget::onProcessFinished);
    
    // Запускаем асинхронно, не ждем
    m_process->start();
    
    // Очищаем начальный вывод после небольшой задержки (асинхронно)
    // Используем флаг, чтобы не очищать, если уже запущена команда
    QTimer::singleShot(500, this, [this]() {
        if (m_process && m_process->state() == QProcess::Running && !m_isRunningCommand) {
            // Очищаем только если нет запущенной команды
            m_output->clear();
            appendOutput(QString("Python: %1\n").arg(m_pythonPath.isEmpty() ? "python" : m_pythonPath), false);
            appendOutput(QString("Рабочая директория: %1\n\n").arg(m_currentDirectory), false);
        }
    });
}

void ConsoleWidget::onProcessReadyRead() {
    // Читаем все данные (stdout и stderr объединены)
    QByteArray data = m_process->readAll();
    
    if (data.isEmpty()) {
        return;
    }
    
    // Для Windows cmd.exe используем правильную кодировку консоли
    // Сначала пробуем CP866 (кодировка консоли Windows для русской локали)
    // Затем CP1251 (Windows-1251 для русской локали)
    // Затем локальную кодировку системы
    // И в последнюю очередь UTF-8
    QString text;
    
    // Пробуем CP866 (основная кодировка консоли Windows)
    QTextCodec *codec866 = QTextCodec::codecForName("IBM866");
    if (codec866) {
        text = codec866->toUnicode(data);
        // Проверяем, что декодирование прошло успешно (нет заменяющих символов)
        if (text.contains(QChar::ReplacementCharacter)) {
            text.clear();
        }
    }
    
    // Если CP866 не подошла, пробуем CP1251
    if (text.isEmpty()) {
        QTextCodec *codec1251 = QTextCodec::codecForName("Windows-1251");
        if (codec1251) {
            text = codec1251->toUnicode(data);
            if (text.contains(QChar::ReplacementCharacter)) {
                text.clear();
            }
        }
    }
    
    // Если не получилось, пробуем локальную кодировку системы
    if (text.isEmpty()) {
        text = QString::fromLocal8Bit(data);
        if (text.contains(QChar::ReplacementCharacter)) {
            text.clear();
        }
    }
    
    // В последнюю очередь пробуем UTF-8
    if (text.isEmpty()) {
        text = QString::fromUtf8(data);
    }
    
    if (!text.isEmpty()) {
        // Определяем, является ли это ошибкой
        bool isError = text.contains("Error") || text.contains("Traceback") || 
                      text.contains("Exception") || text.contains("Traceback (most recent call last)") ||
                      text.contains("Ошибка") || text.contains("ошибка");
        
        // Выводим текст точно так, как он пришел из cmd
        appendOutput(text, isError);
        
        // Отправляем сигнал для парсинга ошибок
        emit outputReceived(text, isError);
        
        // Проверяем, завершилась ли команда по маркеру [EXIT_CODE:...]
        if (m_isRunningCommand && text.contains("[EXIT_CODE:")) {
            // Извлекаем код возврата
            QRegExp rx("\\[EXIT_CODE:(\\d+)\\]");
            int exitCode = 0;
            if (rx.indexIn(text) != -1) {
                exitCode = rx.cap(1).toInt();
            }
            m_isRunningCommand = false;
            emit commandFinished(exitCode);
        }
    }
}

void ConsoleWidget::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(status);
    bool wasRunning = m_isRunningCommand;
    m_isRunningCommand = false;
    
    // Если это была команда (не просто закрытие консоли), отправляем сигнал
    if (wasRunning) {
        emit commandFinished(exitCode);
    }
    
    // Перезапускаем консоль, если она была закрыта
    if (status == QProcess::CrashExit) {
        QThread::msleep(500);
        startConsole();
    }
}

bool ConsoleWidget::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_input && event->type() == QEvent::KeyPress) {
        const auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Up) {
            navigateHistory(-1);
            return true;
        }
        if (keyEvent->key() == Qt::Key_Down) {
            navigateHistory(1);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ConsoleWidget::addToHistory(const QString &command) {
    if (command.isEmpty()) {
        return;
    }
    if (!m_commandHistory.isEmpty() && m_commandHistory.last() == command) {
        m_historyIndex = -1;
        m_historyDraft.clear();
        return;
    }
    m_commandHistory.append(command);
    const int maxHistorySize = 1000;
    if (m_commandHistory.size() > maxHistorySize) {
        m_commandHistory.removeFirst();
    }
    m_historyIndex = -1;
    m_historyDraft.clear();
}

void ConsoleWidget::navigateHistory(int direction) {
    if (m_commandHistory.isEmpty()) {
        return;
    }

    if (m_historyIndex < 0 && direction < 0) {
        m_historyDraft = m_input->text();
    }

    if (direction < 0) {
        if (m_historyIndex < 0) {
            m_historyIndex = m_commandHistory.size() - 1;
        } else if (m_historyIndex > 0) {
            --m_historyIndex;
        }
    } else {
        if (m_historyIndex < 0) {
            return;
        }
        if (m_historyIndex < m_commandHistory.size() - 1) {
            ++m_historyIndex;
        } else {
            m_historyIndex = -1;
            m_input->setText(m_historyDraft);
            m_historyDraft.clear();
            return;
        }
    }

    if (m_historyIndex >= 0 && m_historyIndex < m_commandHistory.size()) {
        m_input->setText(m_commandHistory.at(m_historyIndex));
    }
}

void ConsoleWidget::onInputReturnPressed() {
    const QString rawCommand = m_input->text().trimmed();
    QString command = rawCommand;
    if (command.isEmpty()) {
        // Отправляем просто Enter
        if (m_process && m_process->state() == QProcess::Running) {
            m_process->write("\r\n");
        }
        return;
    }
    
    // Перехватываем команду pip и заменяем на python -m pip для использования выбранного интерпретатора
    // Это гарантирует, что pip будет использовать правильный Python
    if (command.startsWith("pip ", Qt::CaseInsensitive) || command == "pip") {
        QString pythonCmd = m_pythonPath.isEmpty() ? "python" : QString("\"%1\"").arg(m_pythonPath);
        // Заменяем "pip" на "python -m pip"
        command = command.replace(QRegularExpression("^pip\\s+", QRegularExpression::CaseInsensitiveOption), 
                                  pythonCmd + " -m pip ");
    }

    addToHistory(rawCommand);
    writeCommand(command);
    m_input->clear();
}

void ConsoleWidget::appendOutput(const QString &text, bool isError) {
    QTextCursor cursor = m_output->textCursor();
    cursor.movePosition(QTextCursor::End);
    
    QTextCharFormat format;
    if (isError) {
        format.setForeground(QColor("#f48771"));
    } else {
        format.setForeground(QColor("#d4d4d4"));
    }
    cursor.setCharFormat(format);
    
    cursor.insertText(text);
    
    // Устанавливаем курсор в конец и прокручиваем к нему
    m_output->moveCursor(QTextCursor::End);
    m_output->ensureCursorVisible();
    
    // Дополнительно прокручиваем вниз для гарантии
    QScrollBar *scrollBar = m_output->verticalScrollBar();
    if (scrollBar) {
        scrollBar->setValue(scrollBar->maximum());
    }
}

void ConsoleWidget::appendInput(const QString &text) {
    appendOutput(QString("> %1\n").arg(text), false);
}

QString ConsoleWidget::getPythonDirectory() const {
    if (m_pythonPath.isEmpty()) {
        return QString();
    }
    
    QFileInfo pythonInfo(m_pythonPath);
    return pythonInfo.absolutePath();
}

