# Назначение файлов и каталогов

Деревья с тысячами чужих файлов описаны **целиком**, без перечисления stdlib. Мусор вынесен в [junk.md](junk.md).

## Корень репозитория

| Путь | Назначение |
|------|------------|
| `AGENTS.md` | Вход для coding-агентов |
| `Info_docs/` | База знаний (эта документация) |
| `Vuzhyk.sln` | Solution VS: IDE, QScintilla, FilePicker, браузер, sync-server, демо |
| `Vuzhyk.vcxproj` (+ `.filters`) | Проект главного exe |
| `QScintilla.vcxproj` | Сборка DLL редактора из `C:\QScintilla` |
| `FilePicker.vcxproj` | Нативные диалоги |
| `VuzhykSyncServer.vcxproj` | Сервер синхронизации |
| `Demo*.vcxproj` (+ `.filters`) | Двенадцать демо-exe |
| `CMakeLists.txt` | Альтернативная сборка (неполная относительно VS) |
| `resources.qrc` | Иконки + `turtle_task_runner.py` |
| `app.rc` | Иконка `Vuzhyk.exe` |
| `.gitignore` | Игнор сборок, логов, бандлов |
| `project_doc/` | Материалы заявки/статьи/презентации (не код) |

## `src/` — исходники IDE

| Путь | Назначение |
|------|------------|
| `main.cpp` | Entry, лог, single-instance |
| `MainWindow.*` | Главное окно |
| `CodeEditor.*` | Редактор QScintilla |
| `ConsoleWidget.*` | Консоль Python |
| `SettingsWidget.*` | Настройки |
| `PackageManagerWidget.*` | pip UI |
| `HelpWidget.*` | Справка |
| `PythonInterpreterPaths.*` | site-packages / pyrob |
| `TitleBar.*` | Кастомный заголовок |
| `WindowFrameOverlay.*` | Рамка окна |
| `AnimatedMenu.*` | Анимация меню |
| `HoverActivatedMenuBar.*` | Menubar как в IDE |
| `filepicker.cpp` | Код FilePicker.exe |
| `agent/*` | Встроенный ИИ-агент |
| `sync/*` | Клиент и управление сервером синхронизации |
| `pyrobeditor/*` | Редактор задач Робота |
| `tasks/TurtleTaskWidget.*` | Задачник черепахи |
| `tasks/TurtleCanvasHost.*` | Холст FVT |
| `tasks/turtle_task_runner.py` | Python-сессия черепахи |
| `extensions/ExtensionManager.*` | Загрузка расширений |
| `sea/SnakeGame.*` | Змейка |

## `sync-server/`

| Путь | Назначение |
|------|------------|
| `sync_server.c` | Единственный исходник `VuzhykSyncServer.exe` |

## `embedded-browser/`

| Путь | Назначение |
|------|------------|
| `src/main.cpp`, `BrowserWindow.*` | Окно Qt WebEngine |
| `EmbeddedBrowserExtension.vcxproj` | Сборка в `extensions/EmbeddedBrowserExtension/` |

## `demo/`

| Путь | Назначение |
|------|------------|
| `DemoRecorder.*`, `DemoH264Impl.c` | Запись MP4 H.264 |
| `third_party/minih264e.h`, `h264_config.h` | Кодировщик |
| `DemoCursor.*` | Анимированный курсор |
| `DemoScale.h` | Масштаб UI ×1.5 |
| `DemoIcons.h` | Иконки демо-хрома |
| `demo.qrc`, `app.rc` | Ресурсы демо |
| `DemoIdeWindow.*` | Полноэкранный фейковый IDE: дом / задачи черепахи |
| `DemoEditorWindow.*` | Hello / редактор |
| `DemoHouseField.*`, `DemoRobotField.*` | Холсты 800×800 |
| `DemoRobotIdeWindow.*`, `DemoRobotTask.*`, `DemoRobotCanvas.*` | Сцена pyrob |
| `DemoTurtleCanvas.*`, `DemoTurtleTask.*` | Черепаха в демо |
| `DemoHelpWindow.*` | Справка |
| `DemoThemeWindow.*` | Смена темы |
| `DemoRunWindow.*` | Запуск скрипта |
| `DemoAgentWindow.*` | Панель агента (имитация) |
| `DemoStepWindow.*` | Автодополнение и шаг отладки |
| `scenes/*_main.cpp` | `main` каждого exe |

