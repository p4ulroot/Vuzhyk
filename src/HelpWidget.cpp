#include "HelpWidget.h"
#include <QHeaderView>
#include <QTextDocument>
#include <QRegularExpression>
#include <QStringList>
#include <QList>
#include <algorithm>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QDir>
#include <QPalette>
#include <QApplication>
#include <QStyle>
#include <QAbstractItemView>

HelpWidget::HelpWidget(QWidget *parent)
    : QWidget(parent) {
    setupUi();
    // Устанавливаем тему по умолчанию (светлая)
    setTheme("light");
}

void HelpWidget::setTheme(const QString &theme) {
    m_theme = theme;
    
    // Применяем стили в зависимости от темы
    if (m_treeWidget) {
        if (theme == "dark") {
            // Темная тема
            QPalette palette = m_treeWidget->palette();
            palette.setColor(QPalette::Text, Qt::white);
            palette.setColor(QPalette::WindowText, Qt::white);
            palette.setColor(QPalette::Base, QColor(35, 35, 35));
            palette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
            m_treeWidget->setPalette(palette);
            
            m_treeWidget->setStyleSheet(
                "QTreeWidget {"
                    "color: #ffffff;"
                    "background-color: #232323;"
                "}"
                "QTreeWidget::item {"
                    "color: #ffffff;"
                "}"
                "QTreeWidget::branch {"
                    "border: none;"
                    "background: none;"
                    "image: none;"
                    "width: 0px;"
                    "height: 0px;"
                "}"
                "QTreeWidget::branch:has-children {"
                    "border: none;"
                    "background: none;"
                    "image: none;"
                    "width: 0px;"
                    "height: 0px;"
                "}"
                "QTreeWidget::branch:closed:has-children {"
                    "image: none;"
                "}"
                "QTreeWidget::branch:open:has-children {"
                    "image: none;"
                "}"
            );
        } else {
            // Светлая тема
            m_treeWidget->setPalette(qApp->style()->standardPalette());
            m_treeWidget->setStyleSheet(
                "QTreeWidget::branch {"
                    "border: none;"
                    "background: none;"
                    "image: none;"
                    "width: 0px;"
                    "height: 0px;"
                "}"
                "QTreeWidget::branch:has-children {"
                    "border: none;"
                    "background: none;"
                    "image: none;"
                    "width: 0px;"
                    "height: 0px;"
                "}"
                "QTreeWidget::branch:closed:has-children {"
                    "image: none;"
                "}"
                "QTreeWidget::branch:open:has-children {"
                    "image: none;"
                "}"
            );
        }
    }
    
    // Обновляем отображаемый контент, если он уже был загружен
    if (m_treeWidget && m_treeWidget->topLevelItemCount() > 0) {
        QTreeWidgetItem *currentItem = m_treeWidget->currentItem();
        if (!currentItem) {
            currentItem = m_treeWidget->topLevelItem(0);
        }
        if (currentItem) {
            onItemClicked(currentItem, 0);
        }
    }
}

void HelpWidget::setupUi() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    
    // Создаем дерево разделов
    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderHidden(true);
    m_treeWidget->setMaximumWidth(300);
    m_treeWidget->setMinimumWidth(200);
    m_treeWidget->setRootIsDecorated(false); // Отключаем стрелки разворота
    
    // Полностью убираем стандартные стрелки через стили
    m_treeWidget->setStyleSheet(
        "QTreeWidget::branch {"
            "border: none;"
            "background: none;"
            "image: none;"
            "width: 0px;"
            "height: 0px;"
        "}"
        "QTreeWidget::branch:has-children {"
            "border: none;"
            "background: none;"
            "image: none;"
            "width: 0px;"
            "height: 0px;"
        "}"
        "QTreeWidget::branch:closed:has-children {"
            "image: none;"
        "}"
        "QTreeWidget::branch:open:has-children {"
            "image: none;"
        "}"
    );
    
    // Создаем браузер содержимого
    m_contentBrowser = new QTextBrowser(this);
    m_contentBrowser->setOpenLinks(false);
    setupHelpBrowserPaths();
    
    m_splitter->addWidget(m_treeWidget);
    m_splitter->addWidget(m_contentBrowser);
    m_splitter->setSizes({200, 600});
    
    layout->addWidget(m_splitter);
    
    // Создаем дерево после того, как все виджеты готовы
    createTreeWidget();
    
    // Подключаем сигнал клика на элемент дерева
    connect(m_treeWidget, &QTreeWidget::itemClicked, this, &HelpWidget::onItemClicked);
}

static QTreeWidgetItem *helpFindItem(QTreeWidgetItem *item, const QString &key)
{
    if (!item)
        return nullptr;
    if (item->data(0, Qt::UserRole).toString() == key)
        return item;
    for (int i = 0; i < item->childCount(); ++i) {
        if (QTreeWidgetItem *found = helpFindItem(item->child(i), key))
            return found;
    }
    return nullptr;
}

QTreeWidgetItem *HelpWidget::findItemByKey(const QString &key) const
{
    if (!m_treeWidget || key.isEmpty())
        return nullptr;
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        if (QTreeWidgetItem *found = helpFindItem(m_treeWidget->topLevelItem(i), key))
            return found;
    }
    return nullptr;
}

void HelpWidget::showItem(QTreeWidgetItem *item)
{
    if (!item || !m_treeWidget)
        return;
    m_treeWidget->setCurrentItem(item);
    m_treeWidget->scrollToItem(item, QAbstractItemView::PositionAtCenter);
    onItemClicked(item, 0);
}

void HelpWidget::createTreeWidget() {
    addTreeItems();
    // Разворачиваем все элементы дерева
    m_treeWidget->expandAll();
    // Выбираем первый элемент и показываем его содержимое
    if (m_treeWidget->topLevelItemCount() > 0) {
        m_treeWidget->setCurrentItem(m_treeWidget->topLevelItem(0));
        onItemClicked(m_treeWidget->topLevelItem(0), 0);
    }
}

void HelpWidget::addTreeItems() {
    // Типы данных
    auto *types = new QTreeWidgetItem(m_treeWidget);
    types->setText(0, "1. Типы данных");
    types->setData(0, Qt::UserRole, "types_main");
    
    // Добавляем подразделы типов данных
    auto *types_1 = new QTreeWidgetItem(types);
    types_1->setText(0, "1.1. Переменные в Python");
    types_1->setData(0, Qt::UserRole, "types_variables");
    
    auto *types_2 = new QTreeWidgetItem(types);
    types_2->setText(0, "1.2. Числа");
    types_2->setData(0, Qt::UserRole, "types_numbers");
    
    auto *types_3 = new QTreeWidgetItem(types);
    types_3->setText(0, "1.3. Строки");
    types_3->setData(0, Qt::UserRole, "types_strings");
    
    auto *types_3_1 = new QTreeWidgetItem(types_3);
    types_3_1->setText(0, "1.3.1. Логические и физические строки");
    types_3_1->setData(0, Qt::UserRole, "types_strings_logical");
    
    auto *types_3_2 = new QTreeWidgetItem(types_3);
    types_3_2->setText(0, "1.3.2. Операции над последовательностями");
    types_3_2->setData(0, Qt::UserRole, "types_strings_operations");
    
    auto *types_3_3 = new QTreeWidgetItem(types_3);
    types_3_3->setText(0, "1.3.3. Методы для работы со строками");
    types_3_3->setData(0, Qt::UserRole, "types_strings_methods");
    
    auto *types_3_4 = new QTreeWidgetItem(types_3);
    types_3_4->setText(0, "1.3.4. Пример программы");
    types_3_4->setData(0, Qt::UserRole, "types_strings_example");
    
    auto *types_4 = new QTreeWidgetItem(types);
    types_4->setText(0, "1.4. Списки");
    types_4->setData(0, Qt::UserRole, "types_lists");
    
    auto *types_4_1 = new QTreeWidgetItem(types_4);
    types_4_1->setText(0, "1.4.1. Методы для работы со списками");
    types_4_1->setData(0, Qt::UserRole, "types_lists_methods");
    
    auto *types_5 = new QTreeWidgetItem(types);
    types_5->setText(0, "1.5. Словари");
    types_5->setData(0, Qt::UserRole, "types_dicts");
    
    auto *types_6 = new QTreeWidgetItem(types);
    types_6->setText(0, "1.6. Методы для работы со словарями");
    types_6->setData(0, Qt::UserRole, "types_dict_methods");
    
    auto *types_7 = new QTreeWidgetItem(types);
    types_7->setText(0, "1.7. Кортежи");
    types_7->setData(0, Qt::UserRole, "types_tuples");
    
    auto *types_8 = new QTreeWidgetItem(types);
    types_8->setText(0, "1.8. Множество");
    types_8->setData(0, Qt::UserRole, "types_sets");
    
    auto *types_9 = new QTreeWidgetItem(types);
    types_9->setText(0, "1.9. Методы для работы с множествами");
    types_9->setData(0, Qt::UserRole, "types_sets_methods");
    
    auto *types_10 = new QTreeWidgetItem(types);
    types_10->setText(0, "1.10. Булевы значения");
    types_10->setData(0, Qt::UserRole, "types_bool");
    
    auto *types_11 = new QTreeWidgetItem(types);
    types_11->setText(0, "1.11. Преобразование типов");
    types_11->setData(0, Qt::UserRole, "types_conversion");
    
    auto *types_12 = new QTreeWidgetItem(types);
    types_12->setText(0, "1.12. Проверка типов");
    types_12->setData(0, Qt::UserRole, "types_checking");
    
    auto *types_13 = new QTreeWidgetItem(types);
    types_13->setText(0, "1.13. Файлы");
    types_13->setData(0, Qt::UserRole, "types_files");
    
    // Операторы
    auto *operators = new QTreeWidgetItem(m_treeWidget);
    operators->setText(0, "2. Операторы языка Python");
    operators->setData(0, Qt::UserRole, "operators_main");
    
    auto *operators_1 = new QTreeWidgetItem(operators);
    operators_1->setText(0, "2.1. Базовые операторы");
    operators_1->setData(0, Qt::UserRole, "operators_basic");
    
    auto *operators_2 = new QTreeWidgetItem(operators);
    operators_2->setText(0, "2.2. Управляющие операторы");
    operators_2->setData(0, Qt::UserRole, "operators_control");
    
    auto *operators_2_1 = new QTreeWidgetItem(operators_2);
    operators_2_1->setText(0, "2.2.1. Условные операторы (if/else)");
    operators_2_1->setData(0, Qt::UserRole, "operators_if_else");
    
    auto *operators_2_2 = new QTreeWidgetItem(operators_2);
    operators_2_2->setText(0, "2.2.2. Оператор while");
    operators_2_2->setData(0, Qt::UserRole, "operators_while");
    
    auto *operators_2_3 = new QTreeWidgetItem(operators_2);
    operators_2_3->setText(0, "2.2.3. Цикл for");
    operators_2_3->setData(0, Qt::UserRole, "operators_for");
    
    auto *operators_2_3_1 = new QTreeWidgetItem(operators_2_3);
    operators_2_3_1->setText(0, "2.2.3.1. Вложенные циклы for");
    operators_2_3_1->setData(0, Qt::UserRole, "operators_for_nested");
    
    auto *operators_2_4 = new QTreeWidgetItem(operators_2);
    operators_2_4->setText(0, "2.2.4. Оператор break");
    operators_2_4->setData(0, Qt::UserRole, "operators_break");
    
    auto *operators_2_5 = new QTreeWidgetItem(operators_2);
    operators_2_5->setText(0, "2.2.5. Оператор continue");
    operators_2_5->setData(0, Qt::UserRole, "operators_continue");
    
    // Функции
    auto *functions = new QTreeWidgetItem(m_treeWidget);
    functions->setText(0, "3. Функции языка Python");
    functions->setData(0, Qt::UserRole, "functions_main");
    
    auto *functions_1 = new QTreeWidgetItem(functions);
    functions_1->setText(0, "3.1. Параметры функций");
    functions_1->setData(0, Qt::UserRole, "functions_params");
    
    auto *functions_2 = new QTreeWidgetItem(functions);
    functions_2->setText(0, "3.2. Локальные переменные");
    functions_2->setData(0, Qt::UserRole, "functions_local");
    
    auto *functions_3 = new QTreeWidgetItem(functions);
    functions_3->setText(0, "3.3. Зарезервированное слово \"global\"");
    functions_3->setData(0, Qt::UserRole, "functions_global");
    
    auto *functions_4 = new QTreeWidgetItem(functions);
    functions_4->setText(0, "3.4. Зарезервированное слово \"nonlocal\"");
    functions_4->setData(0, Qt::UserRole, "functions_nonlocal");
    
    auto *functions_5 = new QTreeWidgetItem(functions);
    functions_5->setText(0, "3.5. Значения аргументов по умолчанию");
    functions_5->setData(0, Qt::UserRole, "functions_default");
    
    auto *functions_6 = new QTreeWidgetItem(functions);
    functions_6->setText(0, "3.6. Ключевые аргументы");
    functions_6->setData(0, Qt::UserRole, "functions_keyword");
    
    auto *functions_7 = new QTreeWidgetItem(functions);
    functions_7->setText(0, "3.7. Переменное число параметров");
    functions_7->setData(0, Qt::UserRole, "functions_varargs");
    
    auto *functions_8 = new QTreeWidgetItem(functions);
    functions_8->setText(0, "3.8. Только ключевые параметры");
    functions_8->setData(0, Qt::UserRole, "functions_keyword_only");
    
    auto *functions_9 = new QTreeWidgetItem(functions);
    functions_9->setText(0, "3.9. Оператор \"return\"");
    functions_9->setData(0, Qt::UserRole, "functions_return");
    
    auto *functions_10 = new QTreeWidgetItem(functions);
    functions_10->setText(0, "3.10. Строки документации");
    functions_10->setData(0, Qt::UserRole, "functions_docstring");

    auto *grade6 = new QTreeWidgetItem(m_treeWidget);
    grade6->setText(0, "4. 6 класс");
    grade6->setData(0, Qt::UserRole, "grade6_main");

    auto *grade6_1 = new QTreeWidgetItem(grade6);
    grade6_1->setText(0, "4.1. Подключение модуля");
    grade6_1->setData(0, Qt::UserRole, "grade6_import");

    auto *grade6_2 = new QTreeWidgetItem(grade6);
    grade6_2->setText(0, "4.2. Внешний вид Черепахи");
    grade6_2->setData(0, Qt::UserRole, "grade6_shape");

    auto *grade6_3 = new QTreeWidgetItem(grade6);
    grade6_3->setText(0, "4.3. Управление окном");
    grade6_3->setData(0, Qt::UserRole, "grade6_window");

    auto *grade6_4 = new QTreeWidgetItem(grade6);
    grade6_4->setText(0, "4.4. Движение Черепахи");
    grade6_4->setData(0, Qt::UserRole, "grade6_move");

    auto *grade6_5 = new QTreeWidgetItem(grade6);
    grade6_5->setText(0, "4.5. Управление пером");
    grade6_5->setData(0, Qt::UserRole, "grade6_pen");

    auto *grade6_6 = new QTreeWidgetItem(grade6);
    grade6_6->setText(0, "4.6. Цвет пера");
    grade6_6->setData(0, Qt::UserRole, "grade6_color");

    auto *grade6_7 = new QTreeWidgetItem(grade6);
    grade6_7->setText(0, "4.7. Состояние Черепахи");
    grade6_7->setData(0, Qt::UserRole, "grade6_state");

    auto *grade6_8 = new QTreeWidgetItem(grade6);
    grade6_8->setText(0, "4.8. Вспомогательный алгоритм");
    grade6_8->setData(0, Qt::UserRole, "grade6_functions");

    auto *grade7 = new QTreeWidgetItem(m_treeWidget);
    grade7->setText(0, "5. 7 класс");
    grade7->setData(0, Qt::UserRole, "grade7_main");

    auto *grade7_1 = new QTreeWidgetItem(grade7);
    grade7_1->setText(0, "5.1. Исполнитель Робот");
    grade7_1->setData(0, Qt::UserRole, "grade7_robot");

    auto *grade7_2 = new QTreeWidgetItem(grade7);
    grade7_2->setText(0, "5.2. Циклы");
    grade7_2->setData(0, Qt::UserRole, "grade7_loops");

    auto *grade7_3 = new QTreeWidgetItem(grade7);
    grade7_3->setText(0, "5.3. Ветвление и условия");
    grade7_3->setData(0, Qt::UserRole, "grade7_branch");

    auto *grade7_4 = new QTreeWidgetItem(grade7);
    grade7_4->setText(0, "5.4. Вывод и типы данных");
    grade7_4->setData(0, Qt::UserRole, "grade7_python");

    auto *grade7_5 = new QTreeWidgetItem(grade7);
    grade7_5->setText(0, "5.5. Ввод данных и вычисления");
    grade7_5->setData(0, Qt::UserRole, "grade7_calc");

    auto *grade7_6 = new QTreeWidgetItem(grade7);
    grade7_6->setText(0, "5.6. Целые числа: // и %");
    grade7_6->setData(0, Qt::UserRole, "grade7_int");
}

