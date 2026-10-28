#include "TurtleTaskWidget.h"
#include "TurtleCanvasHost.h"
#include "../CodeEditor.h"

#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QList>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QScrollArea>
#include <QSize>
#include <QSizePolicy>
#include <QSplitter>
#include <QSvgRenderer>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QIcon taskPanelIcon(const QString &fileName, const QString &color)
{
    QFile file(QStringLiteral(":/icons/icons/light-theme/") + fileName);
    if (!file.open(QIODevice::ReadOnly))
        return QIcon();
    QByteArray svgData = file.readAll();
    file.close();
    if (color != QLatin1String("#000000") && color != QLatin1String("black")) {
        QString svg = QString::fromUtf8(svgData);
        svg.replace(QStringLiteral("#000000"), color, Qt::CaseInsensitive);
        svg.replace(QStringLiteral("black"), color, Qt::CaseInsensitive);
        svg.replace(QStringLiteral("rgb(0,0,0)"), color, Qt::CaseInsensitive);
        svgData = svg.toUtf8();
    }
    QSvgRenderer renderer(svgData);
    if (!renderer.isValid())
        return QIcon();
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter);
    painter.end();
    QIcon icon;
    icon.addPixmap(pixmap);
    return icon;
}

QString defaultInstruction()
{
    return QString::fromUtf8(
        "Перед тобой готовая программа. Она перемещает черепашку на 100 шагов вперёд. "
        "Синяя линия на экране — это образец результата. Выполни программу и посмотри, "
        "как черепашка рисует свою линию поверх образца.");
}

} // namespace

QString taskSamplePath(const TurtleTaskDefinition &task)
{
    if (!task.samplePath.isEmpty())
        return task.samplePath;
    if (task.sampleForward > 0)
        return QStringLiteral("F%1").arg(task.sampleForward);
    return QString();
}

TurtleTaskDefinition makeTurtleTask(const QString &id,
                                    const QString &title,
                                    const QString &instruction,
                                    const QString &starterCode,
                                    const QString &samplePath)
{
    TurtleTaskDefinition task;
    task.id = id;
    task.title = title;
    task.instruction = instruction;
    task.starterCode = starterCode;
    task.samplePath = samplePath;
    task.sampleForward = 0;
    task.checkId = QStringLiteral("match_sample");
    return task;
}

TurtleTaskDefinition turtleTaskForward100()
{
    TurtleTaskDefinition task = makeTurtleTask(
        QStringLiteral("turtle_forward_100"),
        QString::fromUtf8("Черепаха: 100 шагов"),
        defaultInstruction(),
        QStringLiteral("from turtle import *\nforward(100)\n"),
        QStringLiteral("F100"));
    task.sampleForward = 100;
    return task;
}

QVector<TurtleTaskChapter> turtleTaskCatalog()
{
    TurtleTaskChapter chapter;
    chapter.title = QString::fromUtf8("Знакомство с черепахой");
    chapter.tasks.append(turtleTaskForward100());
    chapter.tasks.append(makeTurtleTask(
        QStringLiteral("turtle_fix_forward_100"),
        QString::fromUtf8("Исправь ошибку"),
        QString::fromUtf8(
            "Эта программа должна перемещать черепашку на 100 шагов вперёд. "
            "Но в ней есть ошибка. Выполни программу, чтобы найти строку с ошибкой и исправить её."),
        QStringLiteral("from turtle import *\nforward()\n"),
        QStringLiteral("F100")));
    chapter.tasks.append(makeTurtleTask(
        QStringLiteral("turtle_change_distance"),
        QString::fromUtf8("Другое расстояние"),
        QString::fromUtf8(
            "Число в скобках — это сколько шагов сделает черепашка. "
            "Сейчас она проходит слишком далеко. Подбери число, чтобы линия совпала с синим образцом."),
        QStringLiteral("from turtle import *\nforward(100)\n"),
        QStringLiteral("F60")));
    chapter.tasks.append(makeTurtleTask(
        QStringLiteral("turtle_corner"),
        QString::fromUtf8("Уголок"),
        QString::fromUtf8(
            "Черепашка умеет поворачивать. Добавь поворот налево на 90 градусов "
            "и ещё один шаг вперёд на 70, чтобы получился уголок как на образце."),
        QStringLiteral("from turtle import *\nforward(70)\n"),
        QStringLiteral("F70 L90 F70")));
    chapter.tasks.append(makeTurtleTask(
        QStringLiteral("turtle_backward"),
        QString::fromUtf8("Назад"),
        QString::fromUtf8(
            "Черепашка может идти не только вперёд. "
            "Нарисуй такую же линию командой backward. Число шагов подбери по образцу."),
        QStringLiteral("from turtle import *\n"),
        QStringLiteral("B80")));
    chapter.tasks.append(makeTurtleTask(
        QStringLiteral("turtle_square"),
        QString::fromUtf8("Квадрат"),
        QString::fromUtf8(
            "Нарисуй квадрат по образцу: четыре одинаковые стороны по 70 шагов "
            "и четыре поворота налево на 90 градусов."),
        QStringLiteral("from turtle import *\nforward(70)\n"),
        QStringLiteral("F70 L90 F70 L90 F70 L90 F70")));
    return QVector<TurtleTaskChapter>() << chapter;
}

