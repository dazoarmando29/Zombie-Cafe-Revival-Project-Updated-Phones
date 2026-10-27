cmake_minimum_required(VERSION 3.18.1)

project(zcr_host C CXX)

set(CMAKE_CXX_STANDARD 17)

# ---- Unicorn Engine (ARM32 emulator core) ----
# Prebuilt for arm64-v8a in a space-free dir (unicorn's qemu configure
# cannot handle spaces in paths). Build it with host\tools\build_unicorn.ps1.
set(UNICORN_ROOT "C:/Users/Dazzo/tools/uc" CACHE PATH "Unicorn install root")
add_library(unicorn STATIC IMPORTED)
set_target_properties(unicorn PROPERTIES
  IMPORTED_LOCATION "${UNICORN_ROOT}/build-arm64/libunicorn.a"
  INTERFACE_INCLUDE_DIRECTORIES "${UNICORN_ROOT}/src/include"
)

add_library(zcr_host SHARED
  zcr_host.cpp
  engine_loader.cpp
  stub_dispatch.cpp
  jni_bridge.cpp
)

target_include_directories(zcr_host PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(zcr_host
  unicorn
  android
  log
  z
)
