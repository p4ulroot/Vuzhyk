#include "CodeEditor.h"

#include <QAbstractItemView>
#include <QAbstractItemModel>
#include <QStringListModel>
#include <QPointer>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QEvent>
#include <QSettings>
#include <QFont>
#include <QTextCursor>
#include <QRegularExpression>
#include <QTimer>
#include <QColor>
#include <QPainter>
#include <QTextDocument>
#include <QTextBlock>
#include <QScrollBar>
#include <Qsci/qsciapis.h>
#include <QImage>
#include <QPixmap>
#include <QPalette>

CodeEditor::CodeEditor(QWidget *parent)
    : QsciScintilla(parent)
    , m_lexer(nullptr) {
    
    // Настройка лексера Python
    setupLexer();
    
    // Настройка полей (margins) для номеров строк и маркеров
    setupMargins();
    
    // Настройка индикаторов для ошибок
    setupIndicators();
    
    // Устанавливаем шрифт
    QFont font;
    font.setFamily("Consolas, 'Courier New', monospace");
    font.setStyleHint(QFont::TypeWriter, QFont::PreferDefault);
    QSettings settings;
    int fontSize = settings.value("editor/fontSize", 10).toInt();
    m_currentFontSize = fontSize;
    font.setPointSize(fontSize);
    setFont(font);
    
    // Обновляем шрифт для лексера (чтобы он совпадал с основным шрифтом)
    if (m_lexer) {
        m_lexer->setFont(font);
    }
    
    // Устанавливаем тот же шрифт для margin (номеров строк)
    setMarginsFont(font);
    
    // Обновляем ширину колонок при инициализации
    updateMarginWidths();
    
    // Настройка табуляции (4 пробела)
    setIndentationsUseTabs(false);
    setIndentationWidth(4);
    setTabWidth(4);
    setTabIndents(true);
    setBackspaceUnindents(true);
    
    // Включаем встроенную автотабуляцию QScintilla
    setAutoIndent(true);
    
    // Включаем встроенный автокомплит QScintilla
    // AcsAll = AcsDocument (слова из документа) + AcsAPIs (ключевые слова из API)
    setAutoCompletionSource(QsciScintilla::AcsAll);
    setAutoCompletionThreshold(2);
    setAutoCompletionCaseSensitivity(false);
    setAutoCompletionReplaceWord(true);
    setAutoCompletionShowSingle(true);
    
    // Включаем подсветку текущей строки
    setCaretLineVisible(true);
    setCaretLineBackgroundColor(QColor(240, 240, 240));
    
    // Включаем скобки
    setBraceMatching(QsciScintilla::SloppyBraceMatch);
    
    // Включаем сворачивание кода (используем PlainFoldStyle, затем настроим стрелки вручную)
    setFolding(QsciScintilla::PlainFoldStyle);
    
    // Настраиваем маркеры сворачивания на стрелки в виде "птичек" (chevron)
    // Chevron - это две палки, образующие угол (например, > или <)
    // Создаем кастомные изображения chevron и используем их для маркеров
    // SC_MARKNUM_FOLDEROPEN (31) - развернутое состояние - chevron вниз (⌄)
    // SC_MARKNUM_FOLDER (30) - свернутое состояние - chevron вправо (>)
    
    // Создаем изображения chevron (цвет будет обновляться в applyTheme)
    // Используем черный цвет по умолчанию, затем обновим в applyTheme
    QImage chevronRight = createChevronImage(true, QColor(0, 0, 0));  // true = вправо
    markerDefine(chevronRight, 30);  // Свернуто - chevron вправо (>)
    
    QImage chevronDown = createChevronImage(false, QColor(0, 0, 0));  // false = вниз
    markerDefine(chevronDown, 31);  // Развернуто - chevron вниз (⌄)
    
    // Остальные маркеры для дерева сворачивания делаем пустыми
    SendScintilla(QsciScintilla::SCI_MARKERDEFINE, 25, QsciScintilla::SC_MARK_EMPTY);  // FOLDEREND
    SendScintilla(QsciScintilla::SCI_MARKERDEFINE, 26, QsciScintilla::SC_MARK_EMPTY);  // FOLDEROPENMID
    SendScintilla(QsciScintilla::SCI_MARKERDEFINE, 27, QsciScintilla::SC_MARK_EMPTY);  // FOLDERMIDTAIL
    SendScintilla(QsciScintilla::SCI_MARKERDEFINE, 28, QsciScintilla::SC_MARK_EMPTY);  // FOLDERTAIL
    SendScintilla(QsciScintilla::SCI_MARKERDEFINE, 29, QsciScintilla::SC_MARK_EMPTY);  // FOLDERSUB
    
    // Настройка курсора
    setCaretWidth(2);
    setCaretForegroundColor(QColor(0, 0, 0));
    
    // Подключаем сигналы
    connect(this, &QsciScintilla::marginClicked, this, &CodeEditor::onMarginClicked);
    connect(this, &QsciScintilla::textChanged, this, &CodeEditor::onTextChanged);
    
    // Создаем dummy document для совместимости
    m_dummyDocument = new QTextDocument(this);
    m_dummyDocument->setPlainText(text());
    
    // Подключаем сигналы для совместимости с document()
    connect(this, &QsciScintilla::textChanged, this, [this]() {
        if (m_dummyDocument) {
            m_dummyDocument->setPlainText(text());
            // Эмулируем сигналы QTextDocument
            emit m_dummyDocument->contentsChanged();
        }
    });
    
    // Применяем тему по умолчанию
    setTheme("light");
}

void CodeEditor::setupLexer() {
    if (!m_lexer) {
        m_lexer = new QsciLexerPython(this);
        m_currentLanguage = "python";
    }

    QFont font = this->font();
    if (font.family().isEmpty()) {
        font.setFamily("Consolas, 'Courier New', monospace");
        QSettings settings;
        int fontSize = settings.value("editor/fontSize", 10).toInt();
        font.setPointSize(fontSize);
    }

    m_lexer->setFont(font);
    setLexer(m_lexer);
    applyTheme(m_theme);

    if (auto python = qobject_cast<QsciLexerPython*>(m_lexer)) {
        QsciAPIs *apis = new QsciAPIs(python);
        if (apis) {
            QStringList keywords = {
                // Ключевые слова
                "and", "as", "assert", "break", "class", "continue", "def", "del",
                "elif", "else", "except", "False", "finally", "for", "from", "global",
                "if", "import", "in", "is", "lambda", "None", "nonlocal", "not",
                "or", "pass", "raise", "return", "True", "try", "while", "with", "yield",
                // Встроенные функции
                "abs", "all", "any", "ascii", "bin", "bool", "bytearray", "bytes",
                "callable", "chr", "classmethod", "compile", "complex", "delattr", "dict", "dir",
                "divmod", "enumerate", "eval", "exec", "filter", "float", "format", "frozenset",
                "getattr", "globals", "hasattr", "hash", "help", "hex", "id", "input",
                "int", "isinstance", "issubclass", "iter", "len", "list", "locals", "map",
                "max", "memoryview", "min", "next", "object", "oct", "open", "ord",
                "pow", "print", "property", "range", "repr", "reversed", "round", "set",
                "setattr", "slice", "sorted", "staticmethod", "str", "sum", "super", "type",
                "vars", "zip", "__import__",
                // Исключения
                "BaseException", "Exception", "ArithmeticError", "AssertionError", "AttributeError",
                "BufferError", "EOFError", "ImportError", "IndentationError", "IndexError",
                "KeyError", "KeyboardInterrupt", "LookupError", "MemoryError", "NameError",
                "NotImplementedError", "OSError", "OverflowError", "ReferenceError", "RuntimeError",
                "StopIteration", "SyntaxError", "SystemError", "SystemExit", "TypeError",
                "UnboundLocalError", "UnicodeError", "UnicodeDecodeError", "UnicodeEncodeError",
                "UnicodeTranslateError", "ValueError", "ZeroDivisionError",
                // Константы
                "Ellipsis", "NotImplemented", "__debug__", "__name__", "__file__", "__doc__"
            };

            for (const QString &keyword : keywords) {
                apis->add(keyword);
            }

            apis->prepare();
            python->setAPIs(apis);
        }
    }
}