QString TurtleTaskWidget::tabTitle() const
{
    return QString::fromUtf8("Задачник черепаха");
}

TurtleTaskDefinition TurtleTaskWidget::resolveInitialTask(const TurtleTaskDefinition &task) const
{
    for (const TurtleTaskChapter &chapter : m_catalog) {
        for (const TurtleTaskDefinition &item : chapter.tasks) {
            if (item.id == task.id)
                return item;
        }
    }
    if (!m_catalog.isEmpty() && !m_catalog.first().tasks.isEmpty())
        return m_catalog.first().tasks.first();
    return task;
}

TurtleTaskWidget::TurtleTaskWidget(const TurtleTaskDefinition &task, QWidget *parent)
    : QWidget(parent)
{
    m_catalog = turtleTaskCatalog();
    m_task = resolveInitialTask(task);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);

    m_editor = new CodeEditor(m_splitter);

    m_panel = new QWidget(m_splitter);
    m_panel->setMinimumWidth(260);
    m_panel->setMaximumWidth(460);
    auto *panelLayout = new QVBoxLayout(m_panel);
    panelLayout->setContentsMargins(10, 10, 10, 10);
    panelLayout->setSpacing(8);

    m_instruction = new QLabel(m_task.instruction, m_panel);
    m_instruction->setWordWrap(true);
    m_instruction->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_instruction->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_instruction->setMinimumHeight(48);

    m_canvas = new TurtleCanvasHost(m_panel);
    m_canvas->setSampleForward(m_task.sampleForward);
    m_canvas->setLogicalCanvasSize(400, 300);
    connect(m_canvas, &TurtleCanvasHost::fvtEvent, this, &TurtleTaskWidget::onCanvasEvent);

    m_controlRow = new QWidget(m_panel);
    auto *btnLayout = new QHBoxLayout(m_controlRow);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(8);

    m_runBtn = new QPushButton(QString::fromUtf8("Выполнить"), m_controlRow);
    m_runBtn->setCursor(Qt::PointingHandCursor);
    m_runBtn->setMinimumHeight(28);

    m_stepBtn = new QPushButton(QString::fromUtf8("Шаг"), m_controlRow);
    m_stepBtn->setCursor(Qt::PointingHandCursor);
    m_stepBtn->setMinimumHeight(28);

    m_resetBtn = new QPushButton(QString::fromUtf8("Сброс"), m_controlRow);
    m_resetBtn->setCursor(Qt::PointingHandCursor);
    m_resetBtn->setMinimumHeight(28);

    btnLayout->addWidget(m_runBtn, 1);
    btnLayout->addWidget(m_stepBtn, 1);
    btnLayout->addWidget(m_resetBtn, 1);

    m_catalogScroll = new QScrollArea(m_panel);
    m_catalogScroll->setWidgetResizable(true);
    m_catalogScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_catalogScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_catalogScroll->setFrameShape(QFrame::NoFrame);
    m_catalogScroll->setFocusPolicy(Qt::NoFocus);
    m_catalogHost = new QWidget(m_catalogScroll);
    m_catalogScroll->setWidget(m_catalogHost);
    buildCatalog();

    panelLayout->addWidget(m_instruction, 0);
    panelLayout->addWidget(m_canvas, 0);
    panelLayout->addWidget(m_controlRow, 0);
    panelLayout->addWidget(m_catalogScroll, 1);

    m_splitter->addWidget(m_editor);
    m_splitter->addWidget(m_panel);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);
    m_splitter->setSizes({ 720, 320 });

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(m_splitter);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_editor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_panel->installEventFilter(this);
    m_canvas->installEventFilter(this);

    connect(m_runBtn, &QPushButton::clicked, this, &TurtleTaskWidget::onRunClicked);
    connect(m_stepBtn, &QPushButton::clicked, this, &TurtleTaskWidget::onStepClicked);
    connect(m_resetBtn, &QPushButton::clicked, this, &TurtleTaskWidget::onResetClicked);

    applyPanelTheme();
    QTimer::singleShot(0, this, [this]() { updateCanvasHeight(); });
}

