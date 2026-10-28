# Script to build QScintilla for Qt 5.12.12
# Usage: .\build_qscintilla.ps1
# Requires QScintilla sources in C:\QScintilla (or specify path with -QScintillaPath)

param(
    [string]$QtDir = "C:\Qt\Qt5.12.12\5.12.12\msvc2017_64",
    [string]$QScintillaPath = "C:\QScintilla"
)

Write-Host "=== Building QScintilla for Qt 5.12.12 ===" -ForegroundColor Green

# Function to setup Visual Studio environment for x64
function Setup-VSEnvironment {
    # Try vcvars64.bat first (more reliable for x64)
    # Priority: VS 2017 first (for Windows 7 compatibility with Qt 5.12.12)
    $vcvarsPaths = @(
        "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    )
    
    foreach ($vcvarsPath in $vcvarsPaths) {
        if (Test-Path $vcvarsPath) {
            Write-Host "Setting up Visual Studio x64 environment from: $vcvarsPath" -ForegroundColor Cyan
            if ($vcvarsPath -match "2017") {
                Write-Host "Using VS 2017 - compatible with Windows 7 (matches Qt 5.12.12 msvc2017_64)" -ForegroundColor Green
            }
            $tempFile = [System.IO.Path]::GetTempFileName()
            $batchContent = "@echo off`ncall `"$vcvarsPath`"`nset > `"$tempFile`""
            $batchFile = [System.IO.Path]::ChangeExtension($tempFile, ".bat")
            Set-Content -Path $batchFile -Value $batchContent
            & cmd /c $batchFile
            if (Test-Path $tempFile) {
                Get-Content $tempFile | ForEach-Object {
                    if ($_ -match "^([^=]+)=(.*)$") {
                        $name = $matches[1]
                        $value = $matches[2]
                        [System.Environment]::SetEnvironmentVariable($name, $value, "Process")
                    }
                }
                Remove-Item $tempFile -ErrorAction SilentlyContinue
            }
            Remove-Item $batchFile -ErrorAction SilentlyContinue
            Write-Host "Visual Studio x64 environment configured" -ForegroundColor Green
            return $true
        }
    }
    
    # Fallback to VsDevCmd.bat with -arch=x64
    # Priority: VS 2017 first (for Windows 7 compatibility with Qt 5.12.12)
    $vsPaths = @(
        "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
    )
    
    foreach ($vsPath in $vsPaths) {
        if (Test-Path $vsPath) {
            Write-Host "Setting up Visual Studio x64 environment from: $vsPath" -ForegroundColor Cyan
            if ($vsPath -match "2017") {
                Write-Host "Using VS 2017 - compatible with Windows 7 (matches Qt 5.12.12 msvc2017_64)" -ForegroundColor Green
            }
            $tempFile = [System.IO.Path]::GetTempFileName()
            $batchContent = "@echo off`ncall `"$vsPath`" -arch=x64`nset > `"$tempFile`""
            $batchFile = [System.IO.Path]::ChangeExtension($tempFile, ".bat")
            Set-Content -Path $batchFile -Value $batchContent
            & cmd /c $batchFile
            if (Test-Path $tempFile) {
                Get-Content $tempFile | ForEach-Object {
                    if ($_ -match "^([^=]+)=(.*)$") {
                        $name = $matches[1]
                        $value = $matches[2]
                        [System.Environment]::SetEnvironmentVariable($name, $value, "Process")
                    }
                }
                Remove-Item $tempFile -ErrorAction SilentlyContinue
            }
            Remove-Item $batchFile -ErrorAction SilentlyContinue
            Write-Host "Visual Studio x64 environment configured" -ForegroundColor Green
            return $true
        }
    }
    
    return $false
}

# Check and setup Visual Studio environment for x64
$clCmd = Get-Command cl -ErrorAction SilentlyContinue
$isX64Env = $false

if ($clCmd) {
    $clPath = $clCmd.Source
    # Check if we're already in x64 environment
    if ($clPath -match "\\x64\\" -or $clPath -match "HostX64\\x64" -or $env:VCINSTALLDIR -match "x64") {
        Write-Host "Detected x64 Visual Studio environment" -ForegroundColor Green
        $isX64Env = $true
    } else {
        Write-Host "Warning: Compiler may not be x64: $clPath" -ForegroundColor Yellow
    }
}

# Setup environment if not already configured for x64
if (-not $isX64Env) {
    Write-Host "Setting up Visual Studio x64 environment..." -ForegroundColor Cyan
    if (-not (Setup-VSEnvironment)) {
        Write-Host "Warning: Could not setup Visual Studio environment automatically" -ForegroundColor Yellow
        Write-Host "Please run this script from 'x64 Native Tools Command Prompt for Visual Studio'" -ForegroundColor Yellow
        Write-Host "Or use build_qscintilla.bat from x64 Native Tools Command Prompt" -ForegroundColor Yellow
    } else {
        # Verify x64 environment after setup
        $clCmd = Get-Command cl -ErrorAction SilentlyContinue
        if ($clCmd) {
            $clPath = $clCmd.Source
            if ($clPath -match "\\x64\\" -or $clPath -match "HostX64") {
                Write-Host "Verified: Using x64 compiler" -ForegroundColor Green
            }
        }
    }
}

# Check Qt installation
if (-not (Test-Path "$QtDir\bin\qmake.exe")) {
    Write-Host "Error: Qt not found in $QtDir" -ForegroundColor Red
    Write-Host "Install Qt 5.12.12 or specify correct path with -QtDir parameter" -ForegroundColor Yellow
    exit 1
}

Write-Host "Qt found: $QtDir" -ForegroundColor Green

# Check QScintilla sources
if (-not (Test-Path $QScintillaPath)) {
    Write-Host "Error: QScintilla not found in $QScintillaPath" -ForegroundColor Red
    Write-Host "Please extract QScintilla sources to C:\QScintilla" -ForegroundColor Yellow
    Write-Host "Or specify correct path with -QScintillaPath parameter" -ForegroundColor Yellow
    exit 1
}

Write-Host "QScintilla found: $QScintillaPath" -ForegroundColor Green

Write-Host "`n=== Building QScintilla ===" -ForegroundColor Cyan

# Path to qmake
$QMakePath = "$QtDir\bin\qmake.exe"

# Detect QScintilla structure (old vs new)
$Qt5Dir = "$QScintillaPath\Qt5Qt6"
$OldSrcDir = "$QScintillaPath\src"
$BuildDir = $null
$ProFile = $null
$HeaderSource = $null

if (Test-Path $Qt5Dir) {
    # New structure (QScintilla 2.13+)
    Write-Host "Detected new QScintilla structure (Qt5Qt6)" -ForegroundColor Cyan
    $BuildDir = $Qt5Dir
    $ProFile = "$BuildDir\qscintilla.pro"
    $HeaderSource = "$BuildDir\Qsci"
} elseif (Test-Path "$OldSrcDir\qscintilla.pro") {
    # Old structure (QScintilla < 2.13)
    Write-Host "Detected old QScintilla structure (src)" -ForegroundColor Cyan
    $BuildDir = $OldSrcDir
    $ProFile = "$BuildDir\qscintilla.pro"
    $HeaderSource = "$BuildDir\Qsci"
} else {
    Write-Host "Error: Cannot detect QScintilla structure" -ForegroundColor Red
    Write-Host "Expected either:" -ForegroundColor Yellow
    Write-Host "  - New structure: $Qt5Dir\qscintilla.pro" -ForegroundColor Yellow
    Write-Host "  - Old structure: $OldSrcDir\qscintilla.pro" -ForegroundColor Yellow
    exit 1
}

if (-not (Test-Path $ProFile)) {
    Write-Host "Error: qscintilla.pro not found in $BuildDir" -ForegroundColor Red
    exit 1
}

# Change to build directory
Push-Location $BuildDir

try {
    # Build Qt5 version
    Write-Host "`n--- Building Qt5 version ---" -ForegroundColor Yellow
    
    # Generate project for x64
    Write-Host "Generating project with qmake (x64)..." -ForegroundColor Yellow
    
    # Check qmake version
    $qmakeVersion = & "$QMakePath" -v 2>&1 | Select-Object -First 1
    Write-Host "Using qmake: $qmakeVersion" -ForegroundColor Cyan
    
    # Force x64 architecture by setting environment variables
    # These variables tell qmake and the compiler to use x64
    $env:QMAKESPEC = "win32-msvc"
    
    # Set architecture-specific environment variables
    if (-not $env:PLATFORM) {
        $env:PLATFORM = "x64"
    }
    
    # Generate Makefile - qmake should detect x64 from Qt installation path (msvc2017_64)
    # But we'll also explicitly set the target architecture
    Write-Host "Generating Makefile for x64..." -ForegroundColor Cyan
    & "$QMakePath" qscintilla.pro
    
    # After qmake, aggressively check and fix Makefile to ensure x64
    $makefiles = @("Makefile.Release", "Makefile.Debug", "Makefile")
    
    foreach ($makefilePath in $makefiles) {
        if (Test-Path $makefilePath) {
            Write-Host "Checking $makefilePath for x64 configuration..." -ForegroundColor Cyan
            $makefileContent = Get-Content $makefilePath -Raw
            $needsFix = $false
            
            # Fix MACHINE parameter in LFLAGS (qmake uses LFLAGS, not LINK_FLAGS)
            if ($makefileContent -match "/MACHINE:X86" -or $makefileContent -match "machine:x86" -or $makefileContent -match "/MACHINE:I386") {
                Write-Host "  Found x86 MACHINE parameter, fixing..." -ForegroundColor Yellow
                $makefileContent = $makefileContent -replace "/MACHINE:X86", "/MACHINE:X64"
                $makefileContent = $makefileContent -replace "/MACHINE:I386", "/MACHINE:X64"
                $makefileContent = $makefileContent -replace "machine:x86", "machine:x64"
                $needsFix = $true
            }
            
            # Ensure MACHINE:X64 is present in LFLAGS if not already there
            if ($makefileContent -match "LFLAGS\s*=" -and $makefileContent -notmatch "/MACHINE:X64") {
                Write-Host "  Adding /MACHINE:X64 to LFLAGS..." -ForegroundColor Yellow
                # Match LFLAGS line and add /MACHINE:X64 at the end using simple string replacement
                if ($makefileContent -match "(LFLAGS\s*=\s*[^\r\n]+)") {
                    $oldLflagsLine = $matches[1]
                    $newLflagsLine = $oldLflagsLine.TrimEnd() + " /MACHINE:X64"
                    $makefileContent = $makefileContent -replace [regex]::Escape($oldLflagsLine), $newLflagsLine
                    $needsFix = $true
                }
            }
            
            # Also check for LINK_FLAGS (some qmake versions use this)
            if ($makefileContent -match "LINK_FLAGS\s*=" -and $makefileContent -notmatch "/MACHINE:X64") {
                Write-Host "  Adding /MACHINE:X64 to LINK_FLAGS..." -ForegroundColor Yellow
                $makefileContent = $makefileContent -replace "(LINK_FLAGS\s*=\s*)([^\r\n]*)", '$1$2 /MACHINE:X64'
                $needsFix = $true
            }
            
            # Fix compiler paths - ensure they point to x64
            if ($makefileContent -match "\\x86\\" -and $makefileContent -notmatch "HostX64") {
                Write-Host "  Found x86 compiler paths, fixing..." -ForegroundColor Yellow
                # Replace x86 paths with x64 where possible
                $makefileContent = $makefileContent -replace "\\bin\\HostX86\\x86\\", "\bin\HostX64\x64\"
                $makefileContent = $makefileContent -replace "\\bin\\x86\\", "\bin\x64\"
                $needsFix = $true
            }
            
            # Ensure compiler is x64 version - check CC and CXX paths
            # If CC/CXX just says "cl", we need to ensure it's the x64 version from PATH
            # But actually, if we're in x64 Native Tools, "cl" should already be x64
            # The issue might be that old .obj files exist. We'll clean them before build.
            
            # However, we can try to explicitly set the compiler path if needed
            # But this is usually not necessary if environment is set up correctly
            
            # Fix library paths
            if ($makefileContent -match "lib\\x86" -or $makefileContent -match "lib\\i386") {
                Write-Host "  Found x86 library paths, fixing..." -ForegroundColor Yellow
                $makefileContent = $makefileContent -replace "lib\\x86", "lib\x64"
                $makefileContent = $makefileContent -replace "lib\\i386", "lib\x64"
                $needsFix = $true
            }
            
            if ($needsFix) {
                Set-Content -Path $makefilePath -Value $makefileContent -NoNewline
                Write-Host "  Fixed $makefilePath for x64" -ForegroundColor Green
            } else {
                # Verify x64 is present
                if ($makefileContent -match "/MACHINE:X64" -or $makefileContent -match "\\x64\\" -or $makefileContent -match "HostX64") {
                    Write-Host "  $makefilePath already configured for x64" -ForegroundColor Green
                } else {
                    Write-Host "  Warning: $makefilePath may not be explicitly configured for x64" -ForegroundColor Yellow
                }
            }
        }
    }
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error generating project" -ForegroundColor Red
        exit 1
    }
    
    # Clean old object files to avoid x86/x64 conflicts
    Write-Host "Cleaning old object files..." -ForegroundColor Cyan
    if (Test-Path "release") {
        # Remove only .obj files, keep directory structure
        Get-ChildItem -Path "release" -Filter "*.obj" -Recurse | Remove-Item -Force -ErrorAction SilentlyContinue
        Write-Host "  Cleaned .obj files from release directory" -ForegroundColor Green
    } else {
        # Create release directory if it doesn't exist
        New-Item -ItemType Directory -Path "release" -Force | Out-Null
        Write-Host "  Created release directory" -ForegroundColor Green
    }
    if (Test-Path "debug") {
        # Remove only .obj files, keep directory structure
        Get-ChildItem -Path "debug" -Filter "*.obj" -Recurse | Remove-Item -Force -ErrorAction SilentlyContinue
        Write-Host "  Cleaned .obj files from debug directory" -ForegroundColor Green
    } else {
        # Create debug directory if it doesn't exist
        New-Item -ItemType Directory -Path "debug" -Force | Out-Null
        Write-Host "  Created debug directory" -ForegroundColor Green
    }
    
    # Check for nmake and verify it's x64
    $nmakeCheck = Get-Command nmake -ErrorAction SilentlyContinue
    if (-not $nmakeCheck) {
        Write-Host "Error: nmake not found in PATH" -ForegroundColor Red
        Write-Host "Open 'x64 Native Tools Command Prompt for Visual Studio' and run script from there" -ForegroundColor Yellow
        exit 1
    }
    
    # Verify nmake is x64 version
    $nmakePath = (Get-Command nmake).Source
    if ($nmakePath -match "HostX86\\x86" -or $nmakePath -match "\\x86\\") {
        Write-Host "Warning: nmake appears to be x86 version: $nmakePath" -ForegroundColor Yellow
        Write-Host "This may cause architecture mismatch. Using x64 Native Tools is recommended." -ForegroundColor Yellow
    } elseif ($nmakePath -match "HostX64\\x64" -or $nmakePath -match "\\x64\\") {
        Write-Host "Using x64 nmake: $nmakePath" -ForegroundColor Green
    }
    
    # Verify compiler is x64 version
    $clCmd = Get-Command cl -ErrorAction SilentlyContinue
    if ($clCmd) {
        $clPath = $clCmd.Source
        if ($clPath -match "\\x64\\" -or $clPath -match "HostX64\\x64") {
            Write-Host "Using x64 compiler: $clPath" -ForegroundColor Green
        } elseif ($clPath -match "\\x86\\" -or $clPath -match "HostX86") {
            Write-Host "Warning: Compiler appears to be x86: $clPath" -ForegroundColor Yellow
            Write-Host "This will cause x86/x64 mismatch. Ensure you're using x64 Native Tools." -ForegroundColor Yellow
        } else {
            Write-Host "Compiler path: $clPath" -ForegroundColor Cyan
        }
    } else {
        Write-Host "Warning: cl compiler not found in PATH" -ForegroundColor Yellow
    }
    
    # Build Debug version
    Write-Host "`nBuilding Debug version..." -ForegroundColor Yellow
    & nmake debug
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error building Debug version" -ForegroundColor Red
        Write-Host "Try running 'nmake debug' manually in directory $BuildDir" -ForegroundColor Yellow
        Write-Host "Or check that you are using correct Visual Studio version (2017 for Qt 5.12.12)" -ForegroundColor Yellow
    } else {
        Write-Host "Debug version built successfully" -ForegroundColor Green
    }
    
    # Build Release version
    Write-Host "`nBuilding Release version..." -ForegroundColor Yellow
    & nmake release
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error building Release version" -ForegroundColor Red
        Write-Host "Try running 'nmake release' manually in directory $BuildDir" -ForegroundColor Yellow
        Write-Host "Or check that you are using correct Visual Studio version (2017 for Qt 5.12.12)" -ForegroundColor Yellow
    } else {
        Write-Host "Release version built successfully" -ForegroundColor Green
    }
    
    # Install (copy files to Qt)
    Write-Host "`n--- Installing to Qt ---" -ForegroundColor Yellow
    
    # Copy headers
    $IncludeDest = "$QtDir\include\Qsci"
    
    if (-not (Test-Path $IncludeDest)) {
        New-Item -ItemType Directory -Path $IncludeDest | Out-Null
    }
    
    if (-not (Test-Path $HeaderSource)) {
        Write-Host "Warning: Header source directory not found: $HeaderSource" -ForegroundColor Yellow
    } else {
        Write-Host "Copying headers..." -ForegroundColor Yellow
        Copy-Item "$HeaderSource\*.h" -Destination $IncludeDest -Force
        Write-Host "Headers copied to $IncludeDest" -ForegroundColor Green
    }
    
    # Copy libraries
    $LibSourceDebug = "$BuildDir\debug"
    $LibSourceRelease = "$BuildDir\release"
    $LibDest = "$QtDir\lib"
    
    Write-Host "Copying libraries..." -ForegroundColor Yellow
    
    if (Test-Path "$LibSourceDebug\qscintilla2_qt5d.lib") {
        Copy-Item "$LibSourceDebug\qscintilla2_qt5d.lib" -Destination $LibDest -Force
        Write-Host "Debug library copied" -ForegroundColor Green
    }
    
    if (Test-Path "$LibSourceRelease\qscintilla2_qt5.lib") {
        Copy-Item "$LibSourceRelease\qscintilla2_qt5.lib" -Destination $LibDest -Force
        Write-Host "Release library copied" -ForegroundColor Green
    }
    
    # Copy DLLs
    $BinDest = "$QtDir\bin"
    
    if (Test-Path "$LibSourceDebug\qscintilla2_qt5d.dll") {
        Copy-Item "$LibSourceDebug\qscintilla2_qt5d.dll" -Destination $BinDest -Force
        Write-Host "Debug DLL copied" -ForegroundColor Green
    }
    
    if (Test-Path "$LibSourceRelease\qscintilla2_qt5.dll") {
        Copy-Item "$LibSourceRelease\qscintilla2_qt5.dll" -Destination $BinDest -Force
        Write-Host "Release DLL copied" -ForegroundColor Green
    }
    
    Pop-Location
    
    Write-Host "`n=== Build completed successfully! ===" -ForegroundColor Green
    Write-Host "QScintilla installed to: $QtDir" -ForegroundColor Green
    Write-Host "Headers: $IncludeDest" -ForegroundColor Cyan
    Write-Host "Libraries: $LibDest" -ForegroundColor Cyan
    Write-Host "DLLs: $BinDest" -ForegroundColor Cyan
    
} catch {
    Write-Host "Error: $_" -ForegroundColor Red
    exit 1
} finally {
    Pop-Location
}
