<#
.SYNOPSIS
  Build and run the companion-save unit tests on a connected Android device.

.DESCRIPTION
  Compiles host/companion_state.cpp + host/tests/companion_test.cpp with the
  NDK clang into a self-contained arm64 binary, pushes it to /data/local/tmp
  and runs it via adb. The same state code runs inside the emulator host.

.EXAMPLE
  .\host\tests\run_companion_tests.ps1
#>
param(
    [string]$Adb = "C:\Users\Dazzo\tools\android-sdk\platform-tools\adb.exe",
    [string]$NdkRoot = "C:\Users\Dazzo\tools\android-sdk\ndk\27.2.12479018"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not (Test-Path (Join-Path $ProjectRoot "host\companion_state.cpp"))) {
    $ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
}

$Clang = Join-Path $NdkRoot "toolchains\llvm\prebuilt\windows-x86_64\bin\clang++.exe"
if (-not (Test-Path $Clang)) { throw "NDK clang++ not found: $Clang" }

$Out = Join-Path $env:TEMP "zcr_companion_test"
Write-Host "[build] $Out" -ForegroundColor Cyan
& $Clang --target=aarch64-linux-android21 -static-libstdc++ -O1 -g `
    -Wall -Wextra -Wno-unused-parameter `
(Join-Path $ProjectRoot "host\companion_state.cpp") `
(Join-Path $ProjectRoot "host\event_state.cpp") `
(Join-Path $ProjectRoot "host\tests\companion_test.cpp") `
    -o $Out
if ($LASTEXITCODE -ne 0) { throw "compile failed" }

Write-Host "[push] -> /data/local/tmp/zcr_companion_test" -ForegroundColor Cyan
& $Adb push $Out /data/local/tmp/zcr_companion_test | Out-Null
& $Adb shell "chmod 755 /data/local/tmp/zcr_companion_test"

Write-Host "[run]" -ForegroundColor Cyan
& $Adb shell /data/local/tmp/zcr_companion_test
if ($LASTEXITCODE -ne 0) { throw "companion tests FAILED (exit $LASTEXITCODE)" }
Write-Host "companion tests PASSED" -ForegroundColor Green