TurtleTaskWidget::~TurtleTaskWidget()
{
    killSession();
    cleanupTempDir();
}

void TurtleTaskWidget::buildCatalog()
{
    if (!m_catalogHost)
        return;

    if (QLayout *old = m_catalogHost->layout()) {
        QLayoutItem *item = nullptr;
        while ((item = old->takeAt(0))) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
        delete old;
    }
    m_taskButtons.clear();

    auto *layout = new QVBoxLayout(m_catalogHost);
    layout->setContentsMargins(0, 8, 0, 0);
    layout->setSpacing(10);

    for (const TurtleTaskChapter &chapter : m_catalog) {
        auto *title = new QLabel(chapter.title, m_catalogHost);
        title->setObjectName(QStringLiteral("taskChapterTitle"));
        title->setWordWrap(true);
        layout->addWidget(title);

        auto *row = new QWidget(m_catalogHost);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(6);
        for (int i = 0; i < chapter.tasks.size(); ++i) {
            auto *btn = new QPushButton(QString::number(i + 1), row);
            btn->setObjectName(QStringLiteral("taskNumBtn"));
            btn->setCursor(Qt::PointingHandCursor);
            btn->setCheckable(true);
            btn->setFixedSize(32, 28);
            btn->setProperty("taskId", chapter.tasks.at(i).id);
            connect(btn, &QPushButton::clicked, this, &TurtleTaskWidget::onCatalogTaskClicked);
            rowLayout->addWidget(btn);
            m_taskButtons.append(btn);
        }
        rowLayout->addStretch(1);
        layout->addWidget(row);
    }
    layout->addStretch(1);
    updateCatalogSelection();
}

void TurtleTaskWidget::updateCatalogSelection()
{
    for (QPushButton *btn : m_taskButtons) {
        if (!btn)
            continue;
        btn->setChecked(btn->property("taskId").toString() == m_task.id);
    }
}

void TurtleTaskWidget::applyTask(const TurtleTaskDefinition &task, bool restartSession)
{
    if (task.id == m_task.id)
        return;

    if (m_executing)
        sendToProcess(QStringLiteral("STOP"));

    m_task = task;
    if (m_editor) {
        m_editor->setPlainText(m_task.starterCode);
        m_editor->setModified(false);
    }
    if (m_canvas)
        m_canvas->setSampleForward(m_task.sampleForward);
    showStatus(QString(), QStringLiteral("info"));
    updateCatalogSelection();

    if (restartSession) {
        killSession();
        ensureSession();
    }
}

void TurtleTaskWidget::onCatalogTaskClicked()
{
    auto *btn = qobject_cast<QPushButton *>(sender());
    if (!btn)
        return;
    const QString id = btn->property("taskId").toString();
    for (const TurtleTaskChapter &chapter : m_catalog) {
        for (const TurtleTaskDefinition &task : chapter.tasks) {
            if (task.id == id) {
                applyTask(task, true);
                return;
            }
        }
    }
}

void TurtleTaskWidget::updateCanvasHeight()
{
    if (!m_canvas)
        return;
    const int w = m_canvas->width();
    if (w <= 0)
        return;
    const int h = m_canvas->heightForWidth(w);
    if (m_canvas->height() != h)
        m_canvas->setFixedHeight(h);
}