void CodeEditor::setFileExtension(const QString &extension)
{
    QString normalized = extension.trimmed().toLower();
    if (normalized.startsWith('.')) {
        normalized = normalized.mid(1);
    }

    QString targetLanguage = "python";
    if (normalized == "html" || normalized == "htm" || normalized == "xhtml") {
        targetLanguage = "html";
    }

    if (targetLanguage == m_currentLanguage && m_lexer) {
        return;
    }

    if (m_lexer) {
        setLexer(nullptr);
        m_lexer->deleteLater();
        m_lexer = nullptr;
    }

    if (targetLanguage == "html") {
        m_lexer = new QsciLexerHTML(this);
    } else {
        m_lexer = new QsciLexerPython(this);
    }

    m_currentLanguage = targetLanguage;
    setupLexer();
}

void CodeEditor::updateAPIs(const QStringList &additionalWords) {
    if (!m_lexer) return;
    auto python = qobject_cast<QsciLexerPython*>(m_lexer);
    if (!python) {
        return;
    }
    
    // Безопасно получаем старый API
    QsciAPIs *oldAPIs = nullptr;
    QsciAbstractAPIs *oldAPIsAbstract = nullptr;
    try {
        oldAPIsAbstract = python->apis();
        if (oldAPIsAbstract) {
            oldAPIs = qobject_cast<QsciAPIs*>(oldAPIsAbstract);
        }
    } catch (...) {
        // Игнорируем ошибки при получении старого API
        oldAPIsAbstract = nullptr;
        oldAPIs = nullptr;
    }
    
    // Отменяем подготовку старого API, если она идет
    if (oldAPIs) {
        try {
            oldAPIs->cancelPreparation();
        } catch (...) {
            // Игнорируем ошибки при отмене подготовки
        }
    }
    
    // Создаем новый API объект
    QsciAPIs *apis = new QsciAPIs(python);
    if (!apis) return;
    
    // Добавляем базовые ключевые слова Python
    QStringList keywords = {
        // Ключевые слова
        "and", "as", "assert", "break", "class", "continue", "def", "del",
        "elif", "else", "except", "False", "finally", "for", "from", "global",
        "if", "import", "in", "is", "lambda", "None", "nonlocal", "not",
        "or", "pass", "raise", "return", "True", "try", "while", "with", "yield",
        // Встроенные функции
        "abs", "all", "any", "ascii", "bin", "bool", "bytearray", "bytes",
        "callable", "chr", "classmethod", "compile", "complex", "delattr", "dict", "dir",
        "divmod", "enumerate", "eval", "exec", "filter", "float", "format", "frozenset",
        "getattr", "globals", "hasattr", "hash", "help", "hex", "id", "input",
        "int", "isinstance", "issubclass", "iter", "len", "list", "locals", "map",
        "max", "memoryview", "min", "next", "object", "oct", "open", "ord",
        "pow", "print", "property", "range", "repr", "reversed", "round", "set",
        "setattr", "slice", "sorted", "staticmethod", "str", "sum", "super", "type",
        "vars", "zip", "__import__",
        // Исключения
        "BaseException", "Exception", "ArithmeticError", "AssertionError", "AttributeError",
        "BufferError", "EOFError", "ImportError", "IndentationError", "IndexError",
        "KeyError", "KeyboardInterrupt", "LookupError", "MemoryError", "NameError",
        "NotImplementedError", "OSError", "OverflowError", "ReferenceError", "RuntimeError",
        "StopIteration", "SyntaxError", "SystemError", "SystemExit", "TypeError",
        "UnboundLocalError", "UnicodeError", "UnicodeDecodeError", "UnicodeEncodeError",
        "UnicodeTranslateError", "ValueError", "ZeroDivisionError",
        // Константы
        "Ellipsis", "NotImplemented", "__debug__", "__name__", "__file__", "__doc__"
    };
    
    // Добавляем базовые ключевые слова
    for (const QString &keyword : keywords) {
        apis->add(keyword);
    }
    
    // Добавляем дополнительные слова (например, задачи pyrob)
    for (const QString &word : additionalWords) {
        if (!word.isEmpty() && word.length() > 1) {
            apis->add(word);
        }
    }
    
    // Устанавливаем API в лексер
    python->setAPIs(apis);
    
    // Подготавливаем API (асинхронно)
    apis->prepare();
    
    // Старый API удалится автоматически при следующем вызове
    if (oldAPIs) {
        oldAPIs->deleteLater();
    }
}

void CodeEditor::setupMargins() {
    // Шрифт для margin будет установлен после установки основного шрифта
    
    // Margin 0: маркеры (точки останова) - отдельная колонка слева
    setMarginType(0, QsciScintilla::SymbolMargin);
    setMarginWidth(0, 16); // Фиксированная ширина для брейкпоинтов (отдельная колонка)
    setMarginSensitivity(0, true);
    // Устанавливаем маску маркеров для margin 0 - брейкпоинты и маркер предпросмотра
    setMarginMarkerMask(0, (1 << BREAKPOINT_MARKER) | (1 << BREAKPOINT_DISABLED_MARKER) | (1 << BREAKPOINT_HOVER_MARKER));
    
    // Margin 1: номера строк - справа от брейкпоинтов
    setMarginType(1, QsciScintilla::NumberMargin);
    setMarginWidth(1, QString("0000"));
    setMarginLineNumbers(1, true);
    setMarginSensitivity(1, false); // Номера строк не кликабельны
    // Отключаем отображение маркеров в margin 1 (колонке номеров строк)
    setMarginMarkerMask(1, 0); // 0 = никакие маркеры не отображаются
    
    // Выравнивание номеров строк по правому краю
    // SC_MARGINOPTION_RIGHTALIGN = 1 (константа из Scintilla)
    SendScintilla(QsciScintilla::SCI_SETMARGINOPTIONS, 1, 1);
    
    // Устанавливаем отступ слева для margin 1 (номеров строк)
    // Это добавляет небольшой отступ между левым краем margin и номерами строк
    SendScintilla(QsciScintilla::SCI_SETMARGINLEFT, 1, 14); // 14 пикселей отступа слева
    
    // Определяем маркеры для точек останова
    // Маркеры будут созданы в updateBreakpointMarkers() с учетом размера шрифта
    updateBreakpointMarkers();
    
    // Устанавливаем темный фон для margin (как полоса между колонками)
    // Цвет будет обновляться в applyTheme
}

