@echo off
REM Batch file to run PowerShell script as Administrator
REM This will request elevation if not already running as admin

echo Checking for Administrator privileges...

net session >nul 2>&1
if %errorLevel% == 0 (
    echo Running as Administrator. Starting installation...
    echo.
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install_vs2017_buildtools.ps1"
) else (
    echo Not running as Administrator. Requesting elevation...
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Start-Process powershell.exe -ArgumentList '-NoProfile -ExecutionPolicy Bypass -File \"%~dp0install_vs2017_buildtools.ps1\"' -Verb RunAs"
)

pause