bool TurtleTaskWidget::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == m_panel || watched == m_canvas) && event && event->type() == QEvent::Resize)
        updateCanvasHeight();
    return QWidget::eventFilter(watched, event);
}

void TurtleTaskWidget::setTheme(const QString &theme)
{
    m_theme = theme;
    if (m_editor)
        m_editor->setTheme(theme);
    applyPanelTheme();
}

void TurtleTaskWidget::setPythonGetter(const std::function<QString()> &getter)
{
    m_pythonGetter = getter;
    QTimer::singleShot(0, this, [this]() { ensureSession(); });
}

bool TurtleTaskWidget::isRunning() const
{
    return m_executing;
}

void TurtleTaskWidget::applyPanelTheme()
{
    const bool dark = (m_theme == QLatin1String("dark"));
    const QString panelBg = dark ? QStringLiteral("#2b2b2b") : QStringLiteral("#ececec");
    QString boxBg = dark ? QStringLiteral("#5a4e2a") : QStringLiteral("#fff3c4");
    QString boxBorder = dark ? QStringLiteral("#8a7a3a") : QStringLiteral("#e6d48a");
    QString boxText = dark ? QStringLiteral("#f5f0d8") : QStringLiteral("#333333");
    if (m_statusKind == QLatin1String("ok")) {
        boxBg = dark ? QStringLiteral("#1f3d1f") : QStringLiteral("#e4f5e4");
        boxBorder = dark ? QStringLiteral("#3d7a3d") : QStringLiteral("#8bc48b");
        boxText = dark ? QStringLiteral("#b6f0b6") : QStringLiteral("#1b7a1b");
    } else if (m_statusKind == QLatin1String("error")) {
        boxBg = dark ? QStringLiteral("#3d1f1f") : QStringLiteral("#fde8e8");
        boxBorder = dark ? QStringLiteral("#8a3a3a") : QStringLiteral("#e0a0a0");
        boxText = dark ? QStringLiteral("#ffb4b4") : QStringLiteral("#b00020");
    } else if (m_statusKind == QLatin1String("info")) {
        boxBg = dark ? QStringLiteral("#2f2f2f") : QStringLiteral("#f3f3f3");
        boxBorder = dark ? QStringLiteral("#555555") : QStringLiteral("#d0d0d0");
        boxText = dark ? QStringLiteral("#d0d0d0") : QStringLiteral("#333333");
    }

    if (m_panel)
        m_panel->setStyleSheet(QStringLiteral("QWidget { background: %1; }").arg(panelBg));
    if (m_instruction) {
        m_instruction->setStyleSheet(QString(
            "QLabel {"
            " background: %1;"
            " color: %2;"
            " border: 1px solid %3;"
            " border-radius: 4px;"
            " padding: 10px;"
            " font-size: 12px;"
            "}"
        ).arg(boxBg, boxText, boxBorder));
    }
    const QString btnStyle = dark
        ? QStringLiteral(
            "QPushButton {"
            " background-color: #4a4a4a;"
            " color: #ffffff;"
            " border: 1px solid #666666;"
            " border-radius: 3px;"
            " padding: 5px 8px;"
            "}"
            "QPushButton:hover {"
            " background-color: #555555;"
            " border-color: #777777;"
            "}"
            "QPushButton:pressed { background-color: #3a3a3a; }"
            "QPushButton:disabled {"
            " background-color: #333333;"
            " color: #777777;"
            " border-color: #444444;"
            "}"
        )
        : QStringLiteral(
            "QPushButton {"
            " background-color: #f0f0f0;"
            " color: #000000;"
            " border: 1px solid #adadad;"
            " border-radius: 3px;"
            " padding: 5px 8px;"
            "}"
            "QPushButton:hover {"
            " background-color: #e5e5e5;"
            " border-color: #888888;"
            "}"
            "QPushButton:pressed { background-color: #d0d0d0; }"
            "QPushButton:disabled {"
            " background-color: #f5f5f5;"
            " color: #a0a0a0;"
            " border-color: #d0d0d0;"
            "}"
        );
    const QString iconColor = dark ? QStringLiteral("#e0e0e0") : QStringLiteral("#000000");
    const QSize iconSize(16, 16);
    if (m_runBtn) {
        m_runBtn->setIcon(taskPanelIcon(QStringLiteral("play.svg"), iconColor));
        m_runBtn->setIconSize(iconSize);
        m_runBtn->setStyleSheet(btnStyle);
    }
    if (m_stepBtn) {
        m_stepBtn->setIcon(taskPanelIcon(QStringLiteral("arrow-right.svg"), iconColor));
        m_stepBtn->setIconSize(iconSize);
        m_stepBtn->setStyleSheet(btnStyle);
    }
    if (m_resetBtn) {
        m_resetBtn->setIcon(taskPanelIcon(QStringLiteral("undo.svg"), iconColor));
        m_resetBtn->setIconSize(iconSize);
        m_resetBtn->setStyleSheet(btnStyle);
    }

    const QString chapterColor = dark ? QStringLiteral("#e8e8e8") : QStringLiteral("#222222");
    const QString numIdleBg = dark ? QStringLiteral("#3a3a3a") : QStringLiteral("#ffffff");
    const QString numIdleBorder = dark ? QStringLiteral("#666666") : QStringLiteral("#adadad");
    const QString numIdleText = dark ? QStringLiteral("#ffffff") : QStringLiteral("#000000");
    const QString numSelBg = dark ? QStringLiteral("#2a82da") : QStringLiteral("#2f6fed");
    if (m_catalogScroll) {
        m_catalogScroll->setStyleSheet(QStringLiteral(
            "QScrollArea { background: transparent; border: none; }"
            "QScrollArea > QWidget > QWidget { background: transparent; }"
        ));
    }
    if (m_catalogHost) {
        const QList<QLabel *> titles = m_catalogHost->findChildren<QLabel *>(QStringLiteral("taskChapterTitle"));
        for (QLabel *title : titles) {
            title->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; font-size: 13px; font-weight: 600; background: transparent; }"
            ).arg(chapterColor));
        }
    }
    const QString numStyle = QString(
        "QPushButton#taskNumBtn {"
        " background-color: %1;"
        " color: %2;"
        " border: 1px solid %3;"
        " border-radius: 3px;"
        " padding: 0px;"
        " font-weight: 600;"
        "}"
        "QPushButton#taskNumBtn:hover { border-color: #777777; }"
        "QPushButton#taskNumBtn:checked {"
        " background-color: %4;"
        " color: #ffffff;"
        " border-color: %4;"
        "}"
    ).arg(numIdleBg, numIdleText, numIdleBorder, numSelBg);
    for (QPushButton *btn : m_taskButtons) {
        if (btn)
            btn->setStyleSheet(numStyle);
    }
}

