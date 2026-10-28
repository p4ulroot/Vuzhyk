# Ключевые зависимости

## Обязательные для сборки IDE

| Зависимость | Версия / путь | Зачем |
|-------------|----------------|--------|
| Qt | **5.12.12** `msvc2017_64` (`C:\Qt\Qt5.12.12\5.12.12\msvc2017_64` в vcxproj) | Widgets, Svg, Network; WebEngine — только расширение браузера |
| Компилятор | MSVC **v141** (VS 2017), C++17, x64 | Совместимость с этим Qt |
| QScintilla | Исходники `C:/QScintilla/src`; рантайм `dlldepend/qscintilla2_qt5.dll` (+ `.lib`) | Редактор |
| Windows SDK | в vcxproj `10.0.19041.0` | WinHTTP, диалоги, DPAPI, реестр |
| Python 3 (на машине сборки) | для CMake `BundlePython`; в VS Python копируется из `Python38/` репозитория | Бандл рядом с exe |

## Рантайм рядом с `Vuzhyk.exe`

| Компонент | Назначение |
|-----------|------------|
| `Python38/` | Встроенный CPython 3.8, pyrob, turtlexy, pip |
| `qscintilla2_qt5.dll` | Редактор |
| Qt DLL + `platforms/qwindows.dll`, imageformats, styles | GUI |
| OpenSSL DLL (копирует `scripts/copy-openssl.bat`) | TLS для OpenRouter / Qt Network |
| `FilePicker.exe` | Open/Save/Browse |
| `VuzhykSyncServer.exe` | Сервер учителя |
| `tools/zip/7za.exe` | Zip и `.7z` интерпретаторов |
| `tools/pdf/` | SumatraPDF + учебники 6–7 класса |
| `help/` | HTML справка |

## Внешние сервисы (не библиотеки)

- **LM Studio** — `http://127.0.0.1:1234/v1` (OpenAI-compatible). Не поставляется с IDE.
- **OpenRouter** — `https://openrouter.ai/api/v1`, ключи в QSettings (`agent/openrouterKeys`).
- Архивы интерпретаторов — релизы вроде `python-installers` (`.7z`), не exe-инсталляторы Python.

## Демо-рекордер

- `demo/third_party/minih264e.h` + `DemoH264Impl.c` — программный H.264.
- Контейнер MP4 собирается в `DemoRecorder.cpp` (без FFmpeg).

## Установщик

- Inno Setup 6, скрипт `installer/Vuzhyk.iss`, `MinVersion=6.1sp1`.
- `windeployqt.exe` из `C:\Qt\Qt5.12.12\5.12.12\msvc2017_64\bin` — его запускает `installer/build-installer.bat`, чтобы Qt DLL и плагины (`platforms`, `imageformats`, `iconengines`, `styles`, `translations`, `bearer`) легли в `x64\Release\Vuzhyk\` до упаковки. `Vuzhyk.vcxproj` этого не делает: без windeployqt Qt-файлы в выход кладут руками.

## Что не является зависимостью продукта

- `opencode-dev/` — не линкуется, не ставится.
- `OldVuzhyk/` — не входит в сборку текущего solution (кроме как соседняя папка).
- Системный Python пользователя — опционален; по умолчанию встроенный 3.8.
