# Manual script to build QScintilla (if automatic script doesn't work)
# Usage: 
# 1. Download QScintilla manually and extract
# 2. Run: .\build_qscintilla_manual.ps1 -QScintillaPath "C:\qscintilla-2.13.3"

param(
    [Parameter(Mandatory=$true)]
    [string]$QScintillaPath,
    
    [string]$QtDir = "C:\Qt\Qt5.12.12\5.12.12\msvc2017_64"
)

Write-Host "=== Manual build of QScintilla ===" -ForegroundColor Green

# Check paths
if (-not (Test-Path $QScintillaPath)) {
    Write-Host "Error: QScintilla not found in $QScintillaPath" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path "$QtDir\bin\qmake.exe")) {
    Write-Host "Error: Qt not found in $QtDir" -ForegroundColor Red
    exit 1
}

$Qt5Dir = "$QScintillaPath\Qt5Qt6"
if (-not (Test-Path $Qt5Dir)) {
    Write-Host "Error: Qt5Qt6 directory not found in $QScintillaPath" -ForegroundColor Red
    Write-Host "Make sure you extracted correct QScintilla version" -ForegroundColor Yellow
    exit 1
}

Write-Host "QScintilla: $QScintillaPath" -ForegroundColor Green
Write-Host "Qt: $QtDir" -ForegroundColor Green

# Change to build directory
Push-Location $Qt5Dir

try {
    $QMakePath = "$QtDir\bin\qmake.exe"
    
    # Generate project
    Write-Host "`nGenerating project..." -ForegroundColor Yellow
    & "$QMakePath" qscintilla.pro
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error generating project" -ForegroundColor Red
        exit 1
    }
    
    # Build
    Write-Host "`nBuilding Debug version..." -ForegroundColor Yellow
    nmake debug
    
    Write-Host "`nBuilding Release version..." -ForegroundColor Yellow
    nmake release
    
    # Install
    Write-Host "`nInstalling files..." -ForegroundColor Yellow
    
    # Headers
    $IncludeDest = "$QtDir\include\Qsci"
    if (-not (Test-Path $IncludeDest)) {
        New-Item -ItemType Directory -Path $IncludeDest | Out-Null
    }
    Copy-Item "Qsci\*.h" -Destination $IncludeDest -Force
    
    # Libraries
    if (Test-Path "debug\qscintilla2_qt5d.lib") {
        Copy-Item "debug\qscintilla2_qt5d.lib" -Destination "$QtDir\lib" -Force
    }
    if (Test-Path "release\qscintilla2_qt5.lib") {
        Copy-Item "release\qscintilla2_qt5.lib" -Destination "$QtDir\lib" -Force
    }
    
    # DLLs
    if (Test-Path "debug\qscintilla2_qt5d.dll") {
        Copy-Item "debug\qscintilla2_qt5d.dll" -Destination "$QtDir\bin" -Force
    }
    if (Test-Path "release\qscintilla2_qt5.dll") {
        Copy-Item "release\qscintilla2_qt5.dll" -Destination "$QtDir\bin" -Force
    }
    
    Write-Host "`n=== Installation completed! ===" -ForegroundColor Green
    
} catch {
    Write-Host "Error: $_" -ForegroundColor Red
    exit 1
} finally {
    Pop-Location
}