void TurtleTaskWidget::showStatus(const QString &text, const QString &kind)
{
    if (text.isEmpty())
        m_statusKind = QStringLiteral("hint");
    else
        m_statusKind = kind;
    if (m_instruction) {
        if (text.isEmpty())
            m_instruction->setText(m_task.instruction);
        else
            m_instruction->setText(text);
    }
    applyPanelTheme();
}

QString TurtleTaskWidget::pythonExecutable() const
{
    if (m_pythonGetter)
        return m_pythonGetter();
    return QString();
}

void TurtleTaskWidget::onResetClicked()
{
    m_pendingRun = false;
    showStatus(QString(), QStringLiteral("info"));
    ensureSession();
    if (!sessionAlive() || !m_sessionReady) {
        m_pendingReset = true;
        return;
    }
    if (m_executing) {
        m_pendingReset = true;
        sendToProcess(QStringLiteral("STOP"));
        return;
    }
    sendToProcess(QStringLiteral("RESET"));
}

void TurtleTaskWidget::onRunClicked()
{
    run(pythonExecutable(), false);
}

void TurtleTaskWidget::onStepClicked()
{
    if (isRunning() && m_stepMode) {
        requestStep();
        return;
    }
    if (isRunning() && !m_stepMode)
        return;
    run(pythonExecutable(), true);
}

void TurtleTaskWidget::requestStep()
{
    sendToProcess(QStringLiteral("STEP"));
    m_waitingStep = false;
}

void TurtleTaskWidget::sendToProcess(const QString &line)
{
    if (!m_process || m_process->state() == QProcess::NotRunning)
        return;
    m_process->write((line + QLatin1Char('\n')).toUtf8());
}