void HelpWidget::setupHelpBrowserPaths() {
    if (!m_contentBrowser) {
        return;
    }
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList roots;
    roots << appDir
          << QDir(appDir).filePath(QStringLiteral(".."))
          << QDir(appDir).filePath(QStringLiteral("../.."));

    QStringList paths;
    for (const QString &root : roots) {
        const QDir dir(root);
        paths << dir.filePath(QStringLiteral("tools/pdf/6kl_ris"));
        paths << dir.filePath(QStringLiteral("help"));
    }
    m_contentBrowser->setSearchPaths(paths);
}

QString HelpWidget::loadExternalHelp(const QString &key) const {
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList roots;
    roots << appDir
          << QDir(appDir).filePath(QStringLiteral(".."))
          << QDir(appDir).filePath(QStringLiteral("../.."));

    for (const QString &root : roots) {
        const QString path = QDir(root).filePath(QStringLiteral("help/") + key + QStringLiteral(".html"));
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString::fromUtf8(f.readAll());
        }
    }
    return QString();
}

void HelpWidget::onItemClicked(QTreeWidgetItem *item, int column) {
    Q_UNUSED(column);
    
    QString key = item->data(0, Qt::UserRole).toString();
    if (key.isEmpty()) {
        return;
    }
    
    QString content = getHelpContent(key);
    QString styledContent = wrapWithStyles(content);
    m_contentBrowser->setHtml(styledContent);
}

// Вспомогательная функция для проверки, находится ли позиция внутри HTML тега
static bool isInsideHtmlTag(const QString &text, int pos) {
    if (pos < 0 || pos >= text.length()) return false;
    
    // Ищем все открывающие <span до позиции pos
    int searchPos = 0;
    while (true) {
        int lastOpenSpan = text.indexOf("<span", searchPos);
        if (lastOpenSpan == -1 || lastOpenSpan >= pos) break;
        
        // Находим закрывающую скобку открывающего тега >
        int openTagEnd = text.indexOf(">", lastOpenSpan);
        if (openTagEnd == -1 || openTagEnd >= pos) {
            searchPos = lastOpenSpan + 1;
            continue;
        }
        
        // Проверяем, есть ли закрывающий </span> между открывающим тегом и нашей позицией
        int closeSpan = text.indexOf("</span>", openTagEnd + 1);
        
        // Если закрывающего тега нет или он после нашей позиции, значит мы внутри span
        if (closeSpan == -1 || closeSpan > pos) {
            // Убеждаемся, что pos не внутри самого тега <span...>
            if (pos > openTagEnd) {
                return true;
            }
        }
        
        // Если закрывающий тег до нашей позиции, ищем следующий открывающий
        if (closeSpan != -1 && closeSpan < pos) {
            searchPos = closeSpan + 7; // 7 = длина "</span>"
        } else {
            searchPos = lastOpenSpan + 1;
        }
    }
    
    return false;
}

QString HelpWidget::highlightPythonCode(const QString &code) {
    QString highlighted = code;
    bool isDark = (m_theme == "dark");
    
    // Цвета для подсветки синтаксиса
    QString stringColor = isDark ? "#CE9178" : "#C55227";      // Строки
    QString commentColor = isDark ? "#6A9955" : "#2D5016";     // Комментарии
    QString numberColor = isDark ? "#569CD6" : "#1A7A3E";      // Числа
    QString keywordColor = isDark ? "#569CD6" : "#0066CC";      // Ключевые слова
    QString builtinColor = isDark ? "#DCDCAA" : "#7B3F98";     // Встроенные функции
    
    // Подсветка строк (в кавычках) - обрабатываем первыми
    // Обрабатываем и тройные кавычки, и обычные строки одинаково
    int pos = 0;
    while (pos < highlighted.length()) {
        // Пропускаем содержимое внутри span тегов
        if (isInsideHtmlTag(highlighted, pos)) {
            int spanStart = highlighted.lastIndexOf("<span", pos);
            if (spanStart != -1) {
                int openTagEnd = highlighted.indexOf(">", spanStart);
                if (openTagEnd != -1) {
                    int spanEnd = highlighted.indexOf("</span>", openTagEnd + 1);
                    if (spanEnd != -1) {
                        pos = spanEnd + 7;
                        continue;
                    }
                }
            }
            pos++;
            continue;
        }
        
        // Обрабатываем строки (одинарные, двойные и тройные кавычки) - одинаково
        if (isInsideHtmlTag(highlighted, pos)) {
            // Находим конец текущего span тега
            int spanStart = highlighted.lastIndexOf("<span", pos);
            if (spanStart != -1) {
                int openTagEnd = highlighted.indexOf(">", spanStart);
                if (openTagEnd != -1) {
                    int spanEnd = highlighted.indexOf("</span>", openTagEnd + 1);
                    if (spanEnd != -1) {
                        pos = spanEnd + 7; // Пропускаем весь span тег
                        continue;
                    }
                }
            }
            pos++;
            continue;
        }
        
        // Ищем начало строки (одинарная, двойная или тройная кавычка)
        int quoteStart = -1;
        QString quoteType; // Для тройных кавычек это будет """ или ''', для обычных - " или '
        bool isTripleQuote = false;
        
        for (int i = pos; i < highlighted.length(); i++) {
            // Пропускаем содержимое внутри span тегов
            if (isInsideHtmlTag(highlighted, i)) {
                // Находим конец текущего span тега и переходим за него
                int spanStart = highlighted.lastIndexOf("<span", i);
                if (spanStart != -1) {
                    int openTagEnd = highlighted.indexOf(">", spanStart);
                    if (openTagEnd != -1) {
                        int spanEnd = highlighted.indexOf("</span>", openTagEnd + 1);
                        if (spanEnd != -1) {
                            i = spanEnd + 7; // Переходим за закрывающий тег (длина "</span>" = 7)
                            continue;
                        }
                    }
                }
            }
            
            // Сначала проверяем тройные кавычки
            if (i < highlighted.length() - 2) {
                QString tripleQuote = highlighted.mid(i, 3);
                if ((tripleQuote == "\"\"\"" || tripleQuote == "'''") && 
                    (i == 0 || highlighted[i-1] != '\\')) {
                    quoteStart = i;
                    quoteType = tripleQuote;
                    isTripleQuote = true;
                    break;
                }
            }
            
            // Затем проверяем обычные кавычки
            if ((highlighted[i] == '"' || highlighted[i] == '\'') && 
                (i == 0 || highlighted[i-1] != '\\')) {
                // Проверяем, что это не начало тройных кавычек
                if (i >= highlighted.length() - 2 || 
                    (highlighted.mid(i, 3) != "\"\"\"" && highlighted.mid(i, 3) != "'''")) {
                    quoteStart = i;
                    quoteType = QString(highlighted[i]);
                    isTripleQuote = false;
                    break;
                }
            }
        }
        if (quoteStart == -1) break;
        
        // Ищем конец строки
        int quoteEnd = -1;
        bool escaped = false;
        
        if (isTripleQuote) {
            // Для тройных кавычек ищем закрывающие тройные кавычки
            for (int i = quoteStart + 3; i < highlighted.length() - 2; i++) {
                // Пропускаем содержимое внутри span тегов
                if (isInsideHtmlTag(highlighted, i)) {
                    int spanStart = highlighted.lastIndexOf("<span", i);
                    if (spanStart != -1) {
                        int openTagEnd = highlighted.indexOf(">", spanStart);
                        if (openTagEnd != -1) {
                            int spanEnd = highlighted.indexOf("</span>", openTagEnd + 1);
                            if (spanEnd != -1) {
                                i = spanEnd + 7;
                                continue;
                            }
                        }
                    }
                }
                
                if (highlighted.mid(i, 3) == quoteType) {
                    if (i == 0 || highlighted[i-1] != '\\') {
                        quoteEnd = i + 2; // +2 чтобы включить все три кавычки
                        break;
                    }
                }
            }
        } else {
            // Для обычных кавычек используем стандартную логику
            for (int i = quoteStart + 1; i < highlighted.length(); i++) {
                // Пропускаем содержимое внутри span тегов
                if (isInsideHtmlTag(highlighted, i)) {
                    int spanStart = highlighted.lastIndexOf("<span", i);
                    if (spanStart != -1) {
                        int openTagEnd = highlighted.indexOf(">", spanStart);
                        if (openTagEnd != -1) {
                            int spanEnd = highlighted.indexOf("</span>", openTagEnd + 1);
                            if (spanEnd != -1) {
                                i = spanEnd + 7;
                                continue;
                            }
                        }
                    }
                }
                
                if (escaped) {
                    escaped = false;
                    continue;
                }
                if (highlighted[i] == '\\') {
                    escaped = true;
                    continue;
                }
                if (highlighted[i] == quoteType[0]) {
                    quoteEnd = i;
                    break;
                }
            }
        }
        
        if (quoteEnd == -1) {
            // Не нашли закрывающую кавычку - пропускаем эту позицию, чтобы не зависнуть
            pos = quoteStart + 1;
            continue;
        }
        
        // Подсвечиваем строку (ТОЧНО ТАК ЖЕ для тройных и обычных кавычек)
        int length = quoteEnd - quoteStart + 1;
        QString match = highlighted.mid(quoteStart, length);
        // Экранируем HTML спецсимволы в содержимом строки
        QString escapedMatch = match;
        escapedMatch.replace("&", "&amp;");
        escapedMatch.replace("<", "&lt;");
        escapedMatch.replace(">", "&gt;");
        // Используем одинарные кавычки в HTML атрибуте, чтобы избежать конфликта с кавычками в строке
        QString replacement = QString("<span style='color: %1;'>").arg(stringColor) + escapedMatch + "</span>";
        highlighted.replace(quoteStart, length, replacement);
        pos = quoteStart + replacement.length();
    }
    
    // Подсветка комментариев - после строк
    // Комментарии обрабатываем осторожно, так как они могут содержать уже подсвеченные span теги
    pos = 0;
    while (pos < highlighted.length()) {
        // Ищем начало комментария (не внутри HTML тега и не внутри строки)
        int commentStart = -1;
        for (int i = pos; i < highlighted.length(); i++) {
            if (highlighted[i] == '#' && !isInsideHtmlTag(highlighted, i)) {
                // Проверяем, не находимся ли мы внутри уже подсвеченной строки (span тега для строк)
                // Простая проверка: если перед нами есть span тег для строки и после него еще нет </span>,
                // значит мы внутри строки
                bool isInsideStringSpan = false;
                QString stringSpanPattern = QString("<span style='color: %1;'").arg(stringColor);
                int spanCheck = highlighted.lastIndexOf(stringSpanPattern, i);
                if (spanCheck != -1) {
                    int spanEnd = highlighted.indexOf("</span>", spanCheck);
                    if (spanEnd == -1 || spanEnd > i) {
                        isInsideStringSpan = true; // Мы внутри span тега строки
                    }
                }
                
                if (!isInsideStringSpan) {
                    commentStart = i;
                    break;
                }
            }
        }
        if (commentStart == -1) break;
        
        // Ищем конец комментария (до конца строки)
        int commentEnd = highlighted.indexOf('\n', commentStart);
        if (commentEnd == -1) commentEnd = highlighted.length();
        
        int commentLength = commentEnd - commentStart;
        QString commentText = highlighted.mid(commentStart, commentLength);
        
        // Экранируем только части комментария, которые НЕ являются span тегами
        QString escapedComment;
        int commentPos = 0;
        while (commentPos < commentText.length()) {
            int spanStart = commentText.indexOf("<span", commentPos);
            if (spanStart == -1) {
                // Нет больше span тегов - экранируем остаток
                QString remaining = commentText.mid(commentPos);
                remaining.replace("&", "&amp;");
                remaining.replace("<", "&lt;");
                remaining.replace(">", "&gt;");
                escapedComment += remaining;
                break;
            }
            
            // Экранируем текст до span тега
            QString before = commentText.mid(commentPos, spanStart - commentPos);
            before.replace("&", "&amp;");
            before.replace("<", "&lt;");
            before.replace(">", "&gt;");
            escapedComment += before;
            
            // Находим закрывающий тег для этого span
            int openTagEnd = commentText.indexOf(">", spanStart);
            if (openTagEnd == -1) {
                escapedComment += commentText.mid(spanStart);
                break;
            }
            
            int spanEnd = commentText.indexOf("</span>", openTagEnd + 1);
            if (spanEnd == -1) {
                // Не закрытый span - экранируем как обычный текст
                QString remaining = commentText.mid(spanStart);
                remaining.replace("&", "&amp;");
                remaining.replace("<", "&lt;");
                remaining.replace(">", "&gt;");
                escapedComment += remaining;
                break;
            }
            
            spanEnd += 7; // длина "</span>"
            // Добавляем span тег без изменений
            escapedComment += commentText.mid(spanStart, spanEnd - spanStart);
            commentPos = spanEnd;
        }
        
        QString replacement = QString("<span style=\"color: %1;\">").arg(commentColor) + escapedComment + "</span>";
        highlighted.replace(commentStart, commentLength, replacement);
        pos = commentStart + replacement.length();
    }
    
    // Подсветка чисел - после строк и комментариев
    QRegularExpression numberPattern(R"(\b\d+(\.\d+)?\b)");
    pos = 0;
    QRegularExpressionMatch numMatch = numberPattern.match(highlighted, pos);
    while (numMatch.hasMatch()) {
        pos = numMatch.capturedStart();
        QString number = numMatch.captured(0);
        // Проверяем, что число не внутри HTML тега
        if (!isInsideHtmlTag(highlighted, pos)) {
            // Числа обычно не содержат HTML, но на всякий случай экранируем
            QString escapedNumber = number;
            escapedNumber.replace("&", "&amp;");
            escapedNumber.replace("<", "&lt;");
            escapedNumber.replace(">", "&gt;");
            QString replacement = QString("<span style=\"color: %1;\">").arg(numberColor) + escapedNumber + "</span>";
            highlighted.replace(pos, number.length(), replacement);
            pos += replacement.length();
        } else {
            pos += number.length();
        }
        numMatch = numberPattern.match(highlighted, pos);
    }
    
    // Ключевые слова Python - обрабатываем после строк, чтобы не трогать строки
    QStringList keywords = {
        "and", "as", "assert", "break", "class", "continue", "def", "del", "elif", "else",
        "except", "False", "finally", "for", "from", "global", "if", "import", "in", "is",
        "lambda", "None", "nonlocal", "not", "or", "pass", "raise", "return", "True", "try",
        "while", "with", "yield"
    };
    
    // Подсветка ключевых слов
    for (const QString &keyword : keywords) {
        QRegularExpression pattern("\\b" + QRegularExpression::escape(keyword) + "\\b");
        pos = 0;
        QRegularExpressionMatch keywordMatch = pattern.match(highlighted, pos);
        while (keywordMatch.hasMatch()) {
            pos = keywordMatch.capturedStart();
            // Проверяем, что не внутри HTML тега или строки
            if (!isInsideHtmlTag(highlighted, pos)) {
                // Ключевые слова обычно не содержат HTML, но на всякий случай экранируем
                QString escapedKeyword = keyword;
                escapedKeyword.replace("&", "&amp;");
                escapedKeyword.replace("<", "&lt;");
                escapedKeyword.replace(">", "&gt;");
                QString replacement = QString("<span style=\"color: %1;\">").arg(keywordColor) + escapedKeyword + "</span>";
                highlighted.replace(pos, keyword.length(), replacement);
                pos += replacement.length();
            } else {
                pos += keyword.length();
            }
            keywordMatch = pattern.match(highlighted, pos);
        }
    }
    
    // Встроенные функции
    QStringList builtins = {
        "print", "len", "range", "open", "type", "dir", "isinstance", "map", "list", "dict",
        "set", "int", "float", "str", "bool", "bytes", "input", "format", "split", "join",
        "upper", "lower", "strip", "replace", "find", "count", "append", "extend", "pop", "remove"
    };
    
    // Подсветка встроенных функций
    for (const QString &builtin : builtins) {
        QRegularExpression pattern("\\b" + QRegularExpression::escape(builtin) + "\\s*\\(");
        pos = 0;
        QRegularExpressionMatch builtinMatch = pattern.match(highlighted, pos);
        while (builtinMatch.hasMatch()) {
            pos = builtinMatch.capturedStart();
            // Проверяем, что не внутри HTML тега или строки
            if (!isInsideHtmlTag(highlighted, pos)) {
                // Встроенные функции обычно не содержат HTML, но на всякий случай экранируем
                QString escapedBuiltin = builtin;
                escapedBuiltin.replace("&", "&amp;");
                escapedBuiltin.replace("<", "&lt;");
                escapedBuiltin.replace(">", "&gt;");
                QString replacement = QString("<span style=\"color: %1;\">").arg(builtinColor) + escapedBuiltin + "</span>";
                highlighted.replace(pos, builtin.length(), replacement);
                pos += replacement.length() + 1; // +1 для скобки
            } else {
                pos += builtin.length() + 1;
            }
            builtinMatch = pattern.match(highlighted, pos);
        }
    }
    
    return highlighted;
}