void CodeEditor::setupIndicators() {
    // Используем маркер для выделения строк с ошибками (красный фон строки)
    // SC_MARK_BACKGROUND закрашивает фон всей строки под текстом
    markerDefine(QsciScintilla::Background, ERROR_MARKER);
    // Устанавливаем цвет фона маркера - яркий красный
    // Используем только setMarkerBackgroundColor, который правильно конвертирует QColor
    QColor errorColor(255, 100, 100); // Яркий красный фон (R, G, B)
    setMarkerBackgroundColor(errorColor, ERROR_MARKER);
}

void CodeEditor::setCompleter(QCompleter *completer) {
    if (m_completer) {
        QObject::disconnect(m_completer, nullptr, this, nullptr);
    }
    m_completer = completer;
    if (!m_completer)
        return;
        m_completer->setWidget(this);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    
    connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated),
            this, &CodeEditor::insertCompletion);
    
    QAbstractItemView *popup = m_completer->popup();
    if (popup) {
        popup->setFont(this->font());
        int lineHeight = fontMetrics().height();
        popup->setStyleSheet(QString("QAbstractItemView::item { height: %1px; }").arg(lineHeight));
    }
}

void CodeEditor::insertCompletion(const QString &completion) {
    if (!m_completer || m_completer->widget() != this)
        return;
    
    QString completionPrefix = m_completer->completionPrefix();
    int pos = SendScintilla(QsciScintilla::SCI_GETCURRENTPOS);
    int startPos = pos - completionPrefix.length();
    
    // Удаляем префикс и вставляем завершение
    SendScintilla(QsciScintilla::SCI_SETSELECTIONSTART, startPos);
    SendScintilla(QsciScintilla::SCI_SETSELECTIONEND, pos);
    // Используем insert() для вставки текста вместо SCI_REPLACESEL
    insert(completion);
}

void CodeEditor::setCommentShortcut(const QKeySequence &sequence)
{
    m_commentShortcut = sequence;
}

void CodeEditor::setUncommentShortcut(const QKeySequence &sequence)
{
    m_uncommentShortcut = sequence;
}

bool CodeEditor::keyEventMatchesShortcut(const QKeyEvent *event, const QKeySequence &shortcut) const
{
    if (!event || shortcut.isEmpty()) {
        return false;
    }
    // Qt 5.12: QKeySequence::matches(key, modifiers, text) недоступен
    const QKeySequence pressed(event->key() | int(event->modifiers()));
    return pressed == shortcut;
}

void CodeEditor::commentSelectedLines()
{
    int lineFrom = 0;
    int indexFrom = 0;
    int lineTo = 0;
    int indexTo = 0;

    if (hasSelectedText()) {
        getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);
    } else {
        getCursorPosition(&lineFrom, &indexFrom);
        lineTo = lineFrom;
    }

    beginUndoAction();
    for (int line = lineTo; line >= lineFrom; --line) {
        const QString lineText = text(line);
        if (lineText.startsWith(QLatin1Char('#'))) {
            continue;
        }
        setCursorPosition(line, 0);
        insert(QStringLiteral("#"));
    }
    endUndoAction();

    if (hasSelectedText()) {
        setSelection(lineFrom, indexFrom + 1, lineTo, indexTo + 1);
    } else {
        setCursorPosition(lineFrom, indexFrom + 1);
    }
}

void CodeEditor::uncommentSelectedLines()
{
    int lineFrom = 0;
    int indexFrom = 0;
    int lineTo = 0;
    int indexTo = 0;

    if (hasSelectedText()) {
        getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);
    } else {
        getCursorPosition(&lineFrom, &indexFrom);
        lineTo = lineFrom;
    }

    const bool hadSelection = hasSelectedText();
    bool changed = false;

    beginUndoAction();
    for (int line = lineTo; line >= lineFrom; --line) {
        const QString lineText = text(line);
        if (!lineText.startsWith(QLatin1Char('#'))) {
            continue;
        }
        setSelection(line, 0, line, 1);
        removeSelectedText();
        changed = true;
    }
    endUndoAction();

    if (!changed) {
        return;
    }

    if (hadSelection) {
        const int newIndexFrom = indexFrom > 0 ? indexFrom - 1 : 0;
        const int newIndexTo = indexTo > 0 ? indexTo - 1 : 0;
        setSelection(lineFrom, newIndexFrom, lineTo, newIndexTo);
    } else {
        const int newIndex = indexFrom > 0 ? indexFrom - 1 : 0;
        setCursorPosition(lineFrom, newIndex);
    }
}

