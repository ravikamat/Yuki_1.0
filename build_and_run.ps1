# ============================================================================
# Yuki v1.0 - Build and Run Script
# Usage:  .\build_and_run.ps1
# ============================================================================

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "   Yuki v1.0 - Build and Run" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# -- Step 1: Configure (only if build dir doesn't exist or CMakeCache is missing)
$buildDir = Join-Path $root "build"
$cacheFile = Join-Path $buildDir "CMakeCache.txt"

if (-not (Test-Path $cacheFile)) {
    Write-Host "[1/3] Configuring CMake..." -ForegroundColor Yellow
    cmake -S $root -B $buildDir `
        -DCMAKE_TOOLCHAIN_FILE="C:\vcpkg\scripts\buildsystems\vcpkg.cmake" `
        -DVCPKG_TARGET_TRIPLET=x64-windows `
        -DYUKI_BUILD_TESTS=OFF `
        -Wno-dev
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] CMake configure failed!" -ForegroundColor Red
        exit 1
    }
    Write-Host "[1/3] Configure complete." -ForegroundColor Green
} else {
    Write-Host "[1/3] Build already configured (skipping cmake)." -ForegroundColor Green
}

# -- Step 2: Build
Write-Host ""
Write-Host "[2/3] Building yuki.exe (Release)..." -ForegroundColor Yellow
cmake --build $buildDir --config Release --target yuki
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed!" -ForegroundColor Red
    exit 1
}
Write-Host "[2/3] Build complete." -ForegroundColor Green

# -- Step 3: Run
$exe = Join-Path $buildDir "Release\yuki.exe"
if (-not (Test-Path $exe)) {
    Write-Host "[ERROR] yuki.exe not found at $exe" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "   Launching Yuki..." -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

& $exe