QString HelpWidget::wrapWithStyles(const QString &content) {
    QString styled = content;
    
    // Находим и обрабатываем блоки <pre>...</pre>
    QRegularExpression prePattern(R"(<pre>(.*?)</pre>)");
    prePattern.setPatternOptions(QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatchIterator i = prePattern.globalMatch(styled);
    
    // Собираем все совпадения с их позициями
    struct MatchInfo {
        int start;
        int length;
        QString replacement;
    };
    QList<MatchInfo> matches;
    
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        QString code = match.captured(1);
        
        // Сначала собираем все span теги ДО подсветки (пока их еще нет)
        // Затем подсвечиваем синтаксис
        QString highlighted = highlightPythonCode(code);
        
        // Теперь нужно экранировать HTML символы в тексте, но не трогать сами span теги
        // Используем плейсхолдеры: заменяем span теги на уникальные маркеры перед экранированием
        QString result = highlighted;
        
        // Собираем все span теги с их позициями ДО экранирования
        struct SpanInfo {
            int start;
            int length;
            QString content;
            QString placeholder;
        };
        QList<SpanInfo> spanInfos;
        
        // Используем простой подход: ищем пары <span...>...</span> последовательно
        int placeholderIndex = 0;
        int searchPos = 0;
        
        while (true) {
            int spanStart = result.indexOf("<span", searchPos);
            if (spanStart == -1) break;
            
            int openTagEnd = result.indexOf(">", spanStart);
            if (openTagEnd == -1) {
                searchPos = spanStart + 1;
                continue;
            }
            
            // Ищем соответствующий закрывающий тег простым способом - первый </span> после открывающего тега
            // Но проверяем, что он не внутри другого span тега
            int pos = openTagEnd + 1;
            int spanEnd = -1;
            int depth = 1;
            
            while (pos < result.length()) {
                int nextOpen = result.indexOf("<span", pos);
                int nextClose = result.indexOf("</span>", pos);
                
                if (nextClose == -1) break;
                
                if (nextOpen != -1 && nextOpen < nextClose) {
                    // Вложенный span
                    depth++;
                    pos = nextOpen + 5;
                } else {
                    // Закрывающий тег
                    depth--;
                    if (depth == 0) {
                        spanEnd = nextClose;
                        break;
                    }
                    pos = nextClose + 7;
                }
            }
            
            if (spanEnd == -1) {
                searchPos = spanStart + 1;
                continue;
            }
            
            spanEnd += 7; // длина "</span>"
            QString spanContent = result.mid(spanStart, spanEnd - spanStart);
            
            // Сохраняем span тег БЕЗ ИЗМЕНЕНИЙ - его содержимое уже экранировано в highlightPythonCode
            // НЕ экранируем содержимое повторно!
            QString placeholder = QString("__SPLACEHOLDER%1__").arg(placeholderIndex++);
            
            spanInfos.append({spanStart, spanEnd - spanStart, spanContent, placeholder});
            searchPos = spanEnd;
        }
        
        // Заменяем span теги на плейсхолдеры (с конца к началу, чтобы не сбить позиции)
        for (int i = spanInfos.size() - 1; i >= 0; --i) {
            const SpanInfo &info = spanInfos[i];
            result.replace(info.start, info.length, info.placeholder);
        }
        
        // Экранируем весь текст (теперь span тегов в нем нет)
        // НО сначала экранируем &, чтобы не сломать уже экранированные символы
        result.replace("&", "&amp;");
        result.replace("<", "&lt;");
        result.replace(">", "&gt;");
        
        // Восстанавливаем span теги из списка (в прямом порядке, так как плейсхолдеры уникальны)
        // ВАЖНО: восстанавливаем ПЕРЕД финальным экранированием, чтобы span теги не были экранированы
        for (const SpanInfo &info : spanInfos) {
            // Плейсхолдеры не должны быть экранированы, так как они не содержат HTML символов
            // Но на всякий случай проверяем, что они не были случайно экранированы
            QString placeholder = info.placeholder;
            // Если плейсхолдер был экранирован, ищем его в экранированном виде
            if (!result.contains(placeholder)) {
                // Пробуем найти экранированную версию (но плейсхолдеры не должны содержать &, <, >)
                continue;
            }
            result.replace(placeholder, info.content);
        }
        
        // Дополнительная проверка: если остались плейсхолдеры, это означает ошибку
        // Но сначала попробуем найти и исправить "висячие" закрывающие теги </span>
        // Проходим по тексту и ищем </span>, которые не имеют соответствующего открывающего тега перед ними
        int checkPos = 0;
        int openSpanCount = 0;
        QList<int> orphanCloseTags;
        
        while (checkPos < result.length()) {
            int nextOpen = result.indexOf("<span", checkPos);
            int nextClose = result.indexOf("</span>", checkPos);
            
            if (nextOpen == -1 && nextClose == -1) break;
            
            if (nextClose != -1 && (nextOpen == -1 || nextClose < nextOpen)) {
                // Нашли закрывающий тег раньше открывающего
                if (openSpanCount == 0) {
                    // Это "висячий" закрывающий тег - удаляем его
                    orphanCloseTags.append(nextClose);
                } else {
                    openSpanCount--;
                }
                checkPos = nextClose + 7;
            } else if (nextOpen != -1) {
                openSpanCount++;
                // Находим закрывающую скобку этого открывающего тега
                int tagEnd = result.indexOf(">", nextOpen);
                if (tagEnd != -1) {
                    checkPos = tagEnd + 1;
                } else {
                    checkPos = nextOpen + 5;
                }
            }
        }
        
        // Удаляем "висячие" закрывающие теги (с конца к началу)
        for (int i = orphanCloseTags.size() - 1; i >= 0; --i) {
            result.remove(orphanCloseTags[i], 7); // 7 = длина "</span>"
        }
        
        // Финальная очистка: удаляем все "висячие" закрывающие теги </span>
        // Проходим по тексту и для каждого </span> проверяем, есть ли соответствующий <span перед ним
        int cleanupPos = 0;
        QList<int> toRemove;
        
        while (true) {
            int closePos = result.indexOf("</span>", cleanupPos);
            if (closePos == -1) break;
            
            // Ищем ближайший открывающий <span до этой позиции
            int lastOpenPos = -1;
            int searchPos = 0;
            int depth = 0;
            
            while (searchPos < closePos) {
                int nextOpen = result.indexOf("<span", searchPos);
                if (nextOpen == -1 || nextOpen >= closePos) break;
                
                int nextOpenEnd = result.indexOf(">", nextOpen);
                if (nextOpenEnd == -1 || nextOpenEnd >= closePos) {
                    searchPos = nextOpen + 1;
                    continue;
                }
                
                // Проверяем, есть ли закрывающий тег для этого открывающего до нашей позиции
                int testClose = result.indexOf("</span>", nextOpenEnd + 1);
                if (testClose == -1 || testClose >= closePos) {
                    // Этот открывающий тег не закрыт до нашей позиции - он может быть нашим
                    lastOpenPos = nextOpen;
                    depth++;
                }
                
                searchPos = nextOpenEnd + 1;
            }
            
            if (lastOpenPos == -1 || depth == 0) {
                // Нет соответствующего открывающего тега - помечаем для удаления
                toRemove.append(closePos);
            }
            
            cleanupPos = closePos + 7;
        }
        
        // Удаляем помеченные теги (с конца к началу)
        for (int i = toRemove.size() - 1; i >= 0; --i) {
            result.remove(toRemove[i], 7);
        }
        
        // Агрессивная очистка: удаляем все закрывающие теги, если их больше открывающих
        // Используем простой счетчик для балансировки
        int openBalance = 0;
        QString cleanedResult;
        cleanedResult.reserve(result.length());
        
        int i = 0;
        while (i < result.length()) {
            if (result.mid(i, 5) == "<span") {
                // Нашли открывающий тег
                int tagEnd = result.indexOf(">", i);
                if (tagEnd != -1) {
                    cleanedResult += result.mid(i, tagEnd - i + 1);
                    openBalance++;
                    i = tagEnd + 1;
                    continue;
                }
            } else if (result.mid(i, 7) == "</span>") {
                // Нашли закрывающий тег
                if (openBalance > 0) {
                    cleanedResult += "</span>";
                    openBalance--;
                }
                // Иначе просто пропускаем этот закрывающий тег
                i += 7;
                continue;
            }
            
            cleanedResult += result[i];
            i++;
        }
        
        result = cleanedResult;
        
        matches.append({match.capturedStart(), match.capturedLength(), 
                        QString("<pre>%1</pre>").arg(result)});
    }
    
    // Заменяем с конца к началу, чтобы не сбить позиции
    for (int j = matches.size() - 1; j >= 0; --j) {
        styled.replace(matches[j].start, matches[j].length, matches[j].replacement);
    }
    
    // Оборачиваем блоки <pre> в таблицу для лучшей поддержки стилей
    // Выбираем цвета в зависимости от темы
    bool isDark = (m_theme == "dark");
    QString tableBgColor = isDark ? "#2a2a2a" : "#d0d0d0";
    QString tableBorderColor = isDark ? "#555555" : "#cccccc";
    QString borderLeftColor = isDark ? "#4CAF50" : "#008000";
    QString codeColor = isDark ? "#e0e0e0" : "#000000";
    
    QString preTable = QString("<table width=\"100%\" cellpadding=\"5\" cellspacing=\"0\" style=\"background-color: %1; border: 1px solid %2; border-left: 4px solid %3; margin: 10px 0px;\"><tr><td><pre style=\"margin: 0px; padding: 0px; background-color: transparent; font-family: 'Courier New', monospace; font-size: 12pt; color: %4;\">")
        .arg(tableBgColor, tableBorderColor, borderLeftColor, codeColor);
    styled.replace(QRegularExpression("<pre>"), preTable);
    styled.replace("</pre>", "</pre></td></tr></table>");
    
    // Добавляем простые стили для заголовков и таблиц
    QString h1Color = isDark ? "#4A9EFF" : "#000080";
    QString h1BorderColor = isDark ? "#4A9EFF" : "#0080ff";
    QString h2Color = isDark ? "#B0B0B0" : "#404040";
    QString h2BorderColor = isDark ? "#808080" : "#808080";
    QString h3Color = isDark ? "#C0C0C0" : "#606060";
    QString tableThBgColor = isDark ? "#3a3a3a" : "#0080ff";
    QString tableThColor = isDark ? "#ffffff" : "#ffffff";
    QString tableThBorderColor = isDark ? "#555555" : "#cccccc";
    QString tableTdBorderColor = isDark ? "#555555" : "#cccccc";
    QString bodyColor = isDark ? "#e0e0e0" : "#000000";
    
    QString styles = QString(R"(
<style>
body { font-family: Arial, sans-serif; color: %1; }
h1 { color: %2; border-bottom: 2px solid %3; padding-bottom: 5px; }
h2 { color: %4; border-bottom: 1px solid %5; padding-bottom: 5px; margin-top: 15px; }
h3 { color: %6; margin-top: 10px; }
table { border-collapse: collapse; width: 100%; margin: 10px 0px; }
table th { background-color: %7; color: %8; padding: 8px; border: 1px solid %9; }
table td { padding: 6px; border: 1px solid %10; }
img { max-width: 100%; height: auto; }
</style>
)")
        .arg(bodyColor).arg(h1Color).arg(h1BorderColor).arg(h2Color).arg(h2BorderColor)
        .arg(h3Color).arg(tableThBgColor).arg(tableThColor).arg(tableThBorderColor).arg(tableTdBorderColor);
    
    return styles + styled;
}