void TurtleTaskWidget::onCanvasEvent(const QJsonObject &event)
{
    const QByteArray json = QJsonDocument(event).toJson(QJsonDocument::Compact);
    sendToProcess(QStringLiteral("EVENT ") + QString::fromUtf8(json));
}

bool TurtleTaskWidget::sessionAlive() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

void TurtleTaskWidget::ensureSession()
{
    if (m_killing)
        return;
    if (sessionAlive())
        return;
    startSession();
}

bool TurtleTaskWidget::writeStudentScript()
{
    if (!m_editor)
        return false;
    if (m_tempDir.isEmpty())
        return false;
    return !writeTempFile(QStringLiteral("student.py"), m_editor->toPlainText().toUtf8()).isEmpty();
}

void TurtleTaskWidget::sendRunCommand()
{
    if (!writeStudentScript()) {
        QMessageBox::critical(this, tr("Ошибка"),
                              tr("Не удалось подготовить файлы задачи."));
        return;
    }
    QString cmd = m_stepMode ? QStringLiteral("RUN 1") : QStringLiteral("RUN 0");
    if (!m_task.checkId.isEmpty())
        cmd += QLatin1Char(' ') + m_task.checkId;
    sendToProcess(cmd);
    m_executing = true;
    m_waitingStep = false;
}

void TurtleTaskWidget::flushPending()
{
    if (m_pendingReset) {
        m_pendingReset = false;
        sendToProcess(QStringLiteral("RESET"));
        return;
    }
    if (m_pendingRun) {
        m_pendingRun = false;
        sendRunCommand();
    }
}

QString TurtleTaskWidget::writeTempFile(const QString &name, const QByteArray &data) const
{
    if (m_tempDir.isEmpty())
        return QString();
    const QString path = QDir(m_tempDir).filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QString();
    f.write(data);
    f.close();
    return path;
}

QString TurtleTaskWidget::loadRunnerScript() const
{
    QFile res(QStringLiteral(":/tasks/turtle_task_runner.py"));
    if (res.open(QIODevice::ReadOnly))
        return QString::fromUtf8(res.readAll());

    const QStringList candidates = {
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("tasks/turtle_task_runner.py")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../tasks/turtle_task_runner.py")),
    };
    for (const QString &path : candidates) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly))
            return QString::fromUtf8(f.readAll());
    }
    return QString();
}

void TurtleTaskWidget::cleanupTempDir()
{
    if (m_tempDir.isEmpty())
        return;
    QDir dir(m_tempDir);
    if (dir.exists())
        dir.removeRecursively();
    m_tempDir.clear();
}

void TurtleTaskWidget::run(const QString &pythonExe, bool stepMode)
{
    const QString py = pythonExe.isEmpty() ? pythonExecutable() : pythonExe;
    if (py.isEmpty()) {
        QMessageBox::critical(this, tr("Python не найден"),
                              tr("Не удалось найти интерпретатор Python."));
        return;
    }
    if (!m_editor)
        return;

    m_stepMode = stepMode;
    m_pendingReset = false;
    showStatus(QString(), QStringLiteral("info"));

    if (!sessionAlive())
        startSession(py);
    if (!sessionAlive())
        return;

    if (!writeStudentScript()) {
        QMessageBox::critical(this, tr("Ошибка"),
                              tr("Не удалось подготовить файлы задачи."));
        return;
    }

    if (!m_sessionReady || m_executing) {
        m_pendingRun = true;
        if (m_executing)
            sendToProcess(QStringLiteral("STOP"));
        return;
    }
    sendRunCommand();
}

void TurtleTaskWidget::stop()
{
    m_waitingStep = false;
    if (m_executing)
        sendToProcess(QStringLiteral("STOP"));
}

void TurtleTaskWidget::killSession()
{
    m_killing = true;
    m_sessionReady = false;
    m_executing = false;
    m_pendingRun = false;
    m_pendingReset = false;
    m_waitingStep = false;
    if (!m_process) {
        m_killing = false;
        return;
    }

    QProcess *proc = m_process;
    m_process = nullptr;
    proc->disconnect(this);

    if (proc->state() != QProcess::NotRunning) {
        proc->write("QUIT\n");
        proc->terminate();
        if (!proc->waitForFinished(600))
            proc->kill();
        proc->waitForFinished(400);
    }
    proc->deleteLater();
    m_killing = false;
}

