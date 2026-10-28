#pragma once

#include <QString>
#include <QVector>

namespace AgentEdit {

enum class Type {
    ReplaceEntireFile,
    ReplaceLines,
    InsertAfter,
    DeleteLines
};

struct Operation {
    Type type { Type::ReplaceEntireFile };
    int startLine { 0 }; // 1-based inclusive (replace/delete)
    int endLine { 0 };   // 1-based inclusive
    int afterLine { 0 }; // 1-based (insert after)
    QString language;
    QString content;
    bool fromProtocol { false }; // true = блок <<<VUZHYK_EDIT
};

/** Собирает контекст открытого файла с номерами строк (1-based). */
QString buildNumberedFileContext(const QString &path, const QString &content, int maxChars = 24000);

/** Парсит операции из ответа модели (блоки <<<VUZHYK_EDIT ... <<<END_EDIT).
 *  Если их нет — fallback: первый/лучший markdown code fence → replace_file. */
QVector<Operation> parseOperations(const QString &response);

/** Применяет операции к тексту файла. lines 1-based. Возвращает новый текст или пустую строку при ошибке. */
QString applyOperations(const QString &originalText, const QVector<Operation> &ops, QString *errorMessage = nullptr);

} // namespace AgentEdit
