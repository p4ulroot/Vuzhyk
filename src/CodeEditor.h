#pragma once

#include <Qsci/qsciscintilla.h>
#include <Qsci/qscilexerpython.h>
#include <Qsci/qscilexerhtml.h>
#include <QCompleter>
#include <QWidget>
#include <QSet>
#include <QPointer>
#include <QStringListModel>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QPaintEvent>
#include <QImage>
#include <QKeySequence>

class CodeEditor : public QsciScintilla {
    Q_OBJECT
public:
    explicit CodeEditor(QWidget *parent = nullptr);

    void setCompleter(QCompleter *completer);
    QCompleter *completer() const { return m_completer; }
    
    void setTheme(const QString &theme);
    void setFileExtension(const QString &extension);
    void setCommentShortcut(const QKeySequence &sequence);
    void setUncommentShortcut(const QKeySequence &sequence);
    
    void highlightErrorLines(const QList<int> &lineNumbers);
    void clearErrorHighlight();
    
    // Обновление API для автокомплита
    void updateAPIs(const QStringList &additionalWords = QStringList());
    
    // Точки останова
    QSet<int> breakpoints() const { return m_breakpoints; }
    void toggleBreakpoint(int lineNumber);
    void setBreakpoint(int lineNumber, bool enabled);
    bool hasBreakpoint(int lineNumber) const;
    
    // Совместимость с QPlainTextEdit API
    QTextDocument *document() const;
    QString toPlainText() const;
    void setPlainText(const QString &text);
    bool isModified() const;
    void setModified(bool modified);
    QTextCursor textCursor() const;
    void setTextCursor(const QTextCursor &cursor);
    void centerCursor();

signals:
    void fontSizeChanged(int size);
    void breakpointToggled(int lineNumber, bool enabled);

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void focusInEvent(QFocusEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *e) override;

private slots:
    void insertCompletion(const QString &completion);
    void onMarginClicked(int margin, int line, Qt::KeyboardModifiers state);
    void onTextChanged();

private:
    void setupLexer();
    void setupMargins();
    void setupIndicators();
    void applyTheme(const QString &theme);
    void updateMarginWidths(); // Обновление ширины колонок при изменении размера шрифта
    void updateBreakpointMarkers(); // Обновление размера маркеров брейкпоинтов при изменении размера шрифта
    void commentSelectedLines();
    void uncommentSelectedLines();
    bool keyEventMatchesShortcut(const QKeyEvent *event, const QKeySequence &shortcut) const;
    
    // Создание изображения chevron (две палки, образующие угол)
    QImage createChevronImage(bool right, const QColor &color = QColor(0, 0, 0));  // true = вправо (>), false = вниз (⌄)
    
    QCompleter *m_completer { nullptr };
    QsciLexer *m_lexer { nullptr };
    QString m_currentLanguage { "python" };
    QString m_theme { "light" };
    QList<int> m_errorLineNumbers;
    QSet<int> m_breakpoints;
    QTextDocument *m_dummyDocument { nullptr }; // Для совместимости с document()
    int m_hoverBreakpointLine { -1 }; // Строка, где показывается предпросмотр брейкпоинта
    int m_currentFontSize { 10 }; // Текущий размер шрифта для отслеживания изменений
    QKeySequence m_commentShortcut { QStringLiteral("Alt+3") };
    QKeySequence m_uncommentShortcut { QStringLiteral("Alt+Ctrl+3") };
    
    // Индикаторы для ошибок и точек останова
    static const int ERROR_INDICATOR = 0;
    static const int ERROR_MARKER = 4; // Маркер для выделения строк с ошибками красным фоном
    static const int BREAKPOINT_MARKER = 1;
    static const int BREAKPOINT_DISABLED_MARKER = 2;
    static const int BREAKPOINT_HOVER_MARKER = 3; // Маркер для предпросмотра при наведении
};
