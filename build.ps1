# GoatedBypass build script
# Usage: .\build.ps1

$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot

# --- 1. Install vcpkg if missing ---
$VcpkgDir = "$Root\vcpkg"
$VcpkgExe = "$VcpkgDir\vcpkg.exe"

if (-not (Test-Path $VcpkgExe)) {
    Write-Host "[*] Installing vcpkg..." -ForegroundColor Cyan
    git clone https://github.com/microsoft/vcpkg.git $VcpkgDir
    & "$VcpkgDir\bootstrap-vcpkg.bat" -disableMetrics
}

# --- 2. Configure with CMake ---
Write-Host "[*] Configuring CMake..." -ForegroundColor Cyan
$BuildDir = "$Root\build"

# Clean stale cache to avoid wrong VS instance being used
if (Test-Path "$BuildDir\CMakeCache.txt") {
    Remove-Item "$BuildDir\CMakeCache.txt" -Force
}

cmake -B $BuildDir `
      -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_TOOLCHAIN_FILE="$VcpkgDir\scripts\buildsystems\vcpkg.cmake" `
      -DVCPKG_TARGET_TRIPLET=x64-windows-static `
      -S $Root

# --- 3. Build ---
Write-Host "[*] Building..." -ForegroundColor Cyan
cmake --build $BuildDir --config Release

Write-Host ""
Write-Host "[+] Done! Binary: $BuildDir\Release\GoatedBypass.exe" -ForegroundColor Green
