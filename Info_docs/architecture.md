# Архитектура

## Слои IDE (`Vuzhyk.exe`)

1. **Окно** — frameless `QMainWindow` (`MainWindow`) + `TitleBar` + `WindowFrameOverlay`. Меню: `HoverActivatedMenuBar` + `AnimatedMenu`.
2. **Редактор** — вкладки `CodeEditor` (`QsciScintilla`), лексер Python/HTML, completer, маркеры ошибок и breakpoint.
3. **Исполнение** — `QProcess` с выбранным `python.exe`: запуск, отладка (подсветка строки, F9 продолжить, Shift+F5 стоп), REPL/консоль (`ConsoleWidget`), `input()` через IPC с процессом.
4. **Учебные вкладки** — редактор задач pyrob (`PyrobEditorWidget`), задачник черепахи (`TurtleTaskWidget` + `TurtleCanvasHost` + `turtle_task_runner.py`), справка (`HelpWidget`), змейка (`SnakeGame`).
5. **Настройки** — `SettingsWidget`: интерпретатор, пакеты (`PackageManagerWidget`), горячие клавиши, расширения, синхронизация, агент.
6. **Агент** — док `AgentPanel`: чат ASK/CODE, провайдеры LM Studio (HTTP) и OpenRouter (HTTPS WinHTTP).
7. **Синхронизация** — в процессе IDE только клиент и управление сервером; HTTP-сервер — другой exe.

Один экземпляр приложения: `QLocalServer` / `QLocalSocket` в `main.cpp`. Файлы с командной строки (в т.ч. Unicode на Windows через `CommandLineToArgvW`) открываются во вкладках.

## Запуск Python

`PythonInterpreterPaths` выбирает `site-packages`:

- встроенный: `{app}/Python38/Lib/site-packages`;
- «управляемый» (есть `Lib/` у интерпретатора или путь под `{app}/interpreters/`) — всегда `{dir python.exe}/Lib/site-packages`;
- иначе запасной вызов `site.getsitepackages()`.

pyrob считается установленным, если в пакете есть `api.py` или `__init__.py`. Задачи: `…/pyrob/tasks`.

Черепаха в задачнике идёт через `turtlexy_fvt` (протокол холста JSON, виджет `TurtleCanvasHost`). Скрипт `src/tasks/turtle_task_runner.py` вшит в `resources.qrc` (`:/tasks/turtle_task_runner.py`) и копируется в выход как `tasks/` при VS post-build.

## Агент

```
MainWindow
  context  → AgentEdit::buildNumberedFileContext (нумерованные строки файла)
  apply    → AgentEdit::parseOperations + applyOperations
AgentPanel → LmStudioClient
               HTTP  QNetworkAccessManager  → LM Studio 127.0.0.1:1234/v1
               HTTPS AgentHttps (WinHTTP)   → openrouter.ai
```

Режим CODE: модель обязана (по системному промпту) выдавать блоки `<<<VUZHYK_EDIT` … `<<<END_EDIT`. Операции: замена файла, замена/вставка/удаление строк. Если блоков нет — fallback на markdown fence. ASK — только ответ, без применения правок.

Промпты зашиты в `AgentPrompts.cpp` (адаптация идей OpenCode, не зависимость от папки `opencode-dev` в рантайме).

У ученика при синхронизации агент может быть запрещён учителем (`allowAgent` в `/api/v1/status` → `sync/cachedStudentAgentAllowed`).

## Синхронизация библиотек

**Учитель:** `LibrarySyncServerControl` пишет ini, ставит автозагрузку HKCU, запускает `VuzhykSyncServer.exe`. Сервер (`sync-server/sync_server.c`) — WinAPI, трей, Winsock, GET:

- `/api/v1/status` — `{ ok, allowAgent }`
- `/api/v1/packages` — имена пакетов из site-packages (нормализация: lower case, `-`→`_`)
- `/api/v1/package?name=` — zip через 7za, затем удаление временного архива

**Ученик:** `LibrarySyncManager` по таймеру сравнивает `pip list` с учителем, качает недостающее, распаковывает 7za в свой site-packages. Пароль вкладки настроек — DPAPI (`LibrarySyncCrypto`).

## Расширения

`ExtensionManager` сканирует `{exe}/extensions/*/extension.json` (id, exe, типы файлов, пункт меню). Включённость — `extensions/{id}/enabled`. Браузер HTML — отдельный exe с Qt WebEngine, не внутри `Vuzhyk.exe` (чтобы не тащить WebEngine в минимальную установку).

## Демо

Каждый `Demo*.exe` — своё `main` в `demo/scenes/`. Общее: курсор (`DemoCursor`), масштаб UI ×1.5 (`DemoScale.h`), рекордер H.264 (`DemoRecorder` + `minih264e`). Захват окна → RGB → planar YUV 4:2:0 (yuv420p) → MP4 рядом с exe. Частота **60 fps**. Очередь кадров в encode-потоке.

## Сборка VS vs CMake

`Vuzhyk.vcxproj` — полный список модулей (включая `PackageManagerWidget`, `HoverActivatedMenuBar`, `TurtleTaskWidget`). `CMakeLists.txt` на момент этой записи **не включает** turtle-задачник, PackageManager и HoverActivatedMenuBar. Для продукта править VS-проект обязательно.
