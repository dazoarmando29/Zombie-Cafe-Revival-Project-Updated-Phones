<#
.SYNOPSIS
  Build and run the emu::heap unit tests on a connected Android device.

.DESCRIPTION
  Compiles host/emu_heap.cpp + host/tests/heap_test.cpp with the NDK clang
  into a self-contained arm64 binary, pushes it to /data/local/tmp and runs
  it via adb. The same allocator code runs inside the emulator host.

.EXAMPLE
  .\host\tests\run_heap_tests.ps1
#>
param(
    [string]$Adb = "C:\Users\Dazzo\tools\android-sdk\platform-tools\adb.exe",
    [string]$NdkRoot = "C:\Users\Dazzo\tools\android-sdk\ndk\27.2.12479018"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not (Test-Path (Join-Path $ProjectRoot "host\emu_heap.cpp"))) {
    $ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
}

$Clang = Join-Path $NdkRoot "toolchains\llvm\prebuilt\windows-x86_64\bin\clang++.exe"
if (-not (Test-Path $Clang)) { throw "NDK clang++ not found: $Clang" }

$Out = Join-Path $env:TEMP "zcr_heap_test"
Write-Host "[build] $Out" -ForegroundColor Cyan
& $Clang --target=aarch64-linux-android21 -static-libstdc++ -O1 -g `
    -Wall -Wextra -Wno-unused-parameter `
    (Join-Path $ProjectRoot "host\emu_heap.cpp") `
    (Join-Path $ProjectRoot "host\tests\heap_test.cpp") `
    -o $Out
if ($LASTEXITCODE -ne 0) { throw "compile failed" }

Write-Host "[push] -> /data/local/tmp/zcr_heap_test" -ForegroundColor Cyan
& $Adb push $Out /data/local/tmp/zcr_heap_test | Out-Null
& $Adb shell "chmod 755 /data/local/tmp/zcr_heap_test"

Write-Host "[run]" -ForegroundColor Cyan
& $Adb shell /data/local/tmp/zcr_heap_test
if ($LASTEXITCODE -ne 0) { throw "heap tests FAILED (exit $LASTEXITCODE)" }
Write-Host "heap tests PASSED" -ForegroundColor Green
