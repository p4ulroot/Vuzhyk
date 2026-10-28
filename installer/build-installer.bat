@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem ===========================================================================
rem Vuzhyk - build the Windows installer (Inno Setup 6).
rem
rem Run in the VM that has: Visual Studio (toolset v141), Qt 5.12.12
rem msvc2017_64, Inno Setup 6 and a finished Release|x64 build.
rem
rem Steps:
rem   1. require x64\Release\Vuzhyk\Vuzhyk.exe
rem   2. deploy Qt next to the exe with windeployqt (when it is found)
rem   3. hard-check the files without which the installed IDE cannot work
rem   4. run ISCC and verify that the setup exe appeared
rem   5. copy the setup to \\vmware-host\Shared Folders\vmshared\Installer
rem      when that share is mounted
rem
rem The version comes from installer\Vuzhyk.iss (#define AppVersion) - that is
rem the only place to edit it. Override for a single run:
rem   installer\build-installer.bat 26.5.0
rem ===========================================================================

cd /d "%~dp0.."

set "BUILD_OUT=x64\Release\Vuzhyk"
set "ISS=installer\Vuzhyk.iss"
set "OUT_DIR=installer\output"

rem --- Step 0: resolve the version -------------------------------------------
set "APP_VERSION=%~1"
if defined APP_VERSION goto :version_ready

rem "#define AppVersion "26.4.1"" -> token 3 = "26.4.1" with quotes
set "VER_TMP="
for /f "tokens=3" %%v in ('findstr /c:"#define AppVersion" "%ISS%"') do set "VER_TMP=%%v"
if not defined VER_TMP goto :no_version
set "APP_VERSION=%VER_TMP:"=%"
if not defined APP_VERSION goto :no_version

:version_ready
echo ============================================================
echo  Vuzhyk installer %APP_VERSION%
echo  Build output: %BUILD_OUT%
echo ============================================================

rem --- Step 1: the Release build must exist ----------------------------------
if not exist "%BUILD_OUT%\Vuzhyk.exe" (
    echo [ERROR] Not found: %BUILD_OUT%\Vuzhyk.exe
    echo         Build Vuzhyk.sln ^(Release ^| x64^) first.
    exit /b 1
)

rem --- Step 2: deploy the Qt runtime next to the exe -------------------------
call :find_windeployqt
if not defined WINDEPLOYQT goto :no_windeployqt
echo [Qt] windeployqt: !WINDEPLOYQT!
"!WINDEPLOYQT!" --release --compiler-runtime "%BUILD_OUT%\Vuzhyk.exe"
if errorlevel 1 echo [WARN] windeployqt failed - check the Qt DLLs in %BUILD_OUT% manually.
goto :qt_done

:no_windeployqt
echo [WARN] windeployqt.exe not found - expecting the Qt DLLs to be already in %BUILD_OUT%.

:qt_done
rem --- Step 3: hard requirements ---------------------------------------------
rem help\gradeN_main.html are read from disk only; without them the student
rem sees the "Razdel v razrabotke..." stub in 16 of 55 help sections.
set "MISSING=0"
for %%F in (
    "Qt5Core.dll"
    "Qt5Gui.dll"
    "Qt5Widgets.dll"
    "Qt5Network.dll"
    "qscintilla2_qt5.dll"
    "platforms\qwindows.dll"
    "FilePicker.exe"
    "Python38\python.exe"
    "help\grade6_main.html"
    "help\grade7_main.html"
    "tools\zip\7za.exe"
) do (
    if not exist "%BUILD_OUT%\%%~F" (
        echo [ERROR] Missing: %BUILD_OUT%\%%~F
        set "MISSING=1"
    )
)
if "!MISSING!"=="1" (
    echo.
    echo [ERROR] Build output is incomplete - the installer was NOT built.
    echo         Rebuild Vuzhyk.sln ^(Release ^| x64^), then run this script again.
    exit /b 1
)

rem --- Step 4: warnings only -------------------------------------------------
if not exist "%BUILD_OUT%\VuzhykSyncServer.exe" echo [WARN] VuzhykSyncServer.exe missing - the teacher role will be unavailable.
if not exist "%BUILD_OUT%\msvcp140.dll" echo [WARN] msvcp140.dll missing - target PCs need the MSVC 2015-2019 x64 runtime.
if not exist "%BUILD_OUT%\libssl-1_1-x64.dll" echo [WARN] libssl-1_1-x64.dll missing - OpenRouter ^(HTTPS^) will not work.
if not exist "%BUILD_OUT%\tasks\turtle_task_runner.py" echo [WARN] tasks\turtle_task_runner.py missing - the turtle workbook will use the copy embedded in Vuzhyk.exe.

rem --- Step 5: Inno Setup -----------------------------------------------------
set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=C:\Program Files\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" goto :no_iscc

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo [ISCC] Building installer ^(version %APP_VERSION%^)...
"%ISCC%" /DAppVersion="%APP_VERSION%" "%ISS%"
if errorlevel 1 (
    echo [ERROR] ISCC failed - see the messages above.
    exit /b 1
)

set "SETUP=%OUT_DIR%\VuzhykSetup-%APP_VERSION%.exe"
if not exist "%SETUP%" (
    echo [ERROR] Expected installer not found: %SETUP%
    exit /b 1
)

echo.
echo Done: %SETUP%
for %%S in ("%SETUP%") do echo Size: %%~zS bytes

rem --- Step 6: publish to the VMware shared folder (optional) -----------------
set "SHARE=\\vmware-host\Shared Folders\vmshared"
if not exist "%SHARE%" goto :no_share
if not exist "%SHARE%\Installer" mkdir "%SHARE%\Installer" >nul 2>&1
robocopy "%OUT_DIR%" "%SHARE%\Installer" "VuzhykSetup-%APP_VERSION%.exe" /IS /IT /NFL /NDL /NJH /NJS >nul
if errorlevel 8 (
    echo [WARN] Could not copy the installer to "%SHARE%\Installer".
) else (
    echo Copied to: %SHARE%\Installer
)
goto :done

:no_share
echo [INFO] Shared folder not available - the installer stays in %OUT_DIR%.

:done
exit /b 0

rem ===========================================================================
rem Error exits
rem ===========================================================================
:no_version
echo [ERROR] Cannot read "#define AppVersion" from %ISS%
echo         Fix the script or pass the version as the first argument.
exit /b 1

:no_iscc
echo [ERROR] Inno Setup 6 not found. Install it from https://jrsoftware.org/isdl.php
echo         Expected: "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
exit /b 1

rem ===========================================================================
rem Subroutines
rem ===========================================================================
:find_windeployqt
set "WINDEPLOYQT="
if defined QTDIR if exist "%QTDIR%\bin\windeployqt.exe" set "WINDEPLOYQT=%QTDIR%\bin\windeployqt.exe"
if defined WINDEPLOYQT goto :eof
if exist "C:\Qt\Qt5.12.12\5.12.12\msvc2017_64\bin\windeployqt.exe" (
    set "WINDEPLOYQT=C:\Qt\Qt5.12.12\5.12.12\msvc2017_64\bin\windeployqt.exe"
    goto :eof
)
if not exist "C:\Qt" goto :eof
for /d %%D in ("C:\Qt\*") do (
    if not defined WINDEPLOYQT if exist "%%D\5.12.12\msvc2017_64\bin\windeployqt.exe" (
        set "WINDEPLOYQT=%%D\5.12.12\msvc2017_64\bin\windeployqt.exe"
    )
)
goto :eof