void CodeEditor::keyPressEvent(QKeyEvent *e) {
    // Проверяем, что редактор готов к работе
    if (!m_lexer || lines() < 0) {
        QsciScintilla::keyPressEvent(e);
            return;
    }

    if (m_currentLanguage == QLatin1String("python")) {
        if (keyEventMatchesShortcut(e, m_uncommentShortcut)) {
            uncommentSelectedLines();
            e->accept();
            return;
        }
        if (keyEventMatchesShortcut(e, m_commentShortcut)) {
            commentSelectedLines();
            e->accept();
            return;
        }
    }

    // Обработка Tab - вставляем 4 пробела
    // Но если автокомплит активен, Tab используется для выбора элемента из списка
    if (e->key() == Qt::Key_Tab && !(e->modifiers() & Qt::ControlModifier)) {
        // Если автокомплит активен, передаем управление QScintilla для выбора элемента
        if (isListActive()) {
            QsciScintilla::keyPressEvent(e);
            return;
        }
        
        if (hasSelectedText()) {
            // Отступ для выделенных строк
            int lineFrom, indexFrom, lineTo, indexTo;
            getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);
            beginUndoAction();
            for (int line = lineFrom; line <= lineTo; ++line) {
                setCursorPosition(line, 0);
                insert("    ");
            }
            // Восстанавливаем выделение после добавления отступов
            setSelection(lineFrom, indexFrom + 4, lineTo, indexTo + 4);
            endUndoAction();
            return;
        } else {
            // Получаем текущую позицию курсора
            int line, index;
            getCursorPosition(&line, &index);
            // Вставляем 4 пробела
            insert("    ");
            // Перемещаем курсор на позицию после вставленных пробелов
            setCursorPosition(line, index + 4);
            return;
        }
    }

    // Обработка BackTab - уменьшаем отступ
    if (e->key() == Qt::Key_Backtab) {
        if (hasSelectedText()) {
            int lineFrom, indexFrom, lineTo, indexTo;
            getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);
            beginUndoAction();
            for (int line = lineFrom; line <= lineTo; ++line) {
                QString lineText = text(line);
                if (lineText.startsWith("    ")) {
                    setSelection(line, 0, line, 4);
                    removeSelectedText();
                } else if (lineText.startsWith("\t")) {
                    setSelection(line, 0, line, 1);
                    removeSelectedText();
                }
            }
            endUndoAction();
            return;
        }
    }

    // Обработка Enter - используем встроенную автотабуляцию QScintilla
    // Автотабуляция обрабатывается автоматически через setAutoIndent(true)
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        // Если автокомплит активен, передаем управление базовому классу
        if (isListActive()) {
            QsciScintilla::keyPressEvent(e);
            return;
        }
        
        // Передаем обработку базовому классу - автотабуляция сработает автоматически
        QsciScintilla::keyPressEvent(e);
        return;
    }
    
    // Автозакрытие скобок и кавычек
    if (!e->text().isEmpty()) {
        QChar ch = e->text().at(0);
        
        // Обработка открывающих скобок и кавычек
        if (ch == '(' || ch == '[' || ch == '{' || ch == '"' || ch == '\'') {
            int line, index;
            getCursorPosition(&line, &index);
            
            // Получаем текущую позицию в документе
            int pos = positionFromLineIndex(line, index);
            
            // Проверяем, есть ли выделенный текст
            bool hasSelection = hasSelectedText();
            
            // Получаем символ после курсора (если есть)
            QChar nextChar;
            bool hasNextChar = false;
            if (pos >= 0 && pos < length()) {
                long charCode = SendScintilla(QsciScintilla::SCI_GETCHARAT, pos);
                if (charCode >= 0 && charCode <= 0xFFFF) {
                    nextChar = QChar(static_cast<ushort>(charCode));
                    hasNextChar = true;
                }
            }
            
            // Если есть выделенный текст, оборачиваем его в скобки/кавычки
            if (hasSelection) {
                int lineFrom, indexFrom, lineTo, indexTo;
                getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);
                
                QString closingChar;
                if (ch == '(') closingChar = ")";
                else if (ch == '[') closingChar = "]";
                else if (ch == '{') closingChar = "}";
                else if (ch == '"') closingChar = "\"";
                else if (ch == '\'') closingChar = "'";
                
                // Вставляем открывающую скобку/кавычку перед выделенным текстом
                setCursorPosition(lineFrom, indexFrom);
                insert(QString(ch));
                
                // Вставляем закрывающую скобку/кавычку после выделенного текста
                // Позиция конца выделения изменилась из-за вставки открывающей скобки
                int newPosTo = positionFromLineIndex(lineTo, indexTo) + 1; // +1 из-за вставленной открывающей
                int newLineTo, newIndexTo;
                lineIndexFromPosition(newPosTo, &newLineTo, &newIndexTo);
                setCursorPosition(newLineTo, newIndexTo);
                insert(closingChar);
                
                // Восстанавливаем выделение (текст между скобками/кавычками)
                setSelection(lineFrom, indexFrom + 1, newLineTo, newIndexTo);
        return;
    }
    
            // Если следующий символ - это уже закрывающая скобка/кавычка того же типа, просто перемещаем курсор
            if (hasNextChar && 
                ((ch == '(' && nextChar == ')') ||
                 (ch == '[' && nextChar == ']') ||
                 (ch == '{' && nextChar == '}') ||
                 (ch == '"' && nextChar == '"') ||
                 (ch == '\'' && nextChar == '\''))) {
                // Просто перемещаем курсор вперед
                setCursorPosition(line, index + 1);
        return;
    }

            // Передаем событие базовому классу для вставки открывающей скобки/кавычки
            QsciScintilla::keyPressEvent(e);
            
            // Получаем текущую позицию курсора (после вставки открывающей скобки)
            // Это позиция между открывающей и закрывающей скобкой
            int currentLine, currentIndex;
            getCursorPosition(&currentLine, &currentIndex);
            
            // Вставляем закрывающую скобку/кавычку
            QString closingChar;
            if (ch == '(') closingChar = ")";
            else if (ch == '[') closingChar = "]";
            else if (ch == '{') closingChar = "}";
            else if (ch == '"') closingChar = "\"";
            else if (ch == '\'') closingChar = "'";
            
            insert(closingChar);
            
            // После insert() курсор автоматически переместился после закрывающей скобки
            // Нужно вернуть его на позицию между скобками (где он был до вставки closingChar)
            setCursorPosition(currentLine, currentIndex);
            return;
        }
        
        // Обработка закрывающих скобок и кавычек - если следующий символ уже закрывающий, просто перемещаем курсор
        if (ch == ')' || ch == ']' || ch == '}' || ch == '"' || ch == '\'') {
            int line, index;
            getCursorPosition(&line, &index);
            int pos = positionFromLineIndex(line, index);
            
            QChar nextChar;
            if (pos >= 0 && pos < length()) {
                long charCode = SendScintilla(QsciScintilla::SCI_GETCHARAT, pos);
                if (charCode >= 0 && charCode <= 0xFFFF) {
                    nextChar = QChar(static_cast<ushort>(charCode));
                    if (nextChar == ch) {
                        // Следующий символ уже закрывающий - просто перемещаем курсор
                        setCursorPosition(line, index + 1);
                        return;
                    }
                }
            }
        }
    }
    
    // Обработка обычных клавиш
    // Встроенный автокомплит QScintilla работает автоматически
    QsciScintilla::keyPressEvent(e);
}

void CodeEditor::focusInEvent(QFocusEvent *e) {
    QsciScintilla::focusInEvent(e);
    if (m_completer) {
        m_completer->setWidget(this);
    }
}

void CodeEditor::wheelEvent(QWheelEvent *e) {
    // Изменение размера шрифта при Ctrl+колесо
    if (e->modifiers() & Qt::ControlModifier) {
        e->accept();
        
        // Используем сохраненный размер шрифта для более надежной работы
        int currentSize = m_currentFontSize;
        
        // Вычисляем изменение размера на основе направления вращения
        int angleDelta = e->angleDelta().y();
        int delta = 0;
        if (angleDelta > 0) {
            delta = 1; // Увеличиваем размер (вращение вверх)
        } else if (angleDelta < 0) {
            delta = -1; // Уменьшаем размер (вращение вниз)
        }
        
        // Если delta равен 0, не делаем ничего
        if (delta == 0) {
            QsciScintilla::wheelEvent(e);
        return;
    }

        // Вычисляем новый размер с ограничениями
        int newSize = qBound(6, currentSize + delta, 72);
        
        // Обновляем размер, если он изменился
        if (newSize != currentSize) {
            // Сохраняем новый размер
            m_currentFontSize = newSize;
            
            // Сохраняем новый размер в настройках
            QSettings settings;
            settings.setValue("editor/fontSize", newSize);
            
            // Обновляем шрифт редактора
            QFont currentFont = this->font();
            currentFont.setPointSize(newSize);
            setFont(currentFont);
            
            // Обновляем шрифт для лексера
            if (m_lexer) {
                m_lexer->setFont(currentFont);
            }
            
            // Обновляем шрифт для margin (номеров строк)
            setMarginsFont(currentFont);
            
            // Обновляем ширину колонок при изменении размера шрифта
            updateMarginWidths();
            
            // Обновляем размер маркеров брейкпоинтов
            updateBreakpointMarkers();
            
            emit fontSizeChanged(newSize);
        }
        return;
    }
    
    QsciScintilla::wheelEvent(e);
}

