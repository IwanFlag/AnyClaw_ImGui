# build.ps1 — AnyClaw PowerShell build script
# Usage:
#   .\build.ps1                    — Debug build
#   .\build.ps1 -Release           — Release build
#   .\build.ps1 -Release -Installer — Release + NSIS installer

param(
    [switch]$Release,
    [switch]$Installer
)

$ErrorActionPreference = "Stop"

$BuildType = if ($Release) { "Release" } else { "Debug" }

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " AnyClaw Build" -ForegroundColor Cyan
Write-Host " Type: $BuildType" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check prerequisites
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: CMake not found. Install CMake 3.16+." -ForegroundColor Red
    exit 1
}

# Detect VS version
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWhere) {
    $vsInstall = & $vsWhere -latest -property installationPath
    if ($vsInstall) {
        Write-Host "Found Visual Studio at: $vsInstall" -ForegroundColor Green
        $generator = "Visual Studio 17 2022"
        # Try to detect if it's 2019
        $vsVersion = & $vsWhere -latest -property installationVersion
        if ($vsVersion -match "^16\.") {
            $generator = "Visual Studio 16 2019"
        }
    }
} else {
    Write-Host "WARNING: Visual Studio not detected via vswhere." -ForegroundColor Yellow
    $generator = "Visual Studio 17 2022"
}

# Create build directory
$buildDir = Join-Path $PSScriptRoot "build"
if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

Push-Location $buildDir

try {
    # Configure
    Write-Host "[1/3] Configuring with $generator..." -ForegroundColor Yellow
    cmake .. -G $generator -A x64
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: CMake configuration failed." -ForegroundColor Red
        exit 1
    }

    # Build
    Write-Host ""
    Write-Host "[2/3] Building $BuildType..." -ForegroundColor Yellow
    cmake --build . --config $BuildType
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Build failed." -ForegroundColor Red
        exit 1
    }

    Write-Host ""
    Write-Host "[3/3] Build complete!" -ForegroundColor Green
    Write-Host "Output: build\bin\$BuildType\AnyClaw.exe" -ForegroundColor Green

    # Installer
    if ($Installer) {
        Write-Host ""
        Write-Host "Generating installer..." -ForegroundColor Yellow

        $nsis = Get-Command makensis -ErrorAction SilentlyContinue
        if (-not $nsis) {
            Write-Host "WARNING: NSIS not found. Skipping installer." -ForegroundColor Yellow
            Write-Host "Install from https://nsis.sourceforge.io/" -ForegroundColor Yellow
        } else {
            $installerDir = Join-Path $buildDir "installer"
            if (-not (Test-Path $installerDir)) {
                New-Item -ItemType Directory -Path $installerDir | Out-Null
            }

            Push-Location $installerDir
            & makensis /DVERSION=1.0.0 (Join-Path $PSScriptRoot "installer" "installer.nsi")
            if ($LASTEXITCODE -eq 0) {
                Write-Host "Installer: build\AnyClaw-1.0.0-Setup.exe" -ForegroundColor Green
            }
            Pop-Location
        }
    }

} finally {
    Pop-Location
}

Write-Host ""
Write-Host "Done." -ForegroundColor Green