QString HelpWidget::getHelpContent(const QString &key) {
    const QString external = loadExternalHelp(key);
    if (!external.isEmpty()) {
        return external;
    }
    if (key == "types_main") {
        return R"(<h1>1. Типы данных</h1>
<p><strong>Переменная в языке программирования</strong> — это название для зарезервированного места в памяти компьютера, предназначенное для хранения значений. 
Основываясь на типе данных переменной, интерпретатор выделяет необходимое количество памяти и решает, что может находиться в зарезервированной области памяти.</p>
<p><strong>Присвоение значения переменной:</strong> В Python вам не нужно объявлять тип переменной вручную. Объявление происходит автоматически (это называется динамическая типизация), 
когда вы присваиваете значение переменной. Знак равенства (=) используется для присвоения значения переменной.</p>
<table border="1" cellpadding="5">
<tr><th>Имя</th><th>Тип</th><th>Описание и пример</th></tr>
<tr><td>Целые Числа</td><td>int</td><td>Целые положительные или отрицательные числа: -35, 0, 24, 123467890033373747428</td></tr>
<tr><td>Числа с плавающей точкой</td><td>float</td><td>Дробные числа: 3.14, 2.5, -2.33333, 0.12334</td></tr>
<tr><td>Строки</td><td>str</td><td>Строки: "asdf", "Hello world", "123456"</td></tr>
<tr><td>Списки</td><td>list</td><td>Последовательность элементов: ["hello", -123, 0.34, "345"]</td></tr>
<tr><td>Словарь</td><td>dict</td><td>Последовательность пар элементов содержащих ключ-значение: {"Language": "Python", "Version": "3.8"}</td></tr>
<tr><td>Кортеж (Tuple)</td><td>tuple</td><td>Неизменяемая упорядоченная последовательность элементов: ("hostname", 1234, -0.45, -32)</td></tr>
<tr><td>Множество</td><td>set</td><td>Изменяемая неупорядоченная последовательность элементов: {10, "Name", -30, 4.02, 100}</td></tr>
<tr><td>Булевые значения</td><td>bool</td><td>Тип данных принимающий одно из двух значений: True (истина) или False (ложь)</td></tr>
</table>)";
    }
    
    if (key == "types_variables") {
        return R"(<h2>1.1. Переменные в Python:</h2>
<p>Переменная в языке программирования это название для зарезервированного места в памяти компьютера, предназначенное для хранения значений. 
Это означает, что когда вы создаете переменную, вы на самом деле резервируете определенное место в памяти компьютера. 
Основываясь на типе данных переменной, интерпретатор выделяет необходимое количество памяти и решает, что может находится в зарезервированной области памяти. 
Для понимания, можете думать о переменной как о коробке, в которую можно положить любую вещь, но только определенного размера. 
Размер в данном примере будет типом переменной. Это не совсем верное определение, но оно дает общее представление о картине в целом.</p>
<p><strong>Присвоение значения переменной:</strong></p>
<p>В Python вам не нужно объявлять тип переменной вручную (как, например в С++). Объявление происходит автоматически (это называется динамическая типизация), 
когда вы присваиваете значение переменной. Знак равенства ( = ) используется для присвоения значения переменной. 
Операнд по левую сторону от знака равно ( = ) это имя переменной, операнд по правую сторону - значение присвоенное этой переменной.</p>
<p><strong>Таблица - Обзор встроенных типов объектов</strong></p>
<table border="1" cellpadding="5">
<tr><th>Имя</th><th>Тип</th><th>Описание и пример</th></tr>
<tr><td>Целые Числа</td><td>int</td><td>Целые положительные или отрицательные числа<br>-35, 0, 24, 123467890033373747428</td></tr>
<tr><td>Числа с плавающей точкой</td><td>float</td><td>Дробные числа 3.14, 2.5, -2.33333, 0.12334</td></tr>
<tr><td>Строки</td><td>str</td><td>Строки «asdf», «Hello world», «123456»</td></tr>
<tr><td>Списки</td><td>list</td><td>последовательность элементов<br>[«hello», -123, 0.34, «345»]</td></tr>
<tr><td>Словарь</td><td>dict</td><td>Последовательность пар элементов содержаших ключ-значение (key-value)<br>{«Language»: «Python», «Version»: «3.8»}</td></tr>
<tr><td>Кортеж (Tuple)</td><td>tuple</td><td>Неизменяемая упорядоченная последовательность элементов<br>(«hostname», 1234, -0.45, -32)</td></tr>
<tr><td>Множество</td><td>set</td><td>Изменяемая неупорядоченная последовательность элементов<br>{10, «Name», -30, 4.02, 100}</td></tr>
<tr><td>Булевые значения</td><td>bool</td><td>Тип данных принимающий одно из двух значений<br>true - истина<br>false - ложь</td></tr>
</table>)";
    }
    
    if (key == "types_numbers") {
        return R"(<h2>1.2. Числа</h2>
<p>Числа - Это не изменяемый тип данных. Числа в Python бывают трёх типов: целые, с плавающей точкой и комплексные.</p>
<ul>
<li>Примером целого числа может служить 2.</li>
<li>Примерами чисел с плавающей точкой (или «плавающих» для краткости) могут быть 3.23 и 52.3E-4. 
Обозначение E показывает степени числа 10. В данном случае 52.3E-4 означает 52.3 · 10<sup>-4</sup>.</li>
<li>Примеры комплексныхчисел:(-5+4j)и(2.3-4.6j)</li>
</ul>
<div style="background-color: #e8f4f8; padding: 10px; margin: 10px 0; border-left: 4px solid #2196F3;">
<p><strong>Примечание</strong></p>
<p>Нет отдельного типа 'long int' (длинное целое). Целые числа по умолчанию могут быть произвольной длины.</p>
</div>)";
    }
    
    if (key == "types_strings") {
        return R"(<h2>1.3. Строки</h2>
<p>Строки - это неизменяемая упорядоченная последовательность символов, заключенная в кавычки. 
Строки применяются для записи текстовой информации (кажем, вашего имени) и произвольных совокупностей байтов 
(наподобие содержимого файла изображения). Они являются первым примером того, что в Python называется последовательностью — 
позиционно упорядоченной коллекцией других объектов. Для содержащихся элементов последовательности поддерживают порядок слева направо: 
элементы сохраняются и извлекаются по своим относительным позициям. Строго говоря, строки представляют собой последовательности односимвольных строк.</p>
<p>Строки можно суммировать. Тогда они объединяются в одну строку, такая операция называется «Конкатенацией строк»:</p>
<pre>firts_string = "asdfgh"
second_string = "oiuytr"
print(firts_string + second_string)</pre>
<div style="background-color: #e8f4f8; padding: 10px; margin: 10px 0; border-left: 4px solid #2196F3;">
<p><strong>Примечание</strong></p>
<p>В Python 3 нет ASCII-строк, потому что Unicode является надмножеством (включает в себя) ASCII. 
Если необходимо получить строку строго в кодировке ASCII, используйте str.encode(«ascii»). По умолчанию все строки в Unicode.</p>
</div>
<div style="background-color: #fff3cd; padding: 10px; margin: 10px 0; border-left: 4px solid #ffc107;">
<p><strong>Предупреждение</strong></p>
<p>Нельзя производить арифметтические операции над строкам и числами<br>
Например:<br>
«qwerty» + 3<br>
Это вызовет ошибку, Но строки можно перемножать<br>
«#» * 10<br>
выведет на экран строку<br>
##########</p>
</div>)";
    }
    
    if (key == "types_strings_logical") {
        return R"(<h3>1.3.1. Логические и физические строки</h3>
<p>Физическая строка – это то, что вы видите, когда набираете программу. Логическая строка – это то, что Python видит как единое предложение. 
Python неявно предполагает, что каждой физической строке соответствует логическая строка. Примером логической строки может служить предложение 
print(„Привет, Мир!") – если оно на одной строке (как вы видите это в редакторе), то эта строка также соответствует физической строке. 
Python неявно стимулирует использование по одному предложению на строку, что облегчает чтение кода.</p>
<p>Чтобы записать более одной логической строки на одной физической строке, вам придётся явно указать это при помощи точки с запятой (;), 
которая отмечает конец логической строки/предложения. Например:</p>
<pre>i=5
print(i)
#то же самое, что
i = 5; print(i);
#и то же самое может быть записано в виде
i = 5; print(i);</pre>
<p>Однако я настоятельно рекомендую вам придерживаться написания одной логической строки в каждой физической строке. 
Таким образом вы можете обойтись совсем без точки с запятой. Кстати, я никогда не использовал и даже не встречал точки с запятой в программах на Python.</p>
<p>Можно использовать более одной физической строки для логической строки, но к этому следует прибегать лишь в случае очень длинных строк. 
Пример написания одной логической строки, занимающей несколько физических строк, приведён ниже. Это называется явным объединением строк.</p>
<pre>s = 'Это строка. \
Это строка продолжается.'
print(s) #Это строка. Это строка продолжается.</pre>)";
    }
    
    if (key == "types_strings_operations") {
        return R"(<h3>1.3.2. Операции над последовательностями</h3>
<p>Как последовательности, строки поддерживают операции, которые предполагают наличие позиционного порядка среди элементов. 
Например, если мы имеем четырех­ символьную строку, записанную в кавычках (обычно одинарных), то можем проверить ее длину с помощью 
встроенной функции <strong>len()</strong> и извлечь ее компоненты посредством выражений индексации.</p>
<pre>#!/usr/bin/env python3
# -*- coding: utf-8 -*-

simple_string = 'Spam'
len(simple_string)
# 4
simple_string[0]
# 'S'
simple_string[1]
# 'p'</pre>
<p>Нумерация всех символов в строке идет с нуля. Но, если нужно обратиться к какому-то по счету символу, начиная с конца, 
то можно указывать отрицательные значения (на этот раз с единицы).</p>
<pre>simple_string = "StringBody"
simple_string[1]
# t

simple_string[-1]
# y</pre>
<p>Кроме обращения к конкретному символу, можно делать срезы строк, указав диапазон номеров (срез выполняется по второе число, не включая его):</p>
<pre>example_string = "Lorem Ipsum is simply dummy text of the printing and typesetting"
example_string[0:9]
# 'Lorem Ips'

example_string[10:22]
# 'm is simply '

# Если не указывается второе число, то срез будет до конца строки:
example_string[-3:]
# 'ing'</pre>
<p>Также в срезе можно указывать шаг:</p>
<pre># Так можно получить нечетные числа
a = '0123456789'
a[1::2]
# '13579'

# А таким образом можно получить все четные числа строки a:
a[::2]
# '02468'

# Срезы также можно использовать для получения строки в обратном порядке:
a[::-1]
# '9876543210'</pre>)";
    }
    
    if (key == "types_strings_methods") {
        return R"(<h3>1.3.3. Методы для работы со строками</h3>
<p><strong>Методы upper, lower, swapcase, capitalize</strong></p>
<p>Методы upper(), lower(), swapcase(), capitalize() выполняют преобразование регистра строки:</p>
<pre>string1 = 'FastEthernet'

string1.upper()
#'FASTETHERNET'

string1.lower()
#'fastethernet'

string1.swapcase()
#'fASTeTHERNET'

string2 = 'tunnel 0'

string2.capitalize()
#'Tunnel 0'

#Очень важно обращать внимание на то, что часто методы возвращают преобразованную строку. И, значит, надо не забыть присвоить ее какой-то переменной (можно той же).
string1 = string1.upper()
print(string1)
#FASTETHERNET</pre>
<p><strong>Метод count</strong></p>
<p>Метод count() используется для подсчета того, сколько раз символ или подстрока встречаются в строке:</p>
<pre>string1 = 'Hello, hello, hello, hello'
string1.count('hello') # 3
string1.count('ello') # 4
string1.count('l') # 8</pre>
<p><strong>Метод find</strong></p>
<p>Методу find() можно передать подстроку или символ, и он покажет, на какой позиции находится первый символ подстроки (для первого совпадения):</p>
<pre>string1 = 'interface FastEthernet0/1'
string1.find('Fast') # 10
string1[string1.find('Fast')::] # 'FastEthernet0/1'</pre>
<p><strong>Методы startswith, endswith</strong></p>
<p>Проверка на то, начинается или заканчивается ли строка на определенные символы (методы startswith(), endswith()):</p>
<pre>string1 = 'FastEthernet0/1'
string1.startswith('Fast') # True
string1.startswith('fast') # False
string1.endswith('0/1') # True
string1.endswith('0/2') # False</pre>
<p><strong>Метод replace</strong></p>
<p>Замена последовательности символов в строке на другую последовательность (метод replace()):</p>
<pre>string1 = 'FastEthernet0/1'
string1.replace('Fast', 'Gigabit') # 'GigabitEthernet0/1'</pre>
<p><strong>Метод strip</strong></p>
<p>Часто при обработке файла файл открывается построчно. Но в конце каждой строки, как правило, есть какие-то спецсимволы (а могут быть и в начале). Например, перевод строки.</p>
<p>Для того, чтобы избавиться от них, очень удобно использовать метод strip():</p>
<pre>string1 = '\n\tinterface FastEthernet0/1\n'
print(string1)
#
#interface FastEthernet0/1
#

string1.strip()
#'interface FastEthernet0/1'</pre>
<div style="background-color: #e8f4f8; padding: 10px; margin: 10px 0; border-left: 4px solid #2196F3;">
<p><strong>Примечание</strong></p>
<p>По умолчанию метод strip() убирает пробельные символы. В этот набор символов входят: tnrfv</p>
<p>Методу strip можно передать как аргумент любые символы. Тогда в начале и в конце строки будут удалены все символы, которые были указаны в строке:</p>
<p>Метод strip() убирает спецсимволы и в начале, и в конце строки. Если необходимо убрать символы только слева или только справа, можно использовать, соответственно, методы lstrip() и rstrip().</p>
</div>
<p><strong>Метод split</strong></p>
<p>Метод split() разбивает строку на части, используя как разделитель какой-то символ (или символы) и возвращает список строк:</p>
<pre>string1 = 'switchport trunk allowed vlan 10,20,30,100-200'
commands = string1.split()
print(commands) # ['switchport', 'trunk', 'allowed', 'vlan', '10,20,30,100-200']

#По умолчанию в качестве разделителя используются пробельные символы (пробелы, табы, перевод строки), но в скобках можно указать любой разделитель:
vlans = commands[-1].split(',')
print(vlans) #['10', '20', '30', '100-200']</pre>
<p><strong>Метод join</strong></p>
<p>Метод join() позволяет объеденить список, кортеж или словарь в строку разделяя ее елементы другой строкой.</p>
<pre>myTuple = ("John", "Peter", "Vicky")
x = "-".join(myTuple)
print(x) #John-Peter-Vicky</pre>
<p><strong>Метод format</strong></p>
<p>Метод format() позволяет подставлять в отмеченные в строке области символами «{}» значени из списка аргументов</p>
<p>например:</p>
<pre>price = 49
txt = "The price is {} dollars"
print(txt.format(price))</pre>
<p>Так же можно указать тип подставлемых значений:</p>
<pre>#Строковые занчения
'{} {}'.format('one', 'two') #one two

#Числовые значения
'{} {}'.format(1, 2) #1 2

#порядок значений можно указывать
'{1} {0}'.format('one', 'two') #two one

#Можно так же подставлять значения классов
class Data(object):

    def __str__(self):
        return 'str'

    def __repr__(self):
        return 'repr'

'{0!s} {0!r}'.format(Data()) #str repr

#Отступы и выравнивания
#по правому краю
'{:>10}'.format('test') #      test

#по левому краю
'{:_<10}'.format('test') #test______

#по центру
'{:^10}'.format('test') #   test

#срезы
'{:.5}'.format('xylophone') #xylop

#срезы и отступы
'{:10.5}'.format('xylophone') #xylop

#числа
'{:d}'.format(42) #42
'{:f}'.format(3.141592653589793) #3.141593

#числа и отступы
'{:4d}'.format(42) #  42
'{:06.2f}'.format(3.141592653589793) #003.14
'{:04d}'.format(42) # 0042

#знаковые числа
'{:+d}'.format(42) #+42
'{: d}'.format((- 23)) #-23
'{: d}'.format(42) # 42
'{:=5d}'.format((- 23)) #-  23
'{:=+5d}'.format(23) #+  23

#можно вставлять значения по именам
data = {'first': 'Hodor', 'last': 'Hodor!'}
'{first} {last}'.format(**data) #Hodor Hodor!
'{first} {last}'.format(first='Hodor', last='Hodor!') #Hodor Hodor!

#Формат даты и времени
from datetime import datetime
'{:%Y-%m-%d %H:%M}'.format(datetime(2001, 2, 3, 4, 5)) #2001-02-03 04:05</pre>
<p>другие примеры форматированого вывода можно ныйти по следующим ссылкам <a href="https://pyformat.info">pyformat.info</a> <a href="https://www.w3schools.com/python/python_string_formatting.asp">w3schools.com</a></p>)";
    }
    
    if (key == "types_strings_example") {
        return R"(<h3>1.3.4. Пример программы</h3>
<p><strong>Подсчет слов</strong></p>
<pre>#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#Программа подсчета слов в файле
import os #подуль для взаимодействия с операционной системой, например просмотр файлов


def get_words(filename):
    #функци принимает в качестве аргумента путь до файла
    with open(filename, encoding="utf8") as file: #эта строка открывает файл
        text = file.read()  #читаем содержимое файла и записываем все в переменную text
        text = text.replace("\n", " ") #преобразуем наш текст в одну длинную строку заменив символ перевода строки на пробел
        text = text.replace(",", "").replace(".", "").replace("?", "").replace("!", "") #а так же уберем все запетые, пробелы, и прочие знаки пунктуации
        text = text.lower()  #перведем все слова в строчные, тоесть если было "Начало изучения Языка Программирования", то будет "начало изучения языка программирования"
        words = text.split() #создадим список слов ("списки","выгледят","вот","так")
        words.sort() #метод sort отсортирует все слова по алфовиту
    return words  #эта строка вернет нам массив строк


def get_words_dict(words):
    #эта функция содасть словарь где ключь, это слово, а значение, то сколько раз оно встречается в тексте
    words_dict = dict() #создаем простой словарь

    for word in words:  #цикл который перебирает наш список и записывает каждое слово в отдельную переменную word
        if word in words_dict:  #проверим условие, если слово уже есть в словаре,
            words_dict[word] = words_dict[word] + 1 # увеличим его значение на 1
        else:
            words_dict[word] = 1 # если не встречается, то просто присвоим значение 1
    return words_dict #вернем наш словарь


def main():
    # наша главная функция в которой выполняется основная логика нашей программы
    # Вызываются все созданные ранее функции, передаются значения, выводятся на экран результаты
    filename = input("Введите путь к файлу: ") #функция input() выводит на экран сообщение и ожидает ввода, все что будет введенобудет записано в переменную filename
    if not os.path.exists(filename): #проверяем соществует ли наш файл на диске
        print("Указанный файл не существует")
    else:
        words = get_words(filename)  #получаем список слов
        words_dict = get_words_dict(words) #получаем словарь из слов и их количества в тексте
        print("Кол-во слов: %d" % len(words)) #печатаем количество слов в тексте
        print("Кол-во уникальных слов: %d" % len(words_dict)) #печатаем количество уникальных слов
        print("Все использованные слова:") # и все слова
        for word in words_dict:
            print(word.ljust(20), words_dict[word])


if __name__ == "__main__":
    main()</pre>)";
    }
    
    if (key == "types_lists") {
        return R"(<h2>1.4. Списки</h2>
<p>Списки – это изменяемые упорядоченные последовательности произвольных объектов. Списки создаются посредством заключения элементов списка в квадратные скобки</p>
<pre>names = [ "Dave", "Mark", "Ann", "Phil" ]</pre>
<p>Элементы списка индексируются целыми числами, первый элемент списка имеет индекс, равный нулю. Для доступа к отдельным элементам списка используется оператор индексирования</p>
<pre>a = names[2] # Вернет третий элемент списка, "Ann"
names[0] = "Jeff" # Запишет имя "Jeff" в первый элемент списка</pre>
<p>С помощью оператора среза можно извлекать и изменять целые фрагменты списков:</p>
<pre>b = names[0:2]     # Вернет ["Jeff", "Mark"]
c = names[2:]      # Вернет ["Thomas", "Ann", "Phil", "Paula"]
names[1] = 'Jeff'  # Во второй элемент запишет имя 'Jeff'
names[0:2] = ['Dave','Mark','Jeff']  # Заменит первые два элемента списком справа.</pre>
<p>Списки могут содержать объекты любого типа, числа, строки, другие списки</p>
<pre>a = [1,"Dave",3.14, ["Mark", 7, 9, [100,101]], 10]</pre>
<p>Списки так же как и строки можно конкатинировать между собоой</p>
<pre>[1,2,3] + [4,5] # Создаст список [1,2,3,4,5]</pre>)";
    }
    
    if (key == "types_lists_methods") {
        return R"(<h3>1.4.1. Методы для работы со списками</h3>
<p><strong>List()</strong></p>
<p>создаст пустой список, либо преобразует аргументы в список</p>
<pre>l = list('1234567890')
print(l) # [1,2,3,4,5,6,7,8,9,0]</pre>
<p><strong>join()</strong></p>
<p>собирает список строк в одну строку с разделителем, который указан перед join:</p>
<pre>a = ['10', '20', '30']
print(','.join(a)) # 10,20,30</pre>
<p><strong>Примечание</strong></p>
<p>Метод join на самом деле относится к строкам, но так как значение ему надо передавать как список, он рассматривается тут.</p>
<p><strong>append()</strong></p>
<p>добавляет в конец списка указанный элемент:</p>
<pre>a = ['10', '20', '30', '100-200']
a.append('300')
print(a) # ['10', '20', '30', '100-200', '300']</pre>
<p><strong>extend()</strong></p>
<p>Если нужно объединить два списка, то можно использовать два способа: метод extend() и операцию сложения.</p>
<p>У этих способов есть важное отличие - extend меняет список, к которому применен метод, а суммирование возвращает новый список, который состоит из двух.</p>
<pre>a = ['10', '20', '30', '100-200']
b = ['300', '400', '500']
a.extend(b)
print(a) # ['10', '20', '30', '100-200', '300', '400', '500']</pre>
<p><strong>pop()</strong></p>
<p>удаляет элемент, который соответствует указанному номеру. Но, что важно, при этом метод возвращает этот элемент:</p>
<pre>a = ['10', '20', '30', '100-200']
a.pop(-1) # '100-200'
print(a) #['10', '20', '30']</pre>
<p><strong>Примечание</strong></p>
<p>Без указания номера удаляется последний элемент списка.</p>
<p><strong>remove()</strong></p>
<p>удаляет указанный элемент и не возвращает удаленный элемент:</p>
<pre>a = ['10', '20', '30', '100-200']
a.remove('20')
print(a) # ['10', '30', '100-200']</pre>
<p><strong>Примечание</strong></p>
<p>В методе remove надо указывать сам элемент, который надо удалить, а не его номер в списке. Если указать номер элемента, возникнет ошибка:</p>
<p><strong>insert()</strong></p>
<p>позволяет вставить элемент на определенное место в списке:</p>
<pre>a = ['10', '20', '30', '100-200']
a.insert(1, '15')
print(a) # ['10', '15', '20', '30', '100-200']</pre>
<p><strong>sort()</strong></p>
<p>сортирует список на месте:</p>
<pre>a = [1, 50, 10, 15]
a.sort()
print(a) # [1, 10, 15, 50]</pre>)";
    }
    
    if (key == "types_dicts") {
        return R"(<h2>1.5. Словари</h2>
<p>Словари Python — нечто совершенно иное; они вообще не являются последовательностями и взамен известны как отображения. Отображения также представляют собой коллекции других объектов, но они хранят объекты по ключам, а не по относительным позициям. В действительности отображения не поддерживают какой-либо надежный порядок слева направо; они просто отображают ключи на связанные значения. Словари — единственный тип отображения в наборе основных объектов Python — являются изменяемыми,' как и списки, их можно модифицировать на месте и они способны увеличиваться и уменьшаться по требованию. Наконец, подобно спискам словари — это гибкий инструмент для представления коллекций, но их мнемонические ключи лучше подходят, когда элементы коллекции именованы или помечены, скажем, как поля в записи базы данных.</p>
<p>Словари - это изменяемый упорядоченный тип данных:</p>
<ul>
<li>данные в словаре - это пары ключ: значение</li>
<li>доступ к значениям осуществляется по ключу, а не по номеру, как в списках</li>
<li>данные в словаре упорядочены по порядку добавления элементов</li>
<li>так как словари изменяемы, то элементы словаря можно менять, добавлять, удалять</li>
<li>ключ должен быть объектом неизменяемого типа: число, строка, кортеж</li>
<li>значение может быть данными любого типа</li>
</ul>
<p>для удобства словарь можно записать так:</p>
<pre>london = {
'id': 1,
'name':'London',
'it_vlan':320,
'user_vlan':1010,
'mngmt_vlan':99,
'to_name': None,
'to_id': None,
'port':'G1/0/11'
}</pre>
<p>Для того, чтобы получить значение из словаря, надо обратиться по ключу, таким же образом, как это было в списках, только вместо номера будет использоваться ключ:</p>
<pre>london = {'name': 'London1', 'location': 'London Str'}
print(london['name'], london['location'])
#'London1' 'London Str'</pre>
<p>При написании в виде литералов словари указываются в фигурных скобках и состо­ ят из ряда пар "ключ: значение". Словари удобны всегда, когда нам необходимо ассо­ циировать набор значений с ключами — например, для описания свойств чего-нибудь. Рассмотрим следующий словарь их трех элементов (с ключами ' food ', ' quantity ' и ' color ', возможно представляющих детали позиции гипотетического меню):</p>
<pre>D = {'food': 'Spam', 'quantity': 4, 'color': 'pink'}</pre>
<p>Мы можем индексировать этот словарь по ключу, чтобы извлекать и изменять зна­ чения, связанные с ключами. Операция индексации словаря имеет такой же синтак­ сис, как для последовательностей, но элементом в квадратных скобках будет ключ, а не относительная позиция:</p>
<pre>D['food'] # Извлечь значение, связанное с ключом 'food' 'Spam'
D['quantity'] += 1 # Добавить 1 к значению, связанному с ключом 'quantity'
print(D)
#{'color': 'pink', 'food': 'Spam', 'quantity': 5}</pre>
<p>Хотя форма литерала в фигурных скобках встречается, пожалуй, чаще приходится видеть словари, построенные другими способами (все данные программы редко из­ вестны до ее запуска). Скажем, следующий код начинает с пустого словаря и заполняет его по одному ключу за раз. В отличие от присваивания элементу в списке, находящемуся вне установленных границ, которое запрещено, присваивание новому ключу словаря приводит к созданию этого ключа:</p>
<pre>D = {}
D['name'] = 'Bob'
D['job'] = 'dev'
D['age'] =40
print (D)
#{'*:age 40, 'job': 'dev', 'name': 'Bob'}
print(D['name'])
#Bob</pre>
<p>В словаре в качестве значения можно использовать словарь:</p>
<pre>london_co = {
'r1': {
    'hostname': 'london_r1',
    'location': '21 New Globe Walk',
    'vendor': 'Cisco',
    'model': '4451',
    'ios': '15.4',
    'ip': '10.255.0.1'
},
'r2': {
    'hostname': 'london_r2',
    'location': '21 New Globe Walk',
    'vendor': 'Cisco',
    'model': '4451',
    'ios': '15.4',
    'ip': '10.255.0.2'
},
'sw1': {
    'hostname': 'london_sw1',
    'location': '21 New Globe Walk',
    'vendor': 'Cisco',
    'model': '3850',
    'ios': '3.6.XE',
    'ip': '10.255.0.101'
}

}</pre>
<p>Получить значения из вложенного словаря можно так:</p>
<pre>london_co['r1']['ios'] #'15.4'
london_co['r1']['model'] #'4451'
london_co['sw1']['ip'] #'10.255.0.101'</pre>)";
    }
    
    if (key == "types_dict_methods") {
        return R"(<h2>1.6. Методы для работы со словарями</h2>
<p><strong>clear()</strong></p>
<p>позволяет очистить словарь:</p>
<p><strong>copy()</strong></p>
<p>создает полную копию словаря</p>
<pre>london = {'name': 'London1', 'location': 'London Str', 'vendor': 'Cisco'}
london2 = london.copy()
id(london) #25524512
id(london2) #25563296
london['vendor'] = 'Juniper'
london2['vendor'] #'Cisco'</pre>
<p><strong>Примечание</strong></p>
<p>Если указать, что один словарь равен другому, то london2 будет ссылкой на словарь. И при изменениях словаря london меняется и словарь london2, так как это ссылки на один и тот же объект.</p>
<p><strong>get()</strong></p>
<p>запрашивает ключ, и если его нет, вместо ошибки возвращает None.</p>
<pre>london = {'name': 'London1', 'location': 'London Str', 'vendor': 'Cisco'}
print(london.get('ios')) #None

#Метод get() позволяет также указывать другое значение вместо None
print(london.get('ios', 'Ooops')) #Ooops</pre>
<p><strong>setdefault()</strong></p>
<p>ищет ключ, и если его нет, вместо ошибки создает ключ со значением None, если ключ есть, setdefault возвращает значение, которое ему соответствует:</p>
<pre>london = {'name': 'London1', 'location': 'London Str', 'vendor': 'Cisco'}
ios = london.setdefault('ios')
print(ios) #None
london #{'name': 'London1', 'location': 'London Str', 'vendor': 'Cisco', 'ios': None}
london.setdefault('name') #'London1'

#Второй аргумент позволяет указать, какое значение должно соответствовать ключу
model = london.setdefault('model', 'Cisco3580')
print(model) #Cisco3580
london
{'name': 'London1',
 'location': 'London Str',
 'vendor': 'Cisco',
 'ios': None,
 'model': 'Cisco3580'}

# Метод setdefault заменяет такую конструкцию:
if key in london:
     value = london[key]
else:
     london[key] = 'somevalue'
     value = london[key]</pre>
<p><strong>keys(), values(), items()</strong></p>
<p>Все три метода возвращают специальные объекты view, которые отображают ключи, значения и пары ключ-значение словаря соответственно.</p>
<p>Очень важная особенность view заключается в том, что они меняются вместе с изменением словаря. И фактически они лишь дают способ посмотреть на соответствующие объекты, но не создают их копию.</p>
<pre>london = {'name': 'London1', 'location': 'London Str', 'vendor': 'Cisco'}
keys = london.keys()
print(keys)
#dict_keys(['name', 'location', 'vendor'])

#Сейчас переменной keys соответствует view dict_keys, в котором три ключа: name, location и vendor.
#Но, если мы добавим в словарь еще одну пару ключ-значение, объект keys тоже поменяется:
london['ip'] = '10.1.1.1'
keys
#dict_keys(['name', 'location', 'vendor', 'ip'])

#Если нужно получить обычный список ключей, который не будет меняться с изменениями словаря, достаточно конвертировать view в список:
list_keys = list(london.keys())
list_keys
#['name', 'location', 'vendor', 'ip']</pre>
<p><strong>del()</strong></p>
<p>Удаляет ключ и значение</p>
<pre>london = {'name': 'London1', 'location': 'London Str', 'vendor': 'Cisco'}
del london['name']
london
#{'location': 'London Str', 'vendor': 'Cisco'}</pre>
<p><strong>update()</strong></p>
<p>Позволяет добавлять в словарь содержимое другого словаря:</p>
<pre>r1 = {'name': 'London1', 'location': 'London Str'}
r1.update({'vendor': 'Cisco', 'ios':'15.2'})
r1
# {'name': 'London1', 'location': 'London Str', 'vendor': 'Cisco', 'ios': '15.2'}

#Аналогичным образом можно обновить значения:
r1.update({'name': 'london-r1', 'ios':'15.4'})
r1

# {'name': 'london-r1',
#  'location': 'London Str',
#  'vendor': 'Cisco',
#  'ios': '15.4'}
</pre>)";
    }
    
    if (key == "types_tuples") {
        return R"(<h2>1.7. Кортежи</h2>
<p>Объект кортежа примерно похож на список, который нельзя изменять — кортежи являются последовательностями подобно спискам, но они неизменяемые подобно строка­ ми. Функционально они используются для представления фиксированных коллекций элементов: скажем, компонентов специфической даты в календаре. Синтаксически они записываются в круглых, а не квадратных скобках и поддерживают произвольные типы, произвольное вложение и обычные операции над последовательностями:</p>
<pre>T = (1, 2, 3, 4) # Кортеж из 4 элементов
len(Т)           # Длина 4
Т + (5, б)       # Конкатенация (1, 2, 3, 4, 5, 6)
Т[О]             # Индексация, нарезание и т.д.</pre>
<p><strong>Примечание</strong></p>
<p>Главное отличие кортежей заключается в том, что после создания их нельзя из­ менять, т.е. они являются неизменяемыми последовательностями (одноэлементные кортежи вроде приведенного ниже требуют хвостовой запятой): tuple2 = („password",)</p>
<p>Итак, зачем нам тип, который похож на список, но поддерживает меньше операций? Откровенно говоря, на практике кортежи применяются в целом не так часто, как списки, но весь смысл в их неизменяемости. Если вы передаете коллекцию объектов внутри своей программы в виде списка, тогда он может быть модифицирован где угодно; если вы используете кортеж, то изменить его не удастся. То есть кортежи обеспечивают своего рода ограничение целостности, что удобно в программах, крупнее тех, которые мы будем писать здесь. Позже в книге мы еще обсудим кортежи, включая расширение, которое построено поверх них и называется именованными кортежами.</p>)";
    }
    
    if (key == "types_sets") {
        return R"(<h2>1.8. Множество</h2>
<p>Множество - это изменяемый неупорядоченный тип данных. В множестве всегда содержатся только уникальные элементы.</p>
<p>Множество в Python - это последовательность элементов, которые разделены между собой запятой и заключены в фигурные скобки.</p>
<p>С помощью множества можно легко убрать повторяющиеся элементы:</p>
<pre>cities = ['Санкт-Петербург', 'Хабаровск', 'Казань', 'Санкт-Петербург', 'Казань']
un_cities = set(cities)
for city in un_cities:
    print("Один мой друг живёт в городе " + city)
# Один мой друг живёт в городе Хабаровск
# Один мой друг живёт в городе Санкт-Петербург
# Один мой друг живёт в городе Казань</pre>
<p>Множества полезны тем, что с ними можно делать различные операции и находить объединение множеств, пересечение и так далее.</p>
<p>Объединение множеств можно получить с помощью метода union() или оператора |:</p>
<pre>vlans1 = {10, 20, 30, 50, 100}
vlans2 = {100, 101, 102, 102, 200}
vlans1.union(vlans2)     # {10, 20, 30, 50, 100, 101, 102, 200}
vlans1 | vlans2         # {10, 20, 30, 50, 100, 101, 102, 200}</pre>
<p>Пересечение множеств можно получить с помощью метода intersection() или оператора &:</p>
<pre>vlans1 = {10, 20, 30, 50, 100}
vlans2 = {100, 101, 102, 102, 200}
vlans1.intersection(vlans2)  # {100}
vlans1 & vlans2              # {100}</pre>
<p><strong>Предупреждение:</strong> Нельзя создать пустое множество с помощью литерала (так как в таком случае это будет словарь): 
set1 = {} → type(set1) # dict. Но пустое множество можно создать так: set2 = set() → type(set2) # set</p>)";
    }
    
    if (key == "types_sets_methods") {
        return R"(<h2>1.9. Методы для работы с множествами</h2>
<p><strong>add()</strong></p>
<p>Добавляет элемент во множество:</p>
<pre>set1 = {10, 20, 30, 40}
set1.add(50)
set1  # {10, 20, 30, 40, 50}</pre>
<p><strong>discard()</strong></p>
<p>Позволяет удалять элементы, не выдавая ошибку, если элемента в множестве нет:</p>
<pre>set1 = {10, 20, 30, 40, 50}
set1.discard(55)  # Ничего не произойдет, ошибки не будет
set1.discard(50)  # Элемент удален
set1  # {10, 20, 30, 40}</pre>
<p><strong>clear()</strong></p>
<p>Очищает множество:</p>
<pre>set1 = {10, 20, 30, 40}
set1.clear()
set1  # set()</pre>)";
    }
    
    if (key == "types_bool") {
        return R"(<h2>1.10. Булевы значения</h2>
<p>Булевы значения в Python это две константы True и False.</p>
<p>В Python истинными и ложными значениями считаются не только True и False.</p>
<p><strong>Истинное значение:</strong></p>
<ul>
<li>любое ненулевое число</li>
<li>любая непустая строка</li>
<li>любой непустой объект</li>
</ul>
<p><strong>Ложное значение:</strong></p>
<ul>
<li>0</li>
<li>None</li>
<li>пустая строка</li>
<li>пустой объект</li>
</ul>
<p>Остальные истинные и ложные значения, как правило, логически следуют из условия.</p>
<p>Для проверки булевого значения объекта, можно воспользоваться bool:</p>
<pre>items = [1, 2, 3]
empty_list = []
bool(empty_list)  # False
bool(items)      # True
bool(0)          # False
bool(1)          # True</pre>)";
    }
    
    if (key == "types_conversion") {
        return R"(<h2>1.11. Преобразование типов</h2>
<p>В Python есть несколько полезных встроенных функций, которые позволяют преобразовать данные из одного типа в другой.</p>
<p><strong>int()</strong></p>
<p>Преобразует строку в int:</p>
<pre>int("10")                # 10
int("11111111", 2)       # 255 (преобразование из двоичной системы)</pre>
<p><strong>bin()</strong></p>
<p>Преобразовать десятичное число в двоичный формат:</p>
<pre>bin(10)   # '0b1010'
bin(255)  # '0b11111111'</pre>
<p><strong>hex()</strong></p>
<p>Аналогичная функция для преобразования в шестнадцатеричный формат:</p>
<pre>hex(10)   # '0xa'
hex(255)  # '0xff'</pre>
<p><strong>list()</strong></p>
<p>Функция list() преобразует аргумент в список:</p>
<pre>list("string")    # ['s', 't', 'r', 'i', 'n', 'g']
list({1, 2, 3})   # [1, 2, 3]
list((1, 2, 3))   # [1, 2, 3]</pre>
<p><strong>set()</strong></p>
<p>Функция set() преобразует аргумент в множество:</p>
<pre>set([1, 2, 3, 3, 4])  # {1, 2, 3, 4}
set((1, 2, 3, 3, 4))  # {1, 2, 3, 4}
set("string string")  # {' ', 'g', 'i', 'n', 'r', 's', 't'}</pre>
<p><strong>tuple()</strong></p>
<p>Функция tuple() преобразует аргумент в кортеж:</p>
<pre>tuple([1, 2, 3, 4])  # (1, 2, 3, 4)
tuple({1, 2, 3, 4})  # (1, 2, 3, 4)
tuple("string")      # ('s', 't', 'r', 'i', 'n', 'g')</pre>
<p><strong>str()</strong></p>
<p>Функция str() преобразует аргумент в строку:</p>
<pre>str(10)  # '10'</pre>)";
    }
    
    if (key == "types_checking") {
        return R"(<h2>1.12. Проверка типов</h2>
<p><strong>isdigit()</strong></p>
<p>Проверяет, состоит ли строка из одних только цифр:</p>
<pre>"a".isdigit()    # False
"a10".isdigit()   # False
"10".isdigit()    # True</pre>
<p><strong>isalpha()</strong></p>
<p>Проверяет, состоит ли строка из одних букв:</p>
<pre>"a".isalpha()      # True
"a100".isalpha()    # False
"a--  ".isalpha()   # False
"a ".isalpha()      # False</pre>
<p><strong>isalnum()</strong></p>
<p>Позволяет проверить, состоит ли строка из букв или цифр:</p>
<pre>"a".isalnum()    # True
"a10".isalnum()   # True</pre>
<p><strong>type()</strong></p>
<p>Иногда, в зависимости от результата, библиотека или функция может выводить разные типы объектов. 
Для проверки типа можно использовать функцию type():</p>
<pre>type("string")           # str
type("string") is str    # True
type((1, 2, 3))         # tuple
type((1, 2, 3)) is tuple # True
type((1, 2, 3)) is list  # False</pre>)";
    }
    
    if (key == "types_files") {
        return R"(<h2>1.13. Файлы</h2>
<p>Объекты файлов являются главным интерфейсом к внешним файлам на компьютере. Они могут применяться для чтения и записи текстовых заметок, 
аудиоклипов, документов Excel, сохраненных сообщений электронной почты и всего того, что вы в итоге сохранили на своем компьютере.</p>
<p>Чтобы создать объект файла, необходимо вызвать встроенную функцию open, передав ей в виде строк имя внешнего файла и необязательный режим обработки.</p>
<p>Например, для создания выходного текстового файла понадобится передать его имя и строку режима обработки 'w', чтобы записывать данные:</p>
<pre>f = open('data.txt', 'w')  # Создать новый файл в режиме записи ('w')
f.write('Hello\n')         # Записать в него строки символов
f.write('world\n')         # Возвратить количество записанных элементов
f.close()                  # Закрыть для сбрасывания буферов вывода на диск</pre>
<p>Чтобы прочитать то, что было записано, необходимо повторно открыть файл в режиме обработки 'r' для чтения текстового ввода 
(он выбирается по умолчанию, если в вызове строка режима не указана):</p>
<pre>f = open('data.txt')    # 'r' (чтение) - стандартный режим обработки
text = f.read()         # Прочитать все содержимое файла в строку
text                    # 'Hello\nworld\n'
print(text)            # Hello
                        # world
text.split()           # ['Hello', 'world']</pre>
<p><strong>Примечание:</strong> Для чтения всех строк из файла можно использовать цикл:</p>
<pre>for line in open('data.txt'):
    print(line)</pre>
<p>Также можно использовать конструкцию with, которая позволяет не закрывать файл вручную:</p>
<pre>with open(filename, encoding="utf8") as file:
    text = file.read()
    text = text.replace("\n", " ")
    # ... остальная обработка</pre>
<p>Такой подход позволяет не закрывать файл вручную - файл закроется автоматически после выхода из блока with.</p>)";
    }
    
    // Операторы
    if (key == "operators_main") {
        return R"(<h1>2. Операторы языка Python</h1>
<p>Большинство предложений (логических строк) в программах содержат выражения. Простой пример выражения: 2 + 3.</p>
<p>Выражение можно разделить на операторы и операнды.</p>
<p>Операторы – это некий функционал, производящий какие-либо действия, который может быть представлен в виде символов, как например +, 
или специальных зарезервированных слов. Операторы могут производить некоторые действия над данными, и эти данные называются операндами. 
В нашем случае 2 и 3 – это операнды.</p>)";
    }
    
    if (key == "operators_basic") {
        return R"(<h2>2.1. Базовые операторы</h2>
<p>Таблица операторов:</p>
<table border="1" cellpadding="5">
<tr><th>Оператор</th><th>Название</th><th>Объяснение</th><th>Примеры</th></tr>
<tr><td>"+"</td><td>Сложение</td><td>Суммирует два объекта</td><td>3 + 5 даст 8; "a" + "b" даст "ab"</td></tr>
<tr><td>"-"</td><td>Вычитание</td><td>Даёт разность двух чисел; если первый операнд отсутствует, он считается равным нулю</td><td>-5.2 даст отрицательное число, а 50 - 24 даст 26</td></tr>
<tr><td>"*"</td><td>Умножение</td><td>Даёт произведение двух чисел или возвращает строку, повторённую заданное число раз</td><td>2 * 3 даст 6. "la" * 3 даст "lalala"</td></tr>
<tr><td>"**"</td><td>Возведение в степень</td><td>Возвращает число х, возведённое в степень y</td><td>3**4 даст 81 (т.е. 3 * 3 * 3 * 3)</td></tr>
<tr><td>/</td><td>Деление</td><td>Возвращает частное от деления x на y</td><td>4 / 3 даст 1.3333333333333333</td></tr>
<tr><td>//</td><td>Целочисленное деление</td><td>Возвращает неполное частное от деления</td><td>4 // 3 даст 1. -4 // 3 даст -2</td></tr>
<tr><td>%</td><td>Деление по модулю</td><td>Возвращает остаток от деления</td><td>8 % 3 даст 2. -25.5 % 2.25 даст 1.5</td></tr>
<tr><td>&lt;&lt;</td><td>Сдвиг влево</td><td>Сдвигает биты числа влево на заданное количество позиций. (Любое число в памяти компьютера представлено в виде битов - или двоичных чисел, т.е. 0 и 1)</td><td>2 &lt;&lt; 2 даст 8. В двоичном виде 2 представляет собой 10. Сдвиг влево на 2 бита даёт 1000, что в десятичном виде означает 8</td></tr>
<tr><td>&gt;&gt;</td><td>Сдвиг вправо</td><td>Сдвигает биты числа вправо на заданное число позиций</td><td>11 &gt;&gt; 1 даст 5. В двоичном виде 11 представляется как 1011, что будучи смещённым на 1 бит вправо, даёт 101, а это, в свою очередь, не что иное как десятичное 5</td></tr>
<tr><td>&amp;</td><td>Побитовое И</td><td>Побитовая операция И над числами</td><td>5 &amp; 3 даёт 1</td></tr>
<tr><td>"|"</td><td>Побитовое ИЛИ</td><td>Побитовая операция ИЛИ над числами</td><td>5 | 3 даёт 7</td></tr>
<tr><td>^</td><td>Побитовое ИСКЛЮЧИТЕЛЬНО ИЛИ</td><td>Побитовая операция ИСКЛЮЧИТЕЛЬНО ИЛИ</td><td>5 ^ 3 даёт 6</td></tr>
<tr><td>~</td><td>Побитовое НЕ</td><td>Побитовая операция НЕ для числа x соответствует -(x+1)</td><td>~5 даёт -6</td></tr>
<tr><td>&lt;</td><td>Меньше</td><td>Определяет, верно ли, что x меньше y. Все операторы сравнения возвращают True или False. Обратите внимание на заглавные буквы в этих словах</td><td>5 &lt; 3 даст False, а 3 &lt; 5 даст True. Можно составлять произвольные цепочки сравнений: 3 &lt; 5 &lt; 7 даёт True</td></tr>
<tr><td>&gt;</td><td>Больше</td><td>Определяет, верно ли, что x больше y. Если оба операнда - числа, то перед сравнением они оба преобразуются к одинаковому типу. В противном случае всегда возвращается False</td><td>5 &gt; 3 даёт True</td></tr>
<tr><td>&lt;=</td><td>Меньше или равно</td><td>Определяет, верно ли, что x меньше или равно y</td><td>x = 3; y = 6; x &lt;= y даёт True</td></tr>
<tr><td>&gt;=</td><td>Больше или равно</td><td>Определяет, верно ли, что x больше или равно y</td><td>x = 4; y = 3; x &gt;= 3 даёт True</td></tr>
<tr><td>==</td><td>Равно</td><td>Проверяет, одинаковы ли объекты</td><td>x = 2; y = 2; x == y даёт True. x = "str"; y = "stR"; x == y даёт False. x = "str"; y = "str"; x == y даёт True</td></tr>
<tr><td>!=</td><td>Не равно</td><td>Проверяет, верно ли, что объекты не равны</td><td>x = 2; y = 3; x != y даёт True</td></tr>
<tr><td>not</td><td>Логическое НЕ</td><td>Если x равно True, оператор вернёт False. Если же x равно False, получим True</td><td>x = True; not x даёт False</td></tr>
<tr><td>and</td><td>Логическое И</td><td>x and y даёт False, если x равно False, в противном случае возвращает значение y</td><td>x = False; y = True; x and y возвращает False, поскольку x равно False. В этом случае Python не станет проверять значение y, так как уже знает, что левая часть выражения 'and' равняется False, что подразумевает, что и всё выражение в целом будет равно False, независимо от значений всех остальных операндов. Это называется укороченной оценкой булевых (логических) выражений</td></tr>
<tr><td>or</td><td>Логическое ИЛИ</td><td>Если x равно True, в результате получим True, в противном случае получим значение y</td><td>x = True; y = False; x or y даёт True. Здесь также может производиться укороченная оценка выражений</td></tr>
</table>)";
    }
    
    if (key == "operators_control") {
        return R"(<h2>2.2. Управляющие операторы</h2>
<p>Управляющие операторы позволяют изменять последовательность выполнения программы. К ним относятся:</p>
<ul>
<li>Условные операторы (if/else)</li>
<li>Оператор while</li>
<li>Цикл for</li>
<li>Оператор break</li>
<li>Оператор continue</li>
</ul>
<p><strong>Важно:</strong> В языке Python блоки разделяются табуляцией или пробелами. Хотя вы можете использовать для отступов пробелы или табуляции, 
их смешивание внутри блока обычно не будет удачной идеей - применяйте либо то, либо другое.</p>)";
    }
    
    if (key == "operators_if_else") {
        return R"(<h3>2.2.1. Условные операторы (if/else)</h3>
<p>Оператор if используется для проверки условий: если условие верно, выполняется блок выражений (называемый "if-блок"), 
иначе выполняется другой блок выражений (называемый "else-блок"). Блок "else" является необязательным.</p>
<p><strong>Предупреждение</strong></p>
<p>В языке Python блоки разделяются табами или пробелами</p>
<p>Запомните эмпирическое правило: хотя вы можете использовать для отступов пробелы или табуляции, их смешивание внутри блока обычно не будет удачной идеей применяйте либо то, либо другое. Формально табуляция считается достаточным количеством пробелов, чтобы сместить текущую строку на расстояние, кратное 8, и код будет работать в случае согласованного смешивания табуляций и пробелов. Тем не менее, такой код может быть сложнее изменять. Хуже того, смешивание табуляций и пробелов затрудняет чтение кода целиком, не говоря уже о правилах синтаксиса Python — табуляции в редакторе сменившего вас программиста могут выглядеть совсем не так, как в вашем редакторе.</p>
<p>Пример использования оператора if:</p>
<pre>number = 23
guess = int(input('Введите целое число : '))

if guess == number:
    print('Поздравляю, вы угадали,') # Здесь начинается новый блок
    print('(хотя и не выиграли никакого приза!)') # Здесь заканчивается новый блок
elif guess < number:
    print('Нет, загаданное число немного больше этого.') # Ещё один блок
    # Внутри блока вы можете выполнять всё, что угодно ...
else:
    print('Нет, загаданное число немного меньше этого.')
    # чтобы попасть сюда, guess должно быть больше, чем number

print('Завершено')
# Это последнее выражение выполняется всегда после выполнения оператора if</pre>)";
    }
    
    if (key == "operators_while") {
        return R"(<h3>2.2.2. Оператор while</h3>
<p>Оператор while — самая универсальная конструкция для итераций в языке Python. Выражаясь простыми терминами, он многократно выполняет блок операторов (обычно с отступом) до тех пор, пока проверка в заголовочной части оценивается как истинное значение. Это называется "циклом", потому что управление продолжает возвращаться к началу оператора, пока проверка не даст ложное значение. Когда результат проверки становится ложным, управление переходит на оператор, следующий после блока while. Совокупный эффект в том, что тело цикла выполняется многократно, пока проверка в заголовочной части дает истинное значение. Если проверка оценивается в ложное значение с самого начала, тогда тело цикла никогда не выполнится и оператор while пропускается.</p>
<p>В своей самой сложной форме оператор while состоит из строки заголовка с выражением проверки, тела с одним или большим количеством оператором с отступами и необязательной части else, которая выполняется, если управление покидает цикл, а оператор break не встретился. Python продолжает оценивать выражение проверки в строке заголовка и выполняет операторы, вложенные в тело цикла, пока проверка не возвратит ложное значение:</p>
<pre>number = 23
running = True

while running:
    guess = int(input('Введите целое число : '))

    if guess == number:
        print('Поздравляю, вы угадали.')
        running = False # это останавливает цикл while
    elif guess < number:
        print('Нет, загаданное число немного больше этого.')
    else:
        print('Нет, загаданное число немного меньше этого.')
else:
    print('Цикл while закончен.')
    # Здесь можете выполнить всё что вам ещё нужно

print('Завершение.')</pre>)";
    }
    
    if (key == "operators_for") {
        return R"(<h3>2.2.3. Цикл for</h3>
<p>Оператор for..in также является оператором цикла, который осуществляет итерацию по последовательности объектов, т.е. проходит через каждый элемент в последовательности. Мы узнаем больше о последовательностях в дальнейших главах, а пока просто запомните, что последовательность – это упорядоченный набор элементов.</p>
<p>Рассмотрим несколько примеров:</p>
<pre>for x in ["spam", "eggs", "ham"]:
    print(x, end=' ')
# результатом этого цикла будет строка spam eggs ham</pre>
<p><strong>Примечание</strong></p>
<p>Обратите внимание на параметр end=" ", по умолчанию функция print() завершает вывод символом конца строки "\n", в случае с end=" " мы меняем его на пробел. Таким образом мы выведем сообщения на экран в строку.</p>
<pre>for i in range(1, 5):
    print(i)
else:
    print('Цикл for закончен')</pre>)";
    }
    
    if (key == "operators_for_nested") {
        return R"(<h3>2.2.3.1. Вложенные циклы for</h3>
<p>Давайте теперь взглянем на цикл for, который сложнее тех, что мы видели до сих пор. В приведенном ниже примере иллюстрируется вложение операторов и конструкция else цикла for. Имея список объектов (items) и список ключей (tests), код ищет каждый ключ в списке объектов и сообщает о результате поиска:</p>
<pre>items = ["ааа", 111, (4, 5), 2.01]
tests = [(4, 5), 3.14]

for key in tests:
    for item in items:
        if item == key:
            print(key, "was found")
            break
    else:
        print(key, "not found!")</pre>)";
    }
    
    if (key == "operators_break") {
        return R"(<h3>2.2.4. Оператор break</h3>
<p>Оператор break служит для прерывания цикла, т.е. остановки выполнения команд даже если условие выполнения цикла ещё не приняло значения False 
или последовательность элементов не закончилась.</p>
<p><strong>Важно отметить:</strong> Если циклы for или while прервать оператором break, соответствующие им блоки else выполняться не будут.</p>
<pre>while True:
    s = input('Введите что-нибудь : ')
    if s == 'выход':
        break
    print('Длина строки:', len(s))

print('Завершение')</pre>)";
    }
    
    if (key == "operators_continue") {
        return R"(<h3>2.2.5. Оператор continue</h3>
<p>Оператор continue используется для указания Python, что необходимо пропустить все оставшиеся команды в текущем блоке цикла и продолжить со следующей итерации цикла.</p>
<pre>while True:
    s = input('Введите что-нибудь : ')
    if s == 'выход':
        break
    if len(s) < 3:
        print('Слишком мало')
        continue
    print('Введённая строка достаточной длины')
    # Разные другие действия здесь...</pre>
<p>Ну и в качестве маленького примера давайте нарисуем в консоли Ёлочку :)</p>
<pre>#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# программа рисует в консоли елочку
#                *
#               ***
#              *****
#             *******
#            *********
#           ***********