void CodeEditor::mouseMoveEvent(QMouseEvent *e) {
    QsciScintilla::mouseMoveEvent(e);
    
    // Проверяем, находится ли мышь над margin 0 (колонкой брейкпоинтов)
    QPoint pos = e->pos();
    
    // Получаем позицию в документе из координат мыши
    int position = SendScintilla(QsciScintilla::SCI_POSITIONFROMPOINT, pos.x(), pos.y());
    int line = SendScintilla(QsciScintilla::SCI_LINEFROMPOSITION, position);
    
    // Проверяем, находится ли мышь над margin 0
    // Margin 0 имеет фиксированную ширину 16 пикселей
    int margin0Width = SendScintilla(QsciScintilla::SCI_GETMARGINWIDTHN, 0);
    bool isOverMargin0 = (pos.x() >= 0 && pos.x() < margin0Width);
    
    if (isOverMargin0 && line >= 0 && line < lines()) {
        // Мышь над колонкой брейкпоинтов
        // Проверяем, нет ли уже брейкпоинта на этой строке
        if (!hasBreakpoint(line) && line != m_hoverBreakpointLine) {
            // Убираем предыдущий маркер предпросмотра
            if (m_hoverBreakpointLine >= 0) {
                markerDelete(m_hoverBreakpointLine, BREAKPOINT_HOVER_MARKER);
            }
            // Показываем маркер предпросмотра на текущей строке
            markerAdd(line, BREAKPOINT_HOVER_MARKER);
            m_hoverBreakpointLine = line;
        }
    } else {
        // Мышь не над колонкой брейкпоинтов - убираем маркер предпросмотра
        if (m_hoverBreakpointLine >= 0) {
            markerDelete(m_hoverBreakpointLine, BREAKPOINT_HOVER_MARKER);
            m_hoverBreakpointLine = -1;
        }
    }
}

void CodeEditor::leaveEvent(QEvent *e) {
    QsciScintilla::leaveEvent(e);
    
    // Убираем маркер предпросмотра при уходе мыши из редактора
    if (m_hoverBreakpointLine >= 0) {
        markerDelete(m_hoverBreakpointLine, BREAKPOINT_HOVER_MARKER);
        m_hoverBreakpointLine = -1;
    }
}

void CodeEditor::onMarginClicked(int margin, int line, Qt::KeyboardModifiers state) {
    Q_UNUSED(state);
    
    if (margin == 0) { // Margin 0 для маркеров (брейкпоинты) - слева
        // Убираем маркер предпросмотра перед установкой брейкпоинта
        if (m_hoverBreakpointLine >= 0) {
            markerDelete(m_hoverBreakpointLine, BREAKPOINT_HOVER_MARKER);
            m_hoverBreakpointLine = -1;
        }
        toggleBreakpoint(line);
    }
}

void CodeEditor::onTextChanged() {
    // Обновляем ширину margin для номеров строк при изменении текста
    // Margin 1 теперь для номеров строк
    // Ширина должна быть минимальной: только количество символов в максимальном номере строки
    
    int lineCount = lines();
    if (lineCount == 0) {
        lineCount = 1; // Минимум 1 строка для расчета
    }
    
    // Правильно считаем количество цифр в максимальном номере строки
    // Максимальный номер строки = lineCount (строки нумеруются с 1)
    int maxLineNumber = lineCount;
    int digits = 1;
    int temp = maxLineNumber;
    while (temp >= 10) {
        temp /= 10;
        ++digits;
    }
    
    // Создаем строку из максимального количества цифр для расчета ширины
    // Используем более точный расчет: ширина одного символа * количество символов
    // Это более точно, чем horizontalAdvance для строки, особенно для 4+ символов
    int charWidth = fontMetrics().horizontalAdvance('0'); // Ширина одного символа
    int textWidth = charWidth * digits; // Ширина всех символов
    
    // Ширина = ширина текста + отступ слева (14 пикселей) + отступ справа (4 пикселя)
    // Для 4+ символов добавляем дополнительный отступ для надежности
    int extraPadding = (digits >= 4) ? 6 : 0; // Дополнительный отступ для 4+ символов (увеличен до 6)
    int margin1Width = textWidth + 14 + 4 + extraPadding; // 14 слева + 4 справа + дополнительный отступ
    setMarginWidth(1, margin1Width);
    
    // Margin 0 (брейкпоинты) имеет фиксированную ширину и не обновляется
}

void CodeEditor::updateMarginWidths() {
    // Обновляем ширину margin 0 (колонка маркеров/брейкпоинтов) пропорционально размеру шрифта
    // Базовый размер 16 пикселей для размера шрифта 10
    // Масштабируем пропорционально: 16 * (currentSize / 10)
    int baseSize = 10;
    int baseWidth = 16;
    int scaledWidth = static_cast<int>(baseWidth * static_cast<double>(m_currentFontSize) / baseSize);
    // Округляем до ближайшего четного числа для лучшего отображения
    scaledWidth = (scaledWidth + 1) / 2 * 2;
    setMarginWidth(0, scaledWidth);
    
    // Обновляем ширину margin 1 (колонка номеров строк)
    // Используем ту же логику, что и в onTextChanged, но с учетом текущего размера шрифта
    int lineCount = lines();
    if (lineCount == 0) {
        lineCount = 1;
    }
    
    int maxLineNumber = lineCount;
    int digits = 1;
    int temp = maxLineNumber;
    while (temp >= 10) {
        temp /= 10;
        ++digits;
    }
    
    // Ширина одного символа зависит от размера шрифта
    int charWidth = fontMetrics().horizontalAdvance('0');
    int textWidth = charWidth * digits;
    
    // Ширина = ширина текста + отступ слева + отступ справа
    // Отступы также масштабируем пропорционально размеру шрифта
    int baseLeftPadding = 14;
    int baseRightPadding = 4;
    int scaledLeftPadding = static_cast<int>(baseLeftPadding * static_cast<double>(m_currentFontSize) / baseSize);
    int scaledRightPadding = static_cast<int>(baseRightPadding * static_cast<double>(m_currentFontSize) / baseSize);
    
    int extraPadding = (digits >= 4) ? static_cast<int>(6 * static_cast<double>(m_currentFontSize) / baseSize) : 0;
    int margin1Width = textWidth + scaledLeftPadding + scaledRightPadding + extraPadding;
    setMarginWidth(1, margin1Width);
    
    // Также обновляем отступ слева для margin 1
    SendScintilla(QsciScintilla::SCI_SETMARGINLEFT, 1, scaledLeftPadding);
}