void TurtleTaskWidget::startSession(const QString &pythonExeArg)
{
    const QString pythonExe = pythonExeArg.isEmpty() ? pythonExecutable() : pythonExeArg;
    if (pythonExe.isEmpty())
        return;
    if (m_killing)
        return;

    m_sessionReady = false;
    m_executing = false;
    m_stdoutBuf.clear();

    if (m_tempDir.isEmpty()) {
        m_tempDir = QDir::temp().filePath(
            QStringLiteral("vuzhyk_task_%1").arg(QString::number(quintptr(this), 16)));
        QDir().mkpath(m_tempDir);
    }

    const QString runnerSource = loadRunnerScript();
    if (runnerSource.isEmpty()) {
        QMessageBox::critical(this, tr("Ошибка"),
                              tr("Не удалось подготовить файлы задачи."));
        return;
    }
    const QString runnerPath = writeTempFile(
        QStringLiteral("turtle_task_runner.py"), runnerSource.toUtf8());
    writeTempFile(QStringLiteral("student.py"),
                  m_editor ? m_editor->toPlainText().toUtf8() : QByteArray());
    if (runnerPath.isEmpty()) {
        QMessageBox::critical(this, tr("Ошибка"),
                              tr("Не удалось записать запускающий скрипт."));
        return;
    }

    if (m_canvas)
        m_canvas->setLogicalCanvasSize(400, 300);

    m_process = new QProcess(this);
    m_process->setProgram(pythonExe);
    QStringList args;
    args << QStringLiteral("-u")
         << runnerPath
         << QStringLiteral("--script") << QDir(m_tempDir).filePath(QStringLiteral("student.py"))
         << QStringLiteral("--width") << QStringLiteral("400")
         << QStringLiteral("--height") << QStringLiteral("300")
         << QStringLiteral("--sample-forward") << QString::number(m_task.sampleForward)
         << QStringLiteral("--sample-path") << taskSamplePath(m_task)
         << QStringLiteral("--check") << m_task.checkId;
    m_process->setArguments(args);
    m_process->setWorkingDirectory(m_tempDir);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
    env.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
    env.insert(QStringLiteral("VUZHYK_FVT"), QStringLiteral("1"));
    env.insert(QStringLiteral("VUZHYK_FVT_WIDTH"), QStringLiteral("400"));
    env.insert(QStringLiteral("VUZHYK_FVT_HEIGHT"), QStringLiteral("300"));
    m_process->setProcessEnvironment(env);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &TurtleTaskWidget::onStdout);
    connect(m_process, &QProcess::readyReadStandardError, this, &TurtleTaskWidget::onStderr);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TurtleTaskWidget::onFinished);

    m_process->start();
    if (!m_process->waitForStarted(4000)) {
        QMessageBox::critical(this, tr("Ошибка"), tr("Не удалось запустить Python."));
        m_process->deleteLater();
        m_process = nullptr;
    }
}

