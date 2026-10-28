#pragma once

#include <QString>

namespace AgentPrompts {

enum class Mode {
    Ask,
    Code
};

/** Базовый системный промпт (адаптирован из OpenCode default.txt). */
QString baseSystemPrompt();

/** Напоминание режима ASK (адаптировано из OpenCode plan.txt). */
QString askModeReminder();

/** Собирает полный system prompt для выбранного режима. */
QString systemPromptForMode(Mode mode);

} // namespace AgentPrompts