Соответствие exe (TargetName):

1. `DemoTurtleHouse1` ← `scenes/house_main.cpp`
2. `DemoHello2` ← `hello_main.cpp`
3. `DemoHouseField3` ← `house_field_main.cpp`
4. `DemoRobotField4` ← `robot_field_main.cpp`
5. `DemoRobotTask5` ← `robot_task_main.cpp`
6. `DemoTurtleTasks6` ← `turtle_tasks_main.cpp`
7. `DemoHelp7` ← `help_main.cpp`
8. `DemoTheme8` ← `theme_main.cpp`
9. `DemoRun9` ← `run_main.cpp`
10. `DemoAgent10` ← `agent_main.cpp`
11. `DemoStep11` ← `step_main.cpp`
12. `DemoTurtleHouseDark12` ← `house_dark_main.cpp`

## `help/`

HTML разделов справки 6 и 7 класса. Читает `HelpWidget`. **Не удалять** как «старую документацию».

## `icons/`

| Путь | Назначение |
|------|------------|
| `logo/vuzhyk.ico`, `logo/py.ico` | Иконка приложения и `.py` |
| `light-theme/*.svg` | Тулбар (в qrc) |
| `window-controls/*.svg` | Кнопки окна |
| Часть svg (`old_bug.svg`, бэкап `file-save-all.svg.2025_…`) | См. junk |

## `installer/`

| Путь | Назначение |
|------|------------|
| `Vuzhyk.iss` | Inno Setup: версия продукта (единственное место), компоненты core/browser, пакует `help/`, `tasks/`, `tools/`, `Python38/` |
| `build-installer.bat` | В ВМ: `windeployqt` → жёсткие проверки сборки → ISCC → копия setup в `vmshared\Installer` |
| `output/` | Артефакт установщика (gitignore) |

## `cmake/`

| Путь | Назначение |
|------|------------|
| `BundlePython.cmake` | Копирование Python в runtime при CMake-сборке |
| `CopyQtWebEngineContents.cmake` | Ресурсы WebEngine для расширения |

## `scripts/`

| Путь | Назначение |
|------|------------|
| `copy-openssl.bat` | Post-build: libcrypto/libssl к выходу |

## `tools/`

| Путь | Назначение |
|------|------------|
| `zip/7za.exe` | 7-Zip CLI |
| `pdf/*.pdf` | Учебники информатики 6–7 |
| `pdf/*_python.txt` | Текстовые выжимки учебников (поиск/справка) |
| `pdf/` SumatraPDF и лицензии | Просмотр PDF из IDE |
| `requirements-docx.txt` | pip-зависимости для скриптов разбора Word (не рантайм IDE) |

## `turtlexy-1.0.0/`, `turtlexy_fvt/`

Исходники pip-пакетов учебной черепахи (setup.py, пакет). Рабочие копии также лежат в `Python38/Lib/site-packages/`. Корневые папки — для разработки/паковки пакета, не для линковки C++.

## `dlldepend/`

Предсобранные `qscintilla2_qt5.dll` / `.lib` и сопутствующие Qt-зависимости, на которые ссылается CMake/VS.

## `Python38/`

Полный бандл интерпретатора + site-packages (pyrob, turtlexy, pip, numpy, matplotlib и т.д.). **Не рефакторить** как код Vuzhyk.

## `extensions/`

Выход сборки расширений (`EmbeddedBrowserExtension/`). В `.gitignore` как `/extensions/`. Манифест `extension.json` создаётся/копируется при сборке.

## `QtMsBuild/`

Интеграция Qt с MSBuild (локально у разработчика). Без неё `.vcxproj` не собираются.

## `project_doc/`

Заявка Минобразования, брошюра, презентация, статья СТНО-2026, скрипт заполнения формы. К компиляции IDE не относится.

## Сборка / IDE (не исходники)

`x64/`, `.qt/`, `.vs/`, `*.vcxproj.user` — артефакты. Игнорируются git.