void CodeEditor::updateBreakpointMarkers() {
    // Обновляем размер маркеров брейкпоинтов пропорционально размеру шрифта
    // Базовый размер маркера 10x10 пикселей для размера шрифта 10
    int baseSize = 10;
    int baseMarkerSize = 10;
    int scaledMarkerSize = static_cast<int>(baseMarkerSize * static_cast<double>(m_currentFontSize) / baseSize);
    // Округляем до ближайшего нечетного числа для лучшего отображения круга
    if (scaledMarkerSize % 2 == 0) {
        scaledMarkerSize += 1;
    }
    
    // Размер изображения должен быть немного больше маркера для отступов
    // Базовый размер изображения 13x11 для маркера 10x10
    int baseImageWidth = 13;
    int baseImageHeight = 11;
    int scaledImageWidth = static_cast<int>(baseImageWidth * static_cast<double>(m_currentFontSize) / baseSize);
    int scaledImageHeight = static_cast<int>(baseImageHeight * static_cast<double>(m_currentFontSize) / baseSize);
    
    // Создаем изображение красного круга (масштабируемое, прижат к левому краю)
    QImage redCircle(scaledImageWidth, scaledImageHeight, QImage::Format_ARGB32);
    redCircle.fill(Qt::transparent);
    QPainter painterRed(&redCircle);
    painterRed.setRenderHint(QPainter::Antialiasing);
    painterRed.setBrush(QBrush(QColor(255, 0, 0)));
    painterRed.setPen(Qt::NoPen);
    // Рисуем круг прижатым к левому краю (x=0)
    painterRed.drawEllipse(0, 0, scaledMarkerSize, scaledMarkerSize);
    painterRed.end();
    
    // Создаем изображение серого круга (масштабируемое, прижат к левому краю)
    QImage grayCircle(scaledImageWidth, scaledImageHeight, QImage::Format_ARGB32);
    grayCircle.fill(Qt::transparent);
    QPainter painterGray(&grayCircle);
    painterGray.setRenderHint(QPainter::Antialiasing);
    painterGray.setBrush(QBrush(QColor(128, 128, 128)));
    painterGray.setPen(Qt::NoPen);
    // Рисуем круг прижатым к левому краю (x=0)
    painterGray.drawEllipse(0, 0, scaledMarkerSize, scaledMarkerSize);
    painterGray.end();
    
    // Создаем изображение темно-красного круга для предпросмотра при наведении
    QImage darkRedCircle(scaledImageWidth, scaledImageHeight, QImage::Format_ARGB32);
    darkRedCircle.fill(Qt::transparent);
    QPainter painterDarkRed(&darkRedCircle);
    painterDarkRed.setRenderHint(QPainter::Antialiasing);
    // Более темный оттенок красного для предпросмотра (полупрозрачный)
    painterDarkRed.setBrush(QBrush(QColor(200, 0, 0, 180))); // Темно-красный с прозрачностью
    painterDarkRed.setPen(Qt::NoPen);
    painterDarkRed.drawEllipse(0, 0, scaledMarkerSize, scaledMarkerSize);
    painterDarkRed.end();
    
    // Используем RGBA изображения как маркеры
    markerDefine(redCircle, BREAKPOINT_MARKER);
    markerDefine(grayCircle, BREAKPOINT_DISABLED_MARKER);
    markerDefine(darkRedCircle, BREAKPOINT_HOVER_MARKER); // Маркер для предпросмотра
}

void CodeEditor::highlightErrorLines(const QList<int> &lineNumbers) {
    clearErrorHighlight();
    m_errorLineNumbers = lineNumbers;
    
    for (int lineNum : lineNumbers) {
        if (lineNum < 0 || lineNum >= lines()) continue;
        
        // Используем маркер для выделения всей строки красным фоном
        // Используем прямой вызов Scintilla API для надежности
        SendScintilla(QsciScintilla::SCI_MARKERADD, lineNum, ERROR_MARKER);
    }
    
    // Принудительно обновляем отображение
    update();
}

void CodeEditor::clearErrorHighlight() {
    // Удаляем все маркеры ошибок
    markerDeleteAll(ERROR_MARKER);
    m_errorLineNumbers.clear();
}

void CodeEditor::toggleBreakpoint(int lineNumber) {
    if (lineNumber < 0 || lineNumber >= lines()) return;
    
    if (m_breakpoints.contains(lineNumber)) {
        markerDelete(lineNumber, BREAKPOINT_MARKER);
        markerDelete(lineNumber, BREAKPOINT_DISABLED_MARKER);
        m_breakpoints.remove(lineNumber);
        emit breakpointToggled(lineNumber, false);
    } else {
        markerAdd(lineNumber, BREAKPOINT_MARKER);
        m_breakpoints.insert(lineNumber);
        emit breakpointToggled(lineNumber, true);
    }
}

void CodeEditor::setBreakpoint(int lineNumber, bool enabled) {
    if (lineNumber < 0 || lineNumber >= lines()) return;
    
    if (enabled) {
        if (!m_breakpoints.contains(lineNumber)) {
            markerAdd(lineNumber, BREAKPOINT_MARKER);
            m_breakpoints.insert(lineNumber);
            emit breakpointToggled(lineNumber, true);
        }
    } else {
        if (m_breakpoints.contains(lineNumber)) {
            markerDelete(lineNumber, BREAKPOINT_MARKER);
            markerDelete(lineNumber, BREAKPOINT_DISABLED_MARKER);
            m_breakpoints.remove(lineNumber);
            emit breakpointToggled(lineNumber, false);
        }
    }
}

bool CodeEditor::hasBreakpoint(int lineNumber) const {
    return m_breakpoints.contains(lineNumber);
}

void CodeEditor::setTheme(const QString &theme) {
    m_theme = theme;
    applyTheme(theme);
}

QTextDocument *CodeEditor::document() const {
    // QScintilla не использует QTextDocument напрямую
    // Возвращаем dummy document для совместимости (создается в конструкторе)
    return m_dummyDocument;
}

QString CodeEditor::toPlainText() const {
    return text();
}

void CodeEditor::setPlainText(const QString &text) {
    setText(text);
    if (m_dummyDocument) {
        m_dummyDocument->setPlainText(text);
    }
}

QTextCursor CodeEditor::textCursor() const {
    // Создаем QTextCursor из позиции в QScintilla
    int line, index;
    getCursorPosition(&line, &index);
    if (m_dummyDocument) {
        QTextCursor cursor(m_dummyDocument);
        // Перемещаем курсор к нужной позиции
        QTextBlock block = m_dummyDocument->findBlockByLineNumber(line);
        if (block.isValid()) {
            int pos = block.position() + qMin(index, block.length() - 1);
            cursor.setPosition(pos);
        }
        return cursor;
    }
    return QTextCursor();
}

void CodeEditor::setTextCursor(const QTextCursor &cursor) {
    if (!m_dummyDocument) return;
    
    // Получаем позицию из QTextCursor и устанавливаем в QScintilla
    int pos = cursor.position();
    QTextBlock block = m_dummyDocument->findBlock(pos);
    int line = block.blockNumber();
    int index = pos - block.position();
    setCursorPosition(line, index);
}

