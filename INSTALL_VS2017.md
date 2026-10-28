# Установка Visual Studio 2017 Build Tools

## Зачем это нужно?

QScintilla была собрана с Visual Studio 2022, но Qt 5.12.12 собрана с MSVC 2017. Для совместимости с Windows 7 нужно пересобрать QScintilla с тем же компилятором, что использовался для Qt.

## Установка

### Способ 1: Через скрипт (рекомендуется)

1. Запустите `install_vs2017_buildtools.bat` от имени администратора
   - Правой кнопкой мыши → "Запуск от имени администратора"

2. Дождитесь завершения установки (может занять 10-30 минут, скачивается несколько GB)

### Способ 2: Вручную через Chocolatey

Откройте PowerShell от имени администратора и выполните:

```powershell
choco install visualstudio2017buildtools visualstudio2017-workload-vctools -y
```

## После установки

1. **Пересоберите QScintilla** с VS 2017:

   ```cmd
   build_qscintilla.bat
   ```

   Скрипт автоматически найдет и использует VS 2017 Build Tools (если установлен).

2. **Проверьте**, что используется VS 2017:
   - В выводе скрипта должно быть: "Using VS 2017 - compatible with Windows 7"

## Альтернативный способ: Ручной запуск

Если автоматический скрипт не находит VS 2017, откройте вручную:

**"x64 Native Tools Command Prompt for VS 2017"**

Затем перейдите в директорию проекта и запустите:

```cmd
cd D:\Vuzhyk
build_qscintilla.bat
```

## Проверка установки

Проверьте, что VS 2017 Build Tools установлен:

```powershell
Test-Path "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools"
```

Должно вернуть `True`.

## Примечание

После пересборки QScintilla с VS 2017, библиотека будет совместима с Windows 7, так как:
- Qt 5.12.12 собрана с MSVC 2017
- QScintilla будет собрана с MSVC 2017
- Оба используют одинаковый runtime, совместимый с Windows 7








