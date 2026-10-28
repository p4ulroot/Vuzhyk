# Важные модули

## `src/main.cpp`

Точка входа. Лог `Vuzhyk_debug.log`, single-instance, тема из argv/QSettings, разбор командной строки Windows, создание `MainWindow`.

## `src/MainWindow.*`

Центр: вкладки редактора, дерево проекта, доки вывода/консоли, запуск и отладка Python, меню, статусбар, вкладки настроек/справки/pyrob/черепахи/змеи, док агента, индикатор синхронизации. Связывает все модули ниже. Горячие клавиши глобально (в т.ч. F5/F9). Открытие файлов через FilePicker. Расширения подмешивают пункты меню.

## `src/CodeEditor.*`

Обёртка QScintilla: подсветка Python, completer ( builtins + слова файла + имена задач pyrob ), fold, скобки, размер шрифта, маркеры ошибки и breakpoint, темы.

## `src/ConsoleWidget.*`

Интерактивный Python в `QProcess`, история команд, вид «терминала».

## `src/SettingsWidget.*` + `src/PackageManagerWidget.*`

Страницы настроек. Интерпретаторы (встроенный / список / системный / свой путь), скачивание `.7z`, ассоциации `.py`, таблица shortcuts, таблица расширений, pip UI, синхронизация, агент (URL LM Studio, ключи OpenRouter).

## `src/PythonInterpreterPaths.*`

Единственное место, где считается site-packages и «установлен ли pyrob».

## `src/HelpWidget.*`

Дерево разделов + `QTextBrowser`. Встроенные тексты и файлы `help/grade6_*.html`, `help/grade7_*.html`. Подсветка блоков кода. PDF учебников открываются снаружи (SumatraPDF).

## `src/TitleBar.*`, `WindowFrameOverlay.*`, `AnimatedMenu.*`, `HoverActivatedMenuBar.*`

Хром окна: перетаскивание, min/max/close, шестерёнка настроек, пасхалка по кликам на иконку, рамка, анимация меню, поведение menubar как в IDE (клик затем hover).

## `src/filepicker.cpp`

Отдельная программа FilePicker: `-e` фильтр, `-s` сохранение, `--dir` папка.

## `src/agent/`

| Файл | Роль |
|------|------|
| `AgentPanel` | UI чата, режимы ASK/CODE, выбор модели и провайдера |
| `AgentPrompts` | Системные промпты |
| `AgentEdit` | Парсинг и применение правок к буферу редактора |
| `LmStudioClient` | `/v1/models`, streaming chat |
| `AgentHttps` | WinHTTP HTTPS |
| `AgentOpenRouterKeys` | Имена ключей и список моделей в QSettings |

## `src/sync/`

| Файл | Роль |
|------|------|
| `LibrarySyncManager` | Ученик: таймер, diff пакетов, скачивание, 7za, кэш allowAgent |
| `LibrarySyncServerControl` | Учитель: ini, Run, start/stop exe |
| `LibrarySyncCrypto` | DPAPI пароля вкладки |

## `src/pyrobeditor/`

Визуальный редактор поля Робота: `ProjectModel` (клетки, стены, старт, варианты, JSON `.pyrob.json`, генерация Python), `GridEditor` (рисование), `PyrobEditorWidget` (тулбар, сохранение, экспорт).

## `src/tasks/`

Задачник черепахи: каталог заданий, редактор + холст FVT, run/step/reset. `turtle_task_runner.py` — долгоживущая сессия Python.

## `src/extensions/ExtensionManager.*`

Поиск `extensions/*/extension.json`, запуск exe, persist enabled.

## `src/sea/SnakeGame.*`

Мини-игра, вкладка «Инструменты». Не учебный модуль информатики, пасхалка/разгрузка.

## Другие exe

- **`sync-server/sync_server.c`** — учительский HTTP + трей.
- **`embedded-browser/src/*`** — окно WebEngine для HTML.
- **`demo/*`** — не модули IDE; имитация chrome и сценариев для записи. Общие: `DemoRecorder`, `DemoCursor`, `DemoScale`, `DemoIcons`. Сцены: `DemoIdeWindow` (черепаха/задачи), `DemoEditorWindow` (hello), `DemoHouseField` / `DemoRobotField` (холсты 800×800), `DemoRobotIdeWindow`, `DemoHelpWindow`, `DemoThemeWindow`, `DemoRunWindow`, `DemoAgentWindow`, `DemoStepWindow`.

## QSettings (кратко)

Группы: `theme`, `geometry`/`windowState`, `editor/fontSize`, `interface/*`, `shortcuts/*`, `runtime/*`, `associations/python`, `sync/*`, `agent/*`, `extensions/{id}/enabled`.
