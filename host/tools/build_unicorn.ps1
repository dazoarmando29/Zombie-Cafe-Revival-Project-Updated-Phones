# Build Unicorn Engine (ARM32 emulation core) for Android arm64-v8a.
#
# Unicorn's bundled qemu/configure cannot run from paths containing spaces,
# so this script keeps the unicorn checkout + build under C:\Users\Dazzo\tools\uc
# and the host CMakeLists links the resulting libunicorn.a.
#
# Requires: git, cmake, ninja, Android NDK, sh + pkg-config + strings (scoop).
param(
    [string]$UnicornVersion = "2.1.4",
    [string]$UcRoot = "C:\Users\Dazzo\tools\uc",
    [string]$NdkRoot = "C:\Users\Dazzo\tools\android-sdk\ndk\27.2.12479018",
    [string]$Abi = "arm64-v8a"
)

$ErrorActionPreference = "Stop"
$Cmake = "C:\Program Files\CMake\bin\cmake.exe"
$Ninja = "C:\Users\Dazzo\AppData\Local\Microsoft\WinGet\Links\ninja.exe"

if (-not (Test-Path "$UcRoot\src\CMakeLists.txt")) {
    git clone --depth 1 --branch $UnicornVersion `
        https://github.com/unicorn-engine/unicorn.git "$UcRoot\src"
}

# NDK clang wrapper for qemu/configure's test compiles (needs --target baked in).
$Clang = "$NdkRoot\toolchains\llvm\prebuilt\windows-x86_64\bin\clang.exe"
$Sysroot = "$NdkRoot\toolchains\llvm\prebuilt\windows-x86_64\sysroot"
@(
    "#!/bin/sh",
    "exec `"$Clang`" --target=aarch64-none-linux-android21 --sysroot=`"$Sysroot`" `"`$@`""
) | Set-Content -Encoding ASCII "$UcRoot\clang-android.sh"

# Patch 1: qemu/configure runs through this wrapper (UCONFIG_CC).
$cm = "$UcRoot\src\CMakeLists.txt"
$c = [System.IO.File]::ReadAllText($cm)
if ($c -notmatch "UCONFIG_CC") {
    $c = $c.Replace(
        "execute_process(COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/qemu/configure`n        --cc=${CMAKE_C_COMPILER}",
        "if(NOT DEFINED UCONFIG_CC)`n        set(UCONFIG_CC ${CMAKE_C_COMPILER})`n    endif()`n    execute_process(COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/qemu/configure`n        --cc=${UCONFIG_CC}")
    [System.IO.File]::WriteAllText($cm, $c)
}

# Patch 2: int128.h typedef clash with the NDK's native __int128_t.
$ih = "$UcRoot\src\qemu\include\qemu\int128.h"
$c = [System.IO.File]::ReadAllText($ih)
if ($c -notmatch "NDK defines __int128_t") {
    $c = $c.Replace(
        "#if !(defined(_MSC_VER) && defined(__clang__))",
        "#if 0 /* NDK defines __int128_t natively; redefinition is an error */")
    [System.IO.File]::WriteAllText($ih, $c)
}

$Cflags = "-Wno-implicit-function-declaration -Wno-error=implicit-function-declaration -Wno-implicit-int -Wno-int-conversion"
$Build = "$UcRoot\build-arm64"

& $Cmake -S "$UcRoot\src" -B $Build -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$Ninja" `
    "-DCMAKE_TOOLCHAIN_FILE=$NdkRoot\build\cmake\android.toolchain.cmake" `
    "-DANDROID_ABI=$Abi" `
    "-DANDROID_PLATFORM=android-21" `
    "-DCMAKE_BUILD_TYPE=Release" `
    "-DUNICORN_ARCH=arm" `
    "-DUNICORN_BUILD_TESTS=OFF" `
    "-DCMAKE_C_FLAGS=$Cflags" `
    "-DUCONFIG_CC=$UcRoot/clang-android.sh"
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

& $Cmake --build $Build
if ($LASTEXITCODE -ne 0) { throw "unicorn build failed" }

Write-Host "OK: $Build\libunicorn.a" -ForegroundColor Green
