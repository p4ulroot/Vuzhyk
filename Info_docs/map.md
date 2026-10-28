# Карта проекта

Vuzhyk — настольная IDE для Python (школьная информатика, РБ). Один solution Visual Studio, несколько exe.

## Процессы в установленной IDE

```
Vuzhyk.exe              главное окно, редактор, запуск скриптов, агент, синхронизация (клиент)
  ├─ FilePicker.exe     нативные диалоги Windows (отдельный процесс)
  ├─ Python38/python.exe        встроенный интерпретатор
  ├─ interpreters/*/python.exe  опциональные скачанные версии
  ├─ VuzhykSyncServer.exe       HTTP-сервер учителя (если роль «учитель»)
  └─ extensions/*/…exe          например EmbeddedBrowserExtension.exe
```

Демо-сцены — **отдельные exe**, не плагины IDE. Собираются конфигурацией `Demo|x64`.

## Деревья исходников (рабочие)

| Путь | Роль |
|------|------|
| `src/` | Код IDE |
| `sync-server/` | Сервер синхронизации на C (WinAPI) |
| `embedded-browser/` | Расширение «встроенный браузер» (Qt WebEngine) |
| `demo/` | Автоматические демо-окна + запись MP4 |
| `help/` | HTML справки 6–7 класс (грузит `HelpWidget`) |
| `icons/` | SVG/ICO, часть вшита в `resources.qrc` |
| `installer/` | Inno Setup |
| `cmake/` | Вспомогательные cmake-скрипты (вторичная сборка) |
| `scripts/` | `copy-openssl.bat` для TLS |
| `tools/zip/` | `7za.exe` |
| `tools/pdf/` | SumatraPDF + PDF учебников |
| `turtlexy-1.0.0/`, `turtlexy_fvt/` | Исходники учебных библиотек черепахи |
| `dlldepend/` | Готовые DLL QScintilla / QtWebChannel для линковки |
| `Python38/` | Бандл CPython 3.8 (не править как «наш код») |
| `Info_docs/` | Эта база знаний |
| `project_doc/` | Заявка на грант, статья, презентация — не код IDE |

## Solution `Vuzhyk.sln`

Продукт: `Vuzhyk`, `QScintilla`, `FilePicker`, `EmbeddedBrowserExtension`, `VuzhykSyncServer`.

Демо (только `Demo|x64`): двенадцать проектов `DemoTurtleHouse` … `DemoTurtleHouseDark`, выходные имена с суффиксом 1…12.

## Конфигурации

| Конфиг | Что собирается |
|--------|----------------|
| `Release\|x64` | IDE и сопутствующие exe в `x64/Release/Vuzhyk/` |
| `Debug\|x64` | То же в `x64/Debug/Vuzhyk/` (у Vuzhyk Debug часто Release CRT/Qt + символы) |
| `Demo\|x64` | Только демо-проекты → `x64/Demo/Demo/` |

## Где что ищет IDE рядом с exe

- `Python38/` — встроенный Python
- `interpreters/` — дополнительные интерпретаторы
- `help/` — HTML справка
- `tools/pdf/` — учебники и SumatraPDF
- `tools/zip/7za.exe` — zip для синхронизации и распаковки `.7z`
- `extensions/` — `extension.json` + exe расширений
- `Vuzhyk_debug.log` — лог (создаётся в рантайме)

Настройки: реестр/`QSettings` org=`Vuzhyk`. Сервер синхронизации: `%APPDATA%\Vuzhyk\sync-server.ini`.
