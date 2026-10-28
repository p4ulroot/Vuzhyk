# Script to install Visual Studio 2017 Build Tools via Chocolatey
# This script must be run as Administrator

Write-Host "=== Installing Visual Studio 2017 Build Tools ===" -ForegroundColor Cyan
Write-Host ""

# Check if running as Administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "ERROR: This script must be run as Administrator!" -ForegroundColor Red
    Write-Host "Right-click PowerShell and select 'Run as Administrator'" -ForegroundColor Yellow
    exit 1
}

# Check if Chocolatey is installed
$chocoInstalled = Get-Command choco -ErrorAction SilentlyContinue
if (-not $chocoInstalled) {
    Write-Host "ERROR: Chocolatey is not installed!" -ForegroundColor Red
    Write-Host "Install Chocolatey first from https://chocolatey.org/install" -ForegroundColor Yellow
    exit 1
}

Write-Host "Chocolatey version: $(choco --version)" -ForegroundColor Green
Write-Host ""

# Install Visual Studio 2017 Build Tools with C++ workload
Write-Host "Installing Visual Studio 2017 Build Tools..." -ForegroundColor Cyan
Write-Host "This may take a while (several GB download)..." -ForegroundColor Yellow
Write-Host ""

choco install visualstudio2017buildtools visualstudio2017-workload-vctools -y

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "=== Installation completed successfully! ===" -ForegroundColor Green
    Write-Host ""
    Write-Host "Visual Studio 2017 Build Tools installed to:" -ForegroundColor Cyan
    Write-Host "  C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools" -ForegroundColor White
    Write-Host ""
    Write-Host "To use VS 2017 compiler, open:" -ForegroundColor Cyan
    Write-Host "  'Developer Command Prompt for VS 2017' (x64)" -ForegroundColor White
    Write-Host "  or" -ForegroundColor White
    Write-Host "  'x64 Native Tools Command Prompt for VS 2017'" -ForegroundColor White
    Write-Host ""
} else {
    Write-Host ""
    Write-Host "=== Installation failed! ===" -ForegroundColor Red
    Write-Host "Check the error messages above" -ForegroundColor Yellow
    exit 1
}








