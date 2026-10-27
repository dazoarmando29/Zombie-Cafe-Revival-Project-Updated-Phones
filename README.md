![test](/src/assets/images/banner.png)
# Zombie Cafe Revival — Updated Phones / 64-bit

A fork of [Airyzz/zombie-cafe-revival](https://github.com/Airyzz/zombie-cafe-revival) focused on making Zombie Cafe run on modern Android phones (Android 14/15+, including 64-bit-only devices such as Pixel 7+), while preserving the original game as closely as possible.

All reverse engineering credit belongs to **Airyzz** — see the [revival article](https://airyz.xyz/p/zombie-cafe-revival/) for the technical background (smali fixes, `libZombieCafeExtension` runtime patcher, `.cct` texture format, netcode).

## Why this fork exists

The game engine (`src/lib/armeabi/libZombieCafeAndroid.so`) is a closed **32-bit ARM** binary. 64-bit-only phones cannot execute it at all. This fork keeps the original engine byte-for-byte and adds a 64-bit emulation host (Phase 2) so the same game code runs everywhere:

| ABI | `libZombieCafeAndroid.so` | Runs on |
|---|---|---|
| `armeabi-v7a` | original 32-bit engine | older/emulated devices |
| `arm64-v8a` | 64-bit host emulating the 32-bit engine | Pixel 7+, all modern phones |

## Status

- **Phase 0** — Windows build pipeline (`build.ps1`), toolchain setup. [in progress]
- **Phase 1** — Android 14/15 compatibility patches (device-ID crash, offline guard, install flags). [pending]
- **Phase 2** — `host/` arm64 emulation layer (Unicorn-based), JNI/GL bridging. [pending]
- **Phase 3** — Universal APK polish, online services (`tool/server`) optional. [pending]

## Building

### Requirements
 - cmake, ninja
 - go 1.23+
 - apktool (Java 17+)
 - Android NDK (r26+)
 - Android build-tools (apksigner)

### Windows quick start

```powershell
.\build.ps1            # full pipeline: assets -> extension -> apktool -> sign
.\build.ps1 -Install   # + adb install
```

See `build.ps1` for the individual steps (replaces the Linux commands below).

### LibZombieCafeExtension
LibZombieCafeExtension is an extra library that applies runtime patches to the games `libZombieCafeAndroid.so`

```bash
cd src/lib/cpp
mkdir build
cd build
cmake ../ -DCMAKE_TOOLCHAIN_FILE=$NDK_HOME/build/cmake/android.toolchain.cmake  -DANDROID_ABI=armeabi-v7a -DANDROID_PLATFORM=android-8

make
```

### Running tools
Part of the build process for this project is running custom tools to convert human readable file structure in to the games expected file formats. 

```bash
go run ./tool/build_tool/ -i src/ -o build/

cp src/lib/cpp/build/libZombieCafeExtension.so ./build/lib/armeabi/libZombieCafeExtension.so

apktool b ./build -o ./build/out/out.apk

jarsigner -verbose -sigalg SHA1withRSA -digestalg SHA1 -keystore debug.keystore -storepass zombiecafe ./build/out/out.apk alias_name

adb install ./build/out/out.apk
```
