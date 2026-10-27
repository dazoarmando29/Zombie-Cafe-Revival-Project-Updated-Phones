<#
.SYNOPSIS
  Zombie Cafe Revival - Updated Phones. Windows build pipeline.

  Replaces the Linux commands in README.md:
    1. Go build_tool   -> packs human-readable assets into game formats
    2. CMake/NDK       -> builds libZombieCafeExtension.so (armeabi-v7a)
    3. apktool         -> rebuilds the APK
    4. zipalign+apksigner -> aligns and signs with debug.keystore

.EXAMPLE
  .\build.ps1              # build APK
  .\build.ps1 -Install     # build + adb install
  .\build.ps1 -Clean       # remove build output first
#>
param(
    [switch]$Install,
    [switch]$Clean,
    [string]$Abi = "armeabi-v7a"
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
    Write-Host "[1/5] Go build_tool: packing assets..." -ForegroundColor Cyan
    & $Go run ./tool/build_tool/ -i src/ -o build/
    if ($LASTEXITCODE -ne 0) { throw "build_tool failed" }

    # ------------------------------------------------ 2. extension .so
    Write-Host "[2/5] CMake/NDK: building libZombieCafeExtension ($Abi)..." -ForegroundColor Cyan
    $ExtBuild = Join-Path $ProjectRoot "src\lib\cpp\build"
    & $Cmake -S (Join-Path $ProjectRoot "src\lib\cpp") -B $ExtBuild -G Ninja `
        "-DCMAKE_MAKE_PROGRAM=$Ninja" `
        "-DCMAKE_TOOLCHAIN_FILE=$NdkRoot\build\cmake\android.toolchain.cmake" `
        "-DANDROID_ABI=$Abi" `
        "-DANDROID_PLATFORM=android-21" `
        "-DCMAKE_BUILD_TYPE=Release"
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
    & $Cmake --build $ExtBuild
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }

    # NOTE: game loads libs from lib/armeabi/ (original layout); the extension
    # is compiled armeabi-v7a and installed there, same as upstream.
    $ExtSo = Join-Path $ExtBuild "libZombieCafeExtension.so"
    $DestDir = Join-Path $BuildDir "lib\armeabi"
    New-Item -ItemType Directory -Force -Path $DestDir | Out-Null
    Copy-Item -Force $ExtSo (Join-Path $DestDir "libZombieCafeExtension.so")
    Write-Host "      copied -> build\lib\armeabi\libZombieCafeExtension.so" -ForegroundColor DarkGray

    # ------------------------------------------------ 3. apktool rebuild
    Write-Host "[3/5] apktool: rebuilding APK..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Force -Path (Split-Path $OutUnsigned) | Out-Null
    & $Java -jar $ApktoolJar b build -o $OutUnsigned
    if ($LASTEXITCODE -ne 0) { throw "apktool failed" }

    # ------------------------------------------------ 4. zipalign
    Write-Host "[4/5] zipalign..." -ForegroundColor Cyan
    & (Join-Path $BuildTools "zipalign.exe") -f -p 4 $OutUnsigned $OutAligned
    if ($LASTEXITCODE -ne 0) { throw "zipalign failed" }

    # ------------------------------------------------ 5. sign
    Write-Host "[5/5] apksigner: signing with debug.keystore..." -ForegroundColor Cyan
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