#Пример - 1
for i in range(1, 20, 2):  #функция range() указывает начальное значение, конечное значение и шаг.
    print('{:^20}'.format('*' * i))  # Печатаем символ выравнивая его по центру



#Пример -2
SPACE = ' '
STRAR = '*'

rows = int(input('Укажите размер ёлочки: '))
spaces = rows-1
stars = 1

for i in range(rows):
    print(
        (SPACE*spaces) +
        (STRAR*stars) +
        (SPACE*spaces)
    )
    stars += 2
    spaces -= 1</pre>)";
    }
    
    // Функции
    if (key == "functions_main") {
        return R"(<h1>3. Функции языка Python</h1>
<p>Функции – это многократно используемые фрагменты программы. Они позволяют дать имя определённому блоку команд 
с тем, чтобы впоследствии запускать этот блок по указанному имени в любом месте программы и сколь угодно много раз.
Это называется вызовом функции. Мы уже использовали много встроенных функций, как то len и range.</p>
<p>Функция – это, пожалуй, наиболее важный строительный блок любой нетривиальной программы (на любом языке программирования),
поэтому в этой главе мы рассмотрим различные аспекты функций.</p>
<p>Функции определяются при помощи зарезервированного слова def. После этого слова указывается имя функции, за которым следует
пара скобок, в которых можно указать имена некоторых переменных, и заключительное двоеточие в конце строки.
Далее следует блок команд, составляющих функцию. На примере можно видеть, что на самом деле это очень просто:</p>
<pre>def sayHello():
    print('Привет, Мир!')  # блок, принадлежащий функции
    # Конец функции

sayHello()  # вызов функции
sayHello()  # ещё один вызов функции</pre>)";
    }
    
    if (key == "functions_params") {
        return R"(<h2>3.1. Параметры функций</h2>
<p>Функции могут принимать параметры, т.е. некоторые значения, передаваемые функции для того, чтобы она что-либо сделала с ними. 
Эти параметры похожи на переменные, за исключением того, что значение этих переменных указывается при вызове функции, и во время 
работы функции им уже присвоены их значения.</p>
<p>Параметры указываются в скобках при объявлении функции и разделяются запятыми. Аналогично мы передаём значения, когда вызываем функцию. 
Обратите внимание на терминологию: имена, указанные в объявлении функции, называются параметрами, тогда как значения, которые вы передаёте 
в функцию при её вызове, – аргументами.</p>
<pre>def printMax(a, b):
    if a > b:
        print(a, 'максимально')
    elif a == b:
        print(a, 'равно', b)
    else:
        print(b, 'максимально')

printMax(3, 4)  # прямая передача значений

x = 5
y = 7
printMax(x, y)  # передача переменных в качестве аргументов</pre>)";
    }
    
    if (key == "functions_local") {
        return R"(<h2>3.2. Локальные переменные</h2>
<p>При объявлении переменных внутри определения функции, они никоим образом не связаны с другими переменными с таким же именем за пределами функции – 
т.е. имена переменных являются локальными в функции. Это называется областью видимости переменной. Область видимости всех переменных ограничена блоком, 
в котором они объявлены, начиная с точки объявления имени.</p>
<pre>x= 50

def func(x):
    print('x равен', x)
    x = 2
    print('Замена локального x на', x)

func(x)
print('x по-прежнему', x)</pre>)";
    }
    
    if (key == "functions_global") {
        return R"(<h2>3.3. Зарезервированное слово "global"</h2>
<p>Чтобы присвоить некоторое значение переменной, определённой на высшем уровне программы (т.е. не в какой-либо области видимости, 
как то функции или классы), необходимо указать Python, что её имя не локально, а глобально (global). Сделаем это при помощи 
зарезервированного слова global. Без применения зарезервированного слова global невозможно присвоить значение переменной, определённой за пределами функции.</p>
<p>Можно использовать уже существующие значения переменных, определённых за пределами функции (при условии, что внутри функции не было объявлено переменной с таким же именем). 
Однако, это не приветствуется, и его следует избегать, поскольку человеку, читающему текст программы, будет непонятно, где находится объявление переменной. Использование зарезервированного слова global достаточно ясно показывает, что переменная объявлена в самом внешнем блоке.</p>
<pre>x = 50

def func():
    global x

    print('x равно', x)
    x = 2
    print('Заменяем глобальное значение x на', x)

func()
print('Значение x составляет', x)</pre>)";
    }
    
    if (key == "functions_nonlocal") {
        return R"(<h2>3.4. Зарезервированное слово "nonlocal"</h2>
<p>Мы увидели, как получать доступ к переменным в локальной и глобальной области видимости. Есть ещё один тип области видимости, 
называемый "нелокальной" (nonlocal) областью видимости, который представляет собой нечто среднее между первыми двумя. 
Нелокальные области видимости встречаются, когда вы определяете функции внутри функций.</p>
<p>Поскольку в Python всё является выполнимым кодом, вы можете определять функции где угодно.</p>
<pre>def func_outer():
    x = 2
    print('x равно', x)

    def func_inner():
        nonlocal x
        x = 5

    func_inner()
    print('Локальное x сменилось на', x)

func_outer()</pre>)";
    }
    
    if (key == "functions_default") {
        return R"(<h2>3.5. Значения аргументов по умолчанию</h2>
<p>Зачастую часть параметров функций могут быть необязательными, и для них будут использоваться некоторые заданные значения по умолчанию, 
если пользователь не укажет собственных. Этого можно достичь с помощью значений аргументов по умолчанию. Их можно указать, добавив к имени параметра 
в определении функции оператор присваивания (=) с последующим значением.</p>
<p>Обратите внимание, что значение по умолчанию должно быть константой. Или точнее говоря, оно должно быть неизменным – это объясняется подробнее в последующих главах. А пока запомните это.</p>
<pre>def say(message, times = 1):
    print(message * times)

say('Привет')
say('Мир', 5)</pre>
<p><strong>Предупреждение</strong></p>
<p><strong>Важно</strong> Значениями по умолчанию могут быть снабжены только параметры, находящиеся в конце списка параметров. 
Таким образом, в списке параметров функции параметр со значением по умолчанию не может предшествовать параметру без значения по умолчанию. 
Это связано с тем, что значения присваиваются параметрам в соответствии с их положением. Например, def func(a, b=5) допустимо, а def func(a=5, b) – не допустимо.</p>)";
    }
    
    if (key == "functions_keyword") {
        return R"(<h2>3.6. Ключевые аргументы</h2>
<p>Если имеется некоторая функция с большим числом параметров, и при её вызове требуется указать только некоторые из них, 
значения этих параметров могут задаваться по их имени – это называется ключевые параметры. В этом случае для передачи аргументов функции 
используется имя (ключ) вместо позиции (как было до сих пор).</p>
<p>Есть два преимущества такого подхода: во-первых, использование функции становится легче, поскольку нет необходимости отслеживать порядок аргументов; 
во-вторых, можно задавать значения только некоторым избранным аргументам, при условии, что остальные параметры имеют значения аргумента по умолчанию.</p>
<pre>def func(a, b=5, c=10):
    print('a равно', a, ', b равно', b, ', а c равно', c)

func(3, 7)
func(25, c=24)
func(c=50, a=100)</pre>)";
    }
    
    if (key == "functions_varargs") {
        return R"(<h2>3.7. Переменное число параметров</h2>
<p>Иногда бывает нужно определить функцию, способную принимать любое число параметров. Этого можно достичь при помощи звёздочек (сохраните как function_varargs.py):</p>
<pre>def total(a=5, *numbers, **phonebook):
    print('a', a)

    #проход по всем элементам кортежа
    for single_item in numbers:
        print('single_item', single_item)

    #проход по всем элементам словаря
    for first_part, second_part in phonebook.items():
        print(first_part,second_part)

print(total(10,1,2,3,Jack=1123,John=2231,Inge=1560))</pre>)";
    }
    
    if (key == "functions_keyword_only") {
        return R"(<h2>3.8. Только ключевые параметры</h2>
<p>Если некоторые ключевые параметры должны быть доступны только по ключу, а не как позиционные аргументы, их можно объявить после параметра со звёздочкой (сохраните как keyword_only.py):</p>
<pre>def total(initial=5, *numbers, extra_number):
    count = initial
    for number in numbers:
        count += number
    count += extra_number
    print(count)

total(10, 1, 2, 3, extra_number=50)
total(10, 1, 2, 3)
# Вызовет ошибку, поскольку мы не указали значение
# аргумента по умолчанию для 'extra_number'.</pre>)";
    }
    
    if (key == "functions_return") {
        return R"(<h2>3.9. Оператор "return"</h2>
<p>Оператор return используется для возврата из функции, т.е. для прекращения её работы и выхода из неё. При этом можно также вернуть некоторое значение из функции.</p>
<pre>#!/usr/bin/python
# Filename: func_return.py

def maximum(x, y):
    if x > y:
        return x
    elif x == y:
        return 'Числа равны.'
    else:
        return y

print(maximum(2, 3))</pre>)";
    }
    
    if (key == "functions_docstring") {
        return R"(<h2>3.10. Строки документации</h2>
<p>Python имеет остроумную особенность, называемую строками документации, обычно обозначаемую сокращённо docstrings. 
Это очень важный инструмент, которым вы обязательно должны пользоваться, поскольку он помогает лучше документировать программу и облегчает её понимание. 
Поразительно, но строку документации можно получить, например, из функции, даже во время выполнения программы!</p>
<pre>def printMax(x, y):
    #Выводит максимальное из двух чисел.

    #Оба значения должны быть целыми числами.
    x = int(x) # конвертируем в целые, если возможно
    y = int(y)

    if x > y:
        print(x, 'наибольшее')
    else:
        print(y, 'наибольшее')

printMax(3, 5)
print(printMax.__doc__)</pre>)";
    }
    
    // Возвращаем заглушку для неизвестных ключей
    return "<h2>Содержимое раздела</h2><p>Раздел в разработке...</p>";
}