void CodeEditor::centerCursor() {
    int line, index;
    getCursorPosition(&line, &index);
    ensureLineVisible(line);
    ensureCursorVisible();
}

bool CodeEditor::isModified() const {
    // QScintilla использует SCI_GETMODIFY для проверки модификации
    return SendScintilla(QsciScintilla::SCI_GETMODIFY) != 0;
}

void CodeEditor::setModified(bool modified) {
    // QScintilla использует SCI_SETSAVEPOINT для сброса флага модификации
    if (!modified) {
        SendScintilla(QsciScintilla::SCI_SETSAVEPOINT);
        if (m_dummyDocument) {
            m_dummyDocument->setModified(false);
        }
        } else {
        // Для установки флага модификации нужно изменить текст
        // Но это не нужно, так как QScintilla автоматически отслеживает изменения
        if (m_dummyDocument) {
            m_dummyDocument->setModified(true);
        }
    }
}

void CodeEditor::applyTheme(const QString &theme) {
    if (theme == "light") {
        // Светлая тема "Light (Visual Studio)" из VS Code
        setPaper(QColor(255, 255, 255));  // #FFFFFF - белый фон
        setColor(QColor(0, 0, 0));  // #000000 - черный текст
        setCaretLineBackgroundColor(QColor(240, 240, 240));  // #F0F0F0 - подсветка текущей строки
        setCaretForegroundColor(QColor(0, 0, 0));  // #000000 - курсор черный
        
        // Устанавливаем фон виджета через палитру и авто-заполнение
        setAutoFillBackground(true);
        QPalette palette;
        palette.setColor(QPalette::Base, QColor(255, 255, 255));
        palette.setColor(QPalette::Window, QColor(255, 255, 255));
        palette.setColor(QPalette::Background, QColor(255, 255, 255));
        setPalette(palette);
        
        // Устанавливаем фон через Scintilla API для всех стилей (RGB формат: 0xRRGGBB)
        // Устанавливаем фон для всех стилей лексера Python
        for (int style = 0; style <= 31; ++style) {
            SendScintilla(QsciScintilla::SCI_STYLESETBACK, style, 0xFFFFFF);
        }
        // Устанавливаем фон для стиля по умолчанию (STYLE_DEFAULT = 32)
        SendScintilla(QsciScintilla::SCI_STYLESETBACK, 32, 0xFFFFFF);
        // Устанавливаем фон для пробелов
        SendScintilla(QsciScintilla::SCI_SETWHITESPACEBACK, true, 0xFFFFFF);
        
        // Цвета для номеров строк (как в VS Code)
        // Темный фон для margin (как полоса между колонками в VS Code)
        setMarginsBackgroundColor(QColor(240, 240, 240));  // #F0F0F0 - темный фон margin
        setMarginsForegroundColor(QColor(163, 163, 163));  // #A3A3A3 - серый цвет номеров строк
        
        // Устанавливаем цвет фона для области сворачивания кода (folding margin)
        setFoldMarginColors(QColor(240, 240, 240), QColor(240, 240, 240));  // Светлый фон для folding
        
        // Обновляем маркеры сворачивания (chevron) для светлой темы
        QImage chevronRight = createChevronImage(true, QColor(0, 0, 0));  // Черный chevron вправо
        markerDefine(chevronRight, 30);  // Свернуто - chevron вправо (>)
        QImage chevronDown = createChevronImage(false, QColor(128, 128, 128));  // Серый (тусклый) chevron вниз
        markerDefine(chevronDown, 31);  // Развернуто - chevron вниз (⌄)
        
        if (m_lexer) {
            m_lexer->setPaper(QColor(255, 255, 255));
            if (auto python = qobject_cast<QsciLexerPython*>(m_lexer)) {
                python->setColor(QColor(0, 0, 0), QsciLexerPython::Default);
                python->setColor(QColor(0, 0, 255), QsciLexerPython::Keyword);
                python->setColor(QColor(163, 21, 21), QsciLexerPython::SingleQuotedString);
                python->setColor(QColor(163, 21, 21), QsciLexerPython::DoubleQuotedString);
                python->setColor(QColor(163, 21, 21), QsciLexerPython::TripleSingleQuotedString);
                python->setColor(QColor(163, 21, 21), QsciLexerPython::TripleDoubleQuotedString);
                python->setColor(QColor(163, 21, 21), QsciLexerPython::SingleQuotedFString);
                python->setColor(QColor(163, 21, 21), QsciLexerPython::DoubleQuotedFString);
                python->setColor(QColor(163, 21, 21), QsciLexerPython::TripleSingleQuotedFString);
                python->setColor(QColor(163, 21, 21), QsciLexerPython::TripleDoubleQuotedFString);
                python->setColor(QColor(0, 128, 0), QsciLexerPython::Comment);
                python->setColor(QColor(0, 128, 0), QsciLexerPython::CommentBlock);
                python->setColor(QColor(121, 94, 38), QsciLexerPython::FunctionMethodName);
                python->setColor(QColor(38, 127, 153), QsciLexerPython::ClassName);
                python->setColor(QColor(0, 128, 0), QsciLexerPython::Number);
                python->setColor(QColor(0, 0, 0), QsciLexerPython::Operator);
                python->setColor(QColor(0, 0, 0), QsciLexerPython::Identifier);
                python->setColor(QColor(0, 0, 255), QsciLexerPython::Decorator);
                python->setColor(QColor(38, 127, 153), QsciLexerPython::HighlightedIdentifier);
            } else if (auto html = qobject_cast<QsciLexerHTML*>(m_lexer)) {
                html->setColor(QColor(0, 0, 0), QsciLexerHTML::Default);
                html->setColor(QColor(0, 0, 255), QsciLexerHTML::Tag);
                html->setColor(QColor(0, 128, 0), QsciLexerHTML::HTMLComment);
                html->setColor(QColor(163, 21, 21), QsciLexerHTML::HTMLValue);
                html->setColor(QColor(38, 127, 153), QsciLexerHTML::Attribute);
                html->setColor(QColor(209, 108, 9), QsciLexerHTML::Entity);
            }
        }
        
        // Цвет маркера ошибок для светлой темы (яркий красный фон)
        QColor errorColorLight(255, 100, 100); // Яркий красный (R, G, B)
        setMarkerBackgroundColor(errorColorLight, ERROR_MARKER);
    } else if (theme == "dark") {
        // Темная тема "Dark+" из VS Code
        setPaper(QColor(30, 30, 30));  // #1E1E1E - темный фон VS Code
        setColor(QColor(212, 212, 212));  // #D4D4D4 - светло-серый текст
        setCaretLineBackgroundColor(QColor(42, 45, 46));  // #2A2D2E - подсветка текущей строки
        setCaretForegroundColor(QColor(255, 255, 255));  // #FFFFFF - белый курсор
        
        // Устанавливаем фон виджета через палитру и авто-заполнение
        setAutoFillBackground(true);
        QPalette palette;
        palette.setColor(QPalette::Base, QColor(30, 30, 30));
        palette.setColor(QPalette::Window, QColor(30, 30, 30));
        palette.setColor(QPalette::Background, QColor(30, 30, 30));
        setPalette(palette);
        
        // Устанавливаем фон через Scintilla API для всех стилей (RGB формат: 0xRRGGBB)
        // Устанавливаем фон для всех стилей лексера Python
        for (int style = 0; style <= 31; ++style) {
            SendScintilla(QsciScintilla::SCI_STYLESETBACK, style, 0x1E1E1E);
        }
        // Устанавливаем фон для стиля по умолчанию (STYLE_DEFAULT = 32)
        SendScintilla(QsciScintilla::SCI_STYLESETBACK, 32, 0x1E1E1E);
        // Устанавливаем фон для пробелов
        SendScintilla(QsciScintilla::SCI_SETWHITESPACEBACK, true, 0x1E1E1E);
        
        // Цвета для номеров строк в темной теме (как в VS Code)
        setMarginsBackgroundColor(QColor(30, 30, 30));  // #1E1E1E - темный фон margin
        setMarginsForegroundColor(QColor(133, 133, 133));  // #858585 - серый цвет номеров строк
        
        // Устанавливаем цвет фона для области сворачивания кода (folding margin)
        setFoldMarginColors(QColor(30, 30, 30), QColor(30, 30, 30));  // Темный фон для folding
        
        // Обновляем маркеры сворачивания (chevron) для темной темы
        QColor chevronColorRight(212, 212, 212);  // Светлый цвет chevron вправо (#D4D4D4)
        QColor chevronColorDown(150, 150, 150);     // Более тусклый цвет chevron вниз
        QImage chevronRight = createChevronImage(true, chevronColorRight);  // Светлый chevron вправо
        markerDefine(chevronRight, 30);  // Свернуто - chevron вправо (>)
        QImage chevronDown = createChevronImage(false, chevronColorDown);  // Тусклый chevron вниз
        markerDefine(chevronDown, 31);  // Развернуто - chevron вниз (⌄)
        
        if (m_lexer) {
            m_lexer->setPaper(QColor(30, 30, 30));
            if (auto python = qobject_cast<QsciLexerPython*>(m_lexer)) {
                python->setColor(QColor(212, 212, 212), QsciLexerPython::Default);
                python->setColor(QColor(86, 156, 214), QsciLexerPython::Keyword);
                python->setColor(QColor(206, 145, 120), QsciLexerPython::SingleQuotedString);
                python->setColor(QColor(206, 145, 120), QsciLexerPython::DoubleQuotedString);
                python->setColor(QColor(206, 145, 120), QsciLexerPython::TripleSingleQuotedString);
                python->setColor(QColor(206, 145, 120), QsciLexerPython::TripleDoubleQuotedString);
                python->setColor(QColor(206, 145, 120), QsciLexerPython::SingleQuotedFString);
                python->setColor(QColor(206, 145, 120), QsciLexerPython::DoubleQuotedFString);
                python->setColor(QColor(206, 145, 120), QsciLexerPython::TripleSingleQuotedFString);
                python->setColor(QColor(206, 145, 120), QsciLexerPython::TripleDoubleQuotedFString);
                python->setColor(QColor(106, 153, 85), QsciLexerPython::Comment);
                python->setColor(QColor(106, 153, 85), QsciLexerPython::CommentBlock);
                python->setColor(QColor(220, 220, 170), QsciLexerPython::FunctionMethodName);
                python->setColor(QColor(78, 201, 176), QsciLexerPython::ClassName);
                python->setColor(QColor(181, 206, 168), QsciLexerPython::Number);
                python->setColor(QColor(212, 212, 212), QsciLexerPython::Operator);
                python->setColor(QColor(212, 212, 212), QsciLexerPython::Identifier);
                python->setColor(QColor(220, 220, 170), QsciLexerPython::Decorator);
                python->setColor(QColor(78, 201, 176), QsciLexerPython::HighlightedIdentifier);
            } else if (auto html = qobject_cast<QsciLexerHTML*>(m_lexer)) {
                html->setColor(QColor(212, 212, 212), QsciLexerHTML::Default);
                html->setColor(QColor(86, 156, 214), QsciLexerHTML::Tag);
                html->setColor(QColor(0, 255, 255), QsciLexerHTML::Attribute);
                html->setColor(QColor(255, 212, 123), QsciLexerHTML::HTMLValue);
                html->setColor(QColor(106, 153, 85), QsciLexerHTML::HTMLComment);
                html->setColor(QColor(255, 128, 128), QsciLexerHTML::Entity);
            }
        }
        
        // Цвет маркера ошибок для темной темы (красный фон, видимый на темном фоне)
        QColor errorColorDark(200, 50, 50); // Яркий красный, видимый на темном фоне (R, G, B)
        setMarkerBackgroundColor(errorColorDark, ERROR_MARKER);
    }
    
    // Обновляем цвета маркеров брейкпоинтов (одинаковые для обеих тем)
    setMarkerBackgroundColor(QColor(255, 0, 0), BREAKPOINT_MARKER);
    setMarkerBackgroundColor(QColor(128, 128, 128), BREAKPOINT_DISABLED_MARKER);
}

