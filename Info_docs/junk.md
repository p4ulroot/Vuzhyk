# Мусор и не-продукт

Сюда вынесено то, что **лежит в дереве**, но не является актуальной частью Vuzhyk IDE. Не копировать отсюда код «как из основного проекта». Не удалять автоматически без просьбы пользователя (кроме явно указанных ниже устаревших md — их сменила эта база).

## Целые деревья, которые не входят в текущий продукт

| Путь | Почему мусор / архив |
|------|----------------------|
| `OldVuzhyk/` | Старая копия IDE (свой sln, src, иконки). Текущий код — корневые `src/` и `Vuzhyk.sln`. |
| `opencode-dev/` | Чужой репозиторий OpenCode. Сборка Vuzhyk его не использует. Промпты агента уже в `src/agent/AgentPrompts.cpp`. |

## Дубликаты и бэкапы

| Путь | Почему |
|------|--------|
| `Python38/Lib/site-packages/turtlexy*` vs `turtlexy-1.0.0/`, `turtlexy_fvt/` | Одни и те же пакеты: бандл и исходники для разработки. Не синхронизировать вручную «на глаз» без задачи. |
| `icons/light-theme/file-save-all.svg.2025_11_02_04_01_37.0.svg` | Автобэкап SVG, не подключён в `resources.qrc`. |
| `icons/light-theme/old_bug.svg` | Старая иконка; в qrc используются `bug.svg` / `next-bug.svg`. |
| `icons/light-theme/bug-next.svg` | Дубль имени рядом с `next-bug.svg`; в qrc — `next-bug.svg`. |

## Служебные скрипты не IDE

Разовые скрипты разбора Word (`extract_doc.py`) и текстовые дампы `project_doc/**/_dump_*.txt` удалены при вводе `Info_docs/`.

`tools/requirements-docx.txt` — pip для python-docx, не ставится ученику. Оставлен как вспомогательный файл для заявки.

## Устаревшая документация (заменена `Info_docs/` + `AGENTS.md`)

Эти файлы **удаляются** при вводе этой базы, потому что они дублировали/устаревали описание проекта и не используются кодом IDE:

- корневые `info.md`, `chathistory.md`, старый подробный `README.md`
- `BUILD_QSCINTILLA.md`
- `PYTHON_INSTALLERS_README.md`, `PYTHON_INSTALLERS_SETUP.md`, `PYTHON_INSTALLERS_LICENSE.md`, `PYTHON_INSTALLERS_GITHUB_ACTIONS.yml`
- `OldVuzhyk/README.md`

**Не мусор (не удалять):** `help/*.html`, `tools/pdf/*` учебники, `src/agent/AgentPrompts.*`, README внутри pip-пакетов `turtlexy*`, материалы `project_doc` (статья, брошюра, презентация, форма гранта).

## Артефакты сборки

`x64/`, `.vs/`, `.qt/`, `installer/output/`, `*.obj`, логи `Vuzhyk_debug.log` — не исходники.
