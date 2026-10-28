#include "AgentEdit.h"

#include <QRegularExpression>
#include <QStringList>
#include <algorithm>

namespace AgentEdit {

namespace {

QStringList splitLines(const QString &text)
{
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    if (normalized.isEmpty()) {
        return {};
    }
    // Сохраняем завершающий пустой элемент только если файл кончается \n — для редактирования
    // удобнее работать без фиктивной пустой строки в конце.
    QStringList lines = normalized.split(QLatin1Char('\n'));
    if (!lines.isEmpty() && lines.last().isEmpty() && normalized.endsWith(QLatin1Char('\n'))) {
        lines.removeLast();
    }
    return lines;
}

QString joinLines(const QStringList &lines)
{
    if (lines.isEmpty()) {
        return QStringLiteral("");
    }
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

QString headerValue(const QString &headers, const QString &key)
{
    const QRegularExpression re(
        QStringLiteral("(?im)^\\s*%1\\s*:\\s*(.+)\\s*$").arg(QRegularExpression::escape(key)));
    const QRegularExpressionMatch m = re.match(headers);
    return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

int headerInt(const QString &headers, const QString &key, int defaultValue = 0)
{
    const QString v = headerValue(headers, key);
    bool ok = false;
    const int n = v.toInt(&ok);
    return ok ? n : defaultValue;
}

Operation operationFromHeaders(const QString &headers, const QString &body)
{
    Operation op;
    op.fromProtocol = true;
    op.content = body;
    op.language = headerValue(headers, QStringLiteral("language")).toLower();

    const QString type = headerValue(headers, QStringLiteral("type")).toLower();
    if (type == QLatin1String("replace_lines") || type == QLatin1String("replace-lines")) {
        op.type = Type::ReplaceLines;
        op.startLine = headerInt(headers, QStringLiteral("start"));
        op.endLine = headerInt(headers, QStringLiteral("end"), op.startLine);
    } else if (type == QLatin1String("insert_after") || type == QLatin1String("insert-after")) {
        op.type = Type::InsertAfter;
        op.afterLine = headerInt(headers, QStringLiteral("line"));
        if (op.afterLine <= 0) {
            op.afterLine = headerInt(headers, QStringLiteral("after"));
        }
    } else if (type == QLatin1String("delete_lines") || type == QLatin1String("delete-lines")) {
        op.type = Type::DeleteLines;
        op.startLine = headerInt(headers, QStringLiteral("start"));
        op.endLine = headerInt(headers, QStringLiteral("end"), op.startLine);
    } else {
        op.type = Type::ReplaceEntireFile;
    }
    return op;
}

} // namespace

QString buildNumberedFileContext(const QString &path, const QString &content, int maxChars)
{
    const QStringList lines = splitLines(content);
    const int total = lines.size();

    QString out;
    out += QStringLiteral("Active editor tab\n");
    out += QStringLiteral("path: %1\n").arg(path);
    out += QStringLiteral("total_lines: %1\n").arg(total);
    out += QStringLiteral("Line numbers below are 1-based.\n");
    if (total > 0) {
        out += QStringLiteral(
            "Prefer replace_lines / insert_after / delete_lines for local edits "
            "(comments, renames, small fixes). Use replace_file only when most of the file must change.\n");
    } else {
        out += QStringLiteral("File is empty: replace_file is appropriate to create new content.\n");
    }
    out += QStringLiteral("----- BEGIN FILE -----\n");

    // Если файл большой — показываем начало и конец с пометкой пропуска
    const int maxLinesBudget = qMax(40, maxChars / 40);
    if (total <= maxLinesBudget) {
        for (int i = 0; i < total; ++i) {
            out += QStringLiteral("%1|%2\n").arg(i + 1, 6).arg(lines.at(i));
        }
    } else {
        const int head = maxLinesBudget * 2 / 3;
        const int tail = maxLinesBudget - head;
        for (int i = 0; i < head; ++i) {
            out += QStringLiteral("%1|%2\n").arg(i + 1, 6).arg(lines.at(i));
        }
        out += QStringLiteral("   ... (%1 lines omitted) ...\n").arg(total - head - tail);
        for (int i = total - tail; i < total; ++i) {
            out += QStringLiteral("%1|%2\n").arg(i + 1, 6).arg(lines.at(i));
        }
        out += QStringLiteral(
            "NOTE: Middle of file omitted due to size. Prefer replace_lines for local edits, "
            "or replace_file only if you rewrite the whole file.\n");
    }

    out += QStringLiteral("----- END FILE -----\n");
    return out;
}

QVector<Operation> parseOperations(const QString &response)
{
    QVector<Operation> ops;

    // <<<VUZHYK_EDIT\nheaders\n>>>\nbody\n<<<END_EDIT
    QRegularExpression blockRe(
        QStringLiteral("<<<VUZHYK_EDIT\\s*\\n([\\s\\S]*?)>>>\\s*\\n([\\s\\S]*?)<<<END_EDIT"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = blockRe.globalMatch(response);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        ops.append(operationFromHeaders(m.captured(1), m.captured(2)));
    }

    if (!ops.isEmpty()) {
        return ops;
    }

    // Fallback: markdown fence → replace_file
    QRegularExpression fence(QStringLiteral("```([^\\n`]*)\\n([\\s\\S]*?)```"));
    QRegularExpressionMatchIterator fit = fence.globalMatch(response);
    QVector<Operation> fences;
    while (fit.hasNext()) {
        const QRegularExpressionMatch m = fit.next();
        const QString lang = m.captured(1).trimmed().toLower();
        if (lang == QLatin1String("vuzhyk-edit")) {
            continue;
        }
        Operation op;
        op.type = Type::ReplaceEntireFile;
        op.fromProtocol = false;
        op.language = lang;
        op.content = m.captured(2);
        if (op.content.endsWith(QLatin1Char('\n'))) {
            op.content.chop(1);
        }
        if (!op.content.trimmed().isEmpty()) {
            fences.append(op);
        }
    }

    if (fences.isEmpty()) {
        return {};
    }

    int best = 0;
    for (int i = 0; i < fences.size(); ++i) {
        if (fences.at(i).language == QLatin1String("python")
            || fences.at(i).language == QLatin1String("py")) {
            best = i;
            break;
        }
        if (fences.at(i).content.size() > fences.at(best).content.size()) {
            best = i;
        }
    }
    return { fences.at(best) };
}

QString applyOperations(const QString &originalText, const QVector<Operation> &ops, QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (ops.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No edit operations found.");
        }
        return {};
    }

    QStringList lines = splitLines(originalText);

    for (const Operation &op : ops) {
        switch (op.type) {
        case Type::ReplaceEntireFile: {
            lines = splitLines(op.content);
            break;
        }
        case Type::ReplaceLines: {
            if (op.startLine < 1 || op.endLine < op.startLine) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Invalid replace_lines range: %1-%2")
                                       .arg(op.startLine)
                                       .arg(op.endLine);
                }
                return {};
            }
            if (lines.isEmpty() && op.startLine == 1 && op.endLine == 1) {
                lines = splitLines(op.content);
                break;
            }
            if (op.startLine > lines.size()) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("replace_lines start %1 beyond file (%2 lines)")
                                       .arg(op.startLine)
                                       .arg(lines.size());
                }
                return {};
            }
            const int end = qMin(op.endLine, lines.size());
            const int startIdx = op.startLine - 1;
            const int count = end - op.startLine + 1;
            const QStringList newLines = splitLines(op.content);
            for (int i = 0; i < count; ++i) {
                lines.removeAt(startIdx);
            }
            for (int i = 0; i < newLines.size(); ++i) {
                lines.insert(startIdx + i, newLines.at(i));
            }
            break;
        }
        case Type::InsertAfter: {
            if (op.afterLine < 0 || op.afterLine > lines.size()) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Invalid insert_after line: %1 (file has %2 lines)")
                                       .arg(op.afterLine)
                                       .arg(lines.size());
                }
                return {};
            }
            const QStringList newLines = splitLines(op.content);
            const int insertAt = op.afterLine; // after line N → index N (0-based after N-1)
            for (int i = 0; i < newLines.size(); ++i) {
                lines.insert(insertAt + i, newLines.at(i));
            }
            break;
        }
        case Type::DeleteLines: {
            if (op.startLine < 1 || op.endLine < op.startLine || op.startLine > lines.size()) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Invalid delete_lines range: %1-%2")
                                       .arg(op.startLine)
                                       .arg(op.endLine);
                }
                return {};
            }
            const int end = qMin(op.endLine, lines.size());
            const int startIdx = op.startLine - 1;
            const int count = end - op.startLine + 1;
            for (int i = 0; i < count; ++i) {
                lines.removeAt(startIdx);
            }
            break;
        }
        }
    }

    return joinLines(lines);
}

} // namespace AgentEdit
