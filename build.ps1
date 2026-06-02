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
cmake -B $BuildDir `
      -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_TOOLCHAIN_FILE="$VcpkgDir\scripts\buildsystems\vcpkg.cmake" `
      -DVCPKG_TARGET_TRIPLET=x64-windows-static `
      -S $Root

# --- 3. Build ---
Write-Host "[*] Building..." -ForegroundColor Cyan
cmake --build $BuildDir --config Release

Write-Host ""
Write-Host "[+] Done! Binary: $BuildDir\Release\GoatedBypass.exe" -ForegroundColor Green
