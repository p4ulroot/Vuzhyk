@echo off
setlocal EnableExtensions

rem Usage: copy-openssl.bat <OutDir> <QtDir> <SolutionDir>
set "OUTDIR=%~1"
set "QTDIR=%~2"
set "ROOT=%~3"

if "%OUTDIR%"=="" exit /b 1
if not "%OUTDIR:~-1%"=="\" set "OUTDIR=%OUTDIR%\"
if not exist "%OUTDIR%" mkdir "%OUTDIR%" >nul 2>&1

set "SSL_DIR="

call :try_dir "%ROOT%third_party\openssl"
call :try_dir "%QTDIR%\bin"
call :try_dir "%QTDIR%\..\..\..\Tools\OpenSSL\Win_x64\bin"
call :try_dir "C:\Qt\Tools\OpenSSL\Win_x64\bin"
call :try_dir "C:\Qt\Qt5.12.12\Tools\OpenSSL\Win_x64\bin"
call :try_dir "%ROOT%Python38\DLLs"
call :try_dir "%OUTDIR%Python38\DLLs"

if defined SSL_DIR (
  call :copy_pair "%SSL_DIR%"
  if exist "%OUTDIR%libssl-1_1-x64.dll" if exist "%OUTDIR%libcrypto-1_1-x64.dll" (
    echo OpenSSL: copied from "%SSL_DIR%" to "%OUTDIR%"
    exit /b 0
  )
)

echo OpenSSL: libssl-1_1-x64.dll / libcrypto-1_1-x64.dll not found.
echo Put them into third_party\openssl or install Qt Tools OpenSSL.
exit /b 1

:try_dir
if defined SSL_DIR goto :eof
if exist "%~1\libssl-1_1-x64.dll" if exist "%~1\libcrypto-1_1-x64.dll" (
  set "SSL_DIR=%~1"
  goto :eof
)
if exist "%~1\libssl-1_1.dll" if exist "%~1\libcrypto-1_1.dll" (
  set "SSL_DIR=%~1"
)
goto :eof

:copy_pair
if exist "%~1\libssl-1_1-x64.dll" (
  copy /Y "%~1\libssl-1_1-x64.dll" "%OUTDIR%libssl-1_1-x64.dll" >nul
  copy /Y "%~1\libcrypto-1_1-x64.dll" "%OUTDIR%libcrypto-1_1-x64.dll" >nul
) else (
  rem Python/OpenSSL without -x64 suffix: Qt 5.12 x64 still looks for *-x64.dll
  copy /Y "%~1\libssl-1_1.dll" "%OUTDIR%libssl-1_1-x64.dll" >nul
  copy /Y "%~1\libcrypto-1_1.dll" "%OUTDIR%libcrypto-1_1-x64.dll" >nul
)
goto :eof
