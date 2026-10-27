<#
.SYNOPSIS
  Zombie Cafe Revival - Updated Phones. Windows build pipeline.

  Produces a universal APK:
    lib/armeabi-v7a/  original 32-bit engine + original extension (native path)
    lib/arm64-v8a/    zcr_host emulating the engine + extension stub (64-bit path)
    assets/engine/    the 32-bit engine image loaded by the emulator host

.EXAMPLE
  .\build.ps1              # build APK
  .\build.ps1 -Install     # build + adb install (bypasses low-target-sdk block)
  .\build.ps1 -Clean       # remove build output first
#>
param(
    [switch]$Install,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------- tool paths
$Go         = "C:\Users\Dazzo\tools\go-sdk\bin\go.exe"
$Java       = "C:\Program Files\Microsoft\jdk-17.0.20.101-hotspot\bin\java.exe"
$ApktoolJar = "C:\Users\Dazzo\tools\apktool\apktool.jar"
$NdkRoot    = "C:\Users\Dazzo\tools\android-sdk\ndk\27.2.12479018"
$BuildTools = "C:\Users\Dazzo\tools\android-sdk\build-tools\35.0.0"
$Adb        = "C:\Users\Dazzo\tools\android-sdk\platform-tools\adb.exe"
$Cmake      = "C:\Program Files\CMake\bin\cmake.exe"
$Ninja      = "C:\Users\Dazzo\AppData\Local\Microsoft\WinGet\Links\ninja.exe"

$ProjectRoot = $PSScriptRoot
$BuildDir    = Join-Path $ProjectRoot "build"
$OutUnsigned = Join-Path $BuildDir "out\out-unsigned.apk"
$OutAligned  = Join-Path $BuildDir "out\out-aligned.apk"
$OutApk      = Join-Path $BuildDir "out\out.apk"

foreach ($t in @($Go, $Java, $ApktoolJar, $NdkRoot, $BuildTools, $Adb, $Cmake, $Ninja)) {
    if (-not (Test-Path $t)) { throw "Missing tool: $t" }
}

Push-Location $ProjectRoot
try {
    if ($Clean -and (Test-Path $BuildDir)) {
        Write-Host "[clean] removing $BuildDir" -ForegroundColor DarkGray
        Remove-Item -Recurse -Force $BuildDir
    }

    # ------------------------------------------------ 1. pack game assets
    Write-Host "[1/7] Go build_tool: packing assets..." -ForegroundColor Cyan
    & $Go run ./tool/build_tool/ -i src/ -o build/
    if ($LASTEXITCODE -ne 0) { throw "build_tool failed" }

    # ------------------------------------------------ 2. 32-bit extension
    Write-Host "[2/7] CMake/NDK: libZombieCafeExtension (armeabi-v7a)..." -ForegroundColor Cyan
    $ExtBuild = Join-Path $ProjectRoot "src\lib\cpp\build"
    & $Cmake -S (Join-Path $ProjectRoot "src\lib\cpp") -B $ExtBuild -G Ninja `
        "-DCMAKE_MAKE_PROGRAM=$Ninja" `
        "-DCMAKE_TOOLCHAIN_FILE=$NdkRoot\build\cmake\android.toolchain.cmake" `
        "-DANDROID_ABI=armeabi-v7a" `
        "-DANDROID_PLATFORM=android-21" `
        "-DCMAKE_BUILD_TYPE=Release"
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed (32-bit ext)" }
    & $Cmake --build $ExtBuild
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed (32-bit ext)" }

    # ------------------------------------------------ 3. arm64 host + stub
    Write-Host "[3/7] CMake/NDK: zcr_host + extension stub (arm64-v8a)..." -ForegroundColor Cyan
    $HostBuild = Join-Path $ProjectRoot "host\build-arm64"
    & $Cmake -S (Join-Path $ProjectRoot "host") -B $HostBuild -G Ninja `
        "-DCMAKE_MAKE_PROGRAM=$Ninja" `
        "-DCMAKE_TOOLCHAIN_FILE=$NdkRoot\build\cmake\android.toolchain.cmake" `
        "-DANDROID_ABI=arm64-v8a" `
        "-DANDROID_PLATFORM=android-21" `
        "-DCMAKE_BUILD_TYPE=Release"
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed (host)" }
    & $Cmake --build $HostBuild
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed (host)" }

    # ------------------------------------------------ 4. stage libs + engine
    Write-Host "[4/7] staging ABI folders + engine asset..." -ForegroundColor Cyan

    $Strip = Join-Path $NdkRoot "toolchains\llvm\prebuilt\windows-x86_64\bin\llvm-strip.exe"
    & $Strip --strip-unneeded (Join-Path $HostBuild "libzcr_host.so")

    $Engine = Join-Path $ProjectRoot "src\lib\armeabi\libZombieCafeAndroid.so"

    # drop the copylist's legacy armeabi output (armeabi-v7a covers all32-bit)
    Remove-Item -Recurse -Force (Join-Path $BuildDir "lib\armeabi") -ErrorAction SilentlyContinue

    # 32-bit native path (original engine + original extension)
    $V7 = Join-Path $BuildDir "lib\armeabi-v7a"
    New-Item -ItemType Directory -Force -Path $V7 | Out-Null
    Copy-Item -Force $Engine (Join-Path $V7 "libZombieCafeAndroid.so")
    Copy-Item -Force (Join-Path $ExtBuild "libZombieCafeExtension.so") `
        (Join-Path $V7 "libZombieCafeExtension.so")

    # arm64 emulation path (host keeps the engine's soname)
    $V8 = Join-Path $BuildDir "lib\arm64-v8a"
    New-Item -ItemType Directory -Force -Path $V8 | Out-Null
    Copy-Item -Force (Join-Path $HostBuild "libzcr_host.so") `
        (Join-Path $V8 "libZombieCafeAndroid.so")
    Copy-Item -Force (Join-Path $HostBuild "libZombieCafeExtension.so") `
        (Join-Path $V8 "libZombieCafeExtension.so")

    # engine image consumed by zcr_host
    $EngDir = Join-Path $BuildDir "assets\engine"
    New-Item -ItemType Directory -Force -Path $EngDir | Out-Null
    Copy-Item -Force $Engine (Join-Path $EngDir "game.so")

    # ------------------------------------------------ 5. apktool rebuild
    Write-Host "[5/7] apktool: rebuilding APK..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Force -Path (Split-Path $OutUnsigned) | Out-Null
    & $Java -jar $ApktoolJar b build -o $OutUnsigned
    if ($LASTEXITCODE -ne 0) { throw "apktool failed" }

    # ------------------------------------------------ 6. zipalign (16 KB)
    Write-Host "[6/7] zipalign -P 16 (16 KB page size)..." -ForegroundColor Cyan
    & (Join-Path $BuildTools "zipalign.exe") -f -P 16 4 $OutUnsigned $OutAligned
    if ($LASTEXITCODE -ne 0) { throw "zipalign failed" }

    # ------------------------------------------------ 7. sign
    Write-Host "[7/7] apksigner: signing with debug.keystore..." -ForegroundColor Cyan
    & (Join-Path $BuildTools "apksigner.bat") sign `
        --ks debug.keystore `
        --ks-pass pass:zombiecafe `
        --ks-key-alias alias_name `
        --key-pass pass:zombiecafe `
        --out $OutApk `
        $OutAligned
    if ($LASTEXITCODE -ne 0) { throw "apksigner failed" }
    & (Join-Path $BuildTools "apksigner.bat") verify --print-certs $OutApk | Select-Object -First 3

    $size = [math]::Round((Get-Item $OutApk).Length / 1MB, 1)
    Write-Host "`nOK: $OutApk ($size MB)" -ForegroundColor Green

    if ($Install) {
        Write-Host "[install] adb install (bypass low-target-sdk block)..." -ForegroundColor Cyan
        & $Adb install -r --bypass-low-target-sdk-block $OutApk
        if ($LASTEXITCODE -ne 0) { throw "adb install failed" }
    }
}
finally {
    Pop-Location
}