void TurtleTaskWidget::handleFvtPayload(const QJsonObject &obj)
{
    const QString op = obj.value(QStringLiteral("op")).toString();
    if (op == QLatin1String("scene")) {
        if (m_canvas)
            m_canvas->applyScene(obj);
        return;
    }
    if (op == QLatin1String("resize")) {
        if (m_canvas) {
            m_canvas->setLogicalCanvasSize(
                obj.value(QStringLiteral("w")).toInt(400),
                obj.value(QStringLiteral("h")).toInt(400));
        }
        return;
    }
    if (op == QLatin1String("check")) {
        const bool ok = obj.value(QStringLiteral("ok")).toBool();
        const QString message = obj.value(QStringLiteral("message")).toString();
        if (!message.isEmpty())
            showStatus(message, ok ? QStringLiteral("ok") : QStringLiteral("error"));
        return;
    }
    if (op == QLatin1String("step_wait")) {
        m_waitingStep = true;
        showStatus(tr("Нажмите «Шаг», чтобы выполнить следующую команду."), QStringLiteral("info"));
        return;
    }
    if (op == QLatin1String("input")) {
        const QString kind = obj.value(QStringLiteral("kind")).toString();
        const QString title = obj.value(QStringLiteral("title")).toString();
        const QString prompt = obj.value(QStringLiteral("prompt")).toString();
        const QString id = obj.value(QStringLiteral("id")).toString();
        QJsonObject reply;
        reply.insert(QStringLiteral("id"), id);
        bool ok = false;
        if (kind == QLatin1String("float") || kind == QLatin1String("int")) {
            const double def = obj.value(QStringLiteral("default")).toDouble();
            const double val = QInputDialog::getDouble(
                this, title, prompt, def,
                obj.contains(QStringLiteral("min")) ? obj.value(QStringLiteral("min")).toDouble() : -1e9,
                obj.contains(QStringLiteral("max")) ? obj.value(QStringLiteral("max")).toDouble() : 1e9,
                kind == QLatin1String("int") ? 0 : 4, &ok);
            if (ok)
                reply.insert(QStringLiteral("value"), val);
            else
                reply.insert(QStringLiteral("value"), QJsonValue());
        } else {
            const QString val = QInputDialog::getText(this, title, prompt, QLineEdit::Normal, QString(), &ok);
            if (ok)
                reply.insert(QStringLiteral("value"), val);
            else
                reply.insert(QStringLiteral("value"), QJsonValue());
        }
        sendToProcess(QStringLiteral("REPLY ") + QString::fromUtf8(QJsonDocument(reply).toJson(QJsonDocument::Compact)));
        return;
    }
    if (op == QLatin1String("done") || op == QLatin1String("title")) {
        return;
    }
}

void TurtleTaskWidget::handleRunnerLine(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (trimmed.startsWith(QLatin1String("FVT "))) {
        const QJsonDocument doc = QJsonDocument::fromJson(trimmed.mid(4).toUtf8());
        if (doc.isObject())
            handleFvtPayload(doc.object());
        return;
    }
    if (trimmed == QLatin1String("VUZHYK_TURTLE_STEP_WAIT")) {
        m_waitingStep = true;
        showStatus(tr("Нажмите «Шаг», чтобы выполнить следующую команду."), QStringLiteral("info"));
        return;
    }
    if (trimmed == QLatin1String("VUZHYK_TURTLE_READY")) {
        m_sessionReady = true;
        flushPending();
        return;
    }
    if (trimmed == QLatin1String("VUZHYK_TURTLE_IDLE")) {
        m_executing = false;
        m_waitingStep = false;
        flushPending();
        return;
    }
    if (trimmed.startsWith(QLatin1String("VUZHYK_TURTLE_CHECK "))) {
        const QString rest = trimmed.mid(20);
        const bool ok = rest.startsWith(QLatin1String("OK "));
        const QString message = rest.mid(ok ? 3 : 5);
        if (!message.isEmpty())
            showStatus(message, ok ? QStringLiteral("ok") : QStringLiteral("error"));
        return;
    }
    if (trimmed == QLatin1String("VUZHYK_TURTLE_DONE")) {
        return;
    }
    if (trimmed == QLatin1String("VUZHYK_TURTLE_ERROR")) {
        showStatus(tr("В программе есть ошибка — смотрите панель вывода."), QStringLiteral("error"));
        return;
    }
    if (!trimmed.isEmpty())
        emit processOutput(line + QLatin1Char('\n'), false);
}

void TurtleTaskWidget::onStdout()
{
    if (!m_process)
        return;
    m_stdoutBuf += QString::fromUtf8(m_process->readAllStandardOutput());
    int nl;
    while ((nl = m_stdoutBuf.indexOf(QLatin1Char('\n'))) >= 0) {
        const QString line = m_stdoutBuf.left(nl);
        m_stdoutBuf.remove(0, nl + 1);
        handleRunnerLine(line);
    }
}

void TurtleTaskWidget::onStderr()
{
    if (!m_process)
        return;
    const QString text = QString::fromUtf8(m_process->readAllStandardError());
    if (!text.isEmpty())
        emit processOutput(text, true);
}

void TurtleTaskWidget::onFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(status);
    if (!m_stdoutBuf.trimmed().isEmpty()) {
        handleRunnerLine(m_stdoutBuf);
        m_stdoutBuf.clear();
    }
    m_sessionReady = false;
    m_executing = false;
    m_waitingStep = false;
    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
}
