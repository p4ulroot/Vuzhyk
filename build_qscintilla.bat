@echo off
REM Batch wrapper to build QScintilla from x64 Native Tools Command Prompt
REM This script sets up the environment and runs the PowerShell script

echo === Building QScintilla for Qt 5.12.12 ===
echo.

REM Check if PowerShell is available
where powershell >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Error: PowerShell not found
    exit /b 1
)

REM Run PowerShell script with execution policy bypass
powershell.exe -ExecutionPolicy Bypass -File "%~dp0build_qscintilla.ps1" %*

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Build failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo.
echo Build completed successfully!
pause








