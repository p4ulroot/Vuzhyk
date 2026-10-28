# Conventions

## Язык

- UI, сообщения об ошибках, справка — **русский**.
- Идентификаторы кода — английский (`MainWindow`, `startRecording`).
- Комментарии в существующих файлах часто русские; не добавляй комментарии «для галочки», если пользователь не просил.

## C++ / Qt

- Стандарт C++17, Qt Widgets (не QML для главного окна).
- Строки: `QStringLiteral("…")`.
- Пара `.h` / `.cpp`, include guard или `#pragma once` как в соседнем файле модуля.
- Сигналы/слоты Qt, `Q_OBJECT` где нужен moc. Новые заголовки с `Q_OBJECT` — в `QtMoc` в vcxproj.
- Не использовать Qt 6 API (`QString::arg` перегрузки, которых нет в 5.12, и т.п.).
- Не тянуть WebEngine в `Vuzhyk.exe`.

## Проекты VS

- Платформа только **x64**.
- Qt в vcxproj: `QtVersion` 5.12.12, `QtMsBuild`, toolset **v141**.
- Новый демо-exe: копировать ближайший `Demo*.vcxproj` + `.filters`, подключить `demo.qrc` + `demo/app.rc`, добавить в `Vuzhyk.sln` секцию `Demo|x64`. `TargetName` — имя + следующий номер (сейчас последний 12).
- IntDir у демо раздельные (`DemoHello_intermediate` и т.д.), OutDir общий `x64\Demo\Demo\`.

## Ресурсы

- Иконки тулбара — `icons/light-theme/*.svg` в `resources.qrc` (prefix `/icons`). Тёмная тема часто перекрашивает те же SVG в коде, отдельной папки `dark-theme` в репозитории нет.
- Новые иконки — и в `resources.qrc`, и использование в коде через `:/icons/icons/...`.
- `turtle_task_runner.py` — и в qrc, и исходник в `src/tasks/`.

## Python в продукте

- Учебные пакеты: `pyrob`, `turtlexy`, `turtlexy_fvt` в site-packages встроенного Python.
- Не хардкодить `C:\Users\...\AppData\...site-packages`.
- Распаковка архивов только через `tools/zip/7za.exe` (как синхронизация).

## Демо-рекордер

- Менять качество/FPS/формат только в `demo/DemoRecorder.cpp` — это общее для всех exe.
- Сейчас: 60 fps, planar 4:2:0, limited range, encode_speed не FASTEST (нужен deblock), высокий `desired_frame_bytes`.
- Префикс MP4 задаётся в `DemoRecorder::start(window, prefix)`.

## Git

- Не коммитить `x64/`, `.vs/`, `.qt/`, `*.user`, логи, `.7z`.
- `Info_docs/` и `AGENTS.md` **не** игнорировать.
- Секреты и `%APPDATA%\Vuzhyk\` не в репозиторий.
- Коммит — только по просьбе пользователя.

## Поведение агента в этом репозитории

- Не выдумывать API, которого нет в заголовках.
- После правок UI по возможности проверять сценарий (или честно писать, что браузера/стенда нет).
- Не «улучшать» DemoTurtleHouse (светлая полная печать домика), если задача не про эту сцену.