QImage CodeEditor::createChevronImage(bool right, const QColor &color) {
    // Создаем изображение chevron (две палки, образующие угол)
    // Размер изображения: 12x12 пикселей
    const int size = 12;
    const int margin = 1;  // Отступ от краев
    QImage image(size, size, QImage::Format_ARGB32);
    image.fill(Qt::transparent);  // Прозрачный фон
    
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Толщина линий
    QPen pen(color, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    
    if (right) {
        // Chevron вправо (>): две линии, образующие широкий угол, направленный вправо
        // Точка схода (вершина угла) справа
        int tipX = size - margin;
        int tipY = size / 2;  // Центр по вертикали
        // Чтобы увеличить угол, сдвигаем начальные точки ПРАВЕЕ (ближе к точке схода)
        int startX = size / 2;  // Сдвигаем начальные точки еще правее для большего угла
        // Верхняя линия: от точки слева к точке схода справа
        painter.drawLine(startX, margin, tipX, tipY);
        // Нижняя линия: от точки слева к точке схода справа
        painter.drawLine(startX, size - margin, tipX, tipY);
    } else {
        // Chevron вниз (⌄): две линии, образующие широкий угол, направленный вниз
        // Точка схода (вершина угла) снизу
        int tipX = size / 2;  // Центр по горизонтали
        int tipY = size - margin;
        // Чтобы увеличить угол, опускаем начальные точки НИЖЕ (ближе к точке схода)
        int startY = size / 2;  // Опускаем начальные точки еще ниже для большего угла
        // Левая линия: от точки сверху к точке схода снизу
        painter.drawLine(margin, startY, tipX, tipY);
        // Правая линия: от точки сверху к точке схода снизу
        painter.drawLine(size - margin, startY, tipX, tipY);
    }
    
    return image;
}
