/* ELF32 loader + runtime patcher for the original 32-bit engine. */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct uc_struct;
typedef struct uc_struct uc_engine;

struct EngineImage {
    std::vector<uint8_t> bytes;   // raw ELF bytes
    uint32_t entry_rva = 0;       // e_entry
};

/* Read engine ELF from APK assets or disk. */
bool engine_load_image(const std::string &path, EngineImage *out);

/* Map the image into emulated memory at ENG_BASE, apply relocations
 * (imported symbols are redirected to stub page addresses). */
bool engine_map(uc_engine *uc, const EngineImage &img);

/* Global constructors discovered during engine_map (DT_INIT +
 * DT_INIT_ARRAY), as emulated code addresses (bit0 = Thumb). Must be
 * executed once before JNI_OnLoad - just like a real dlopen does. */
const std::vector<uint32_t> &engine_init_routines();

/* Apply the ZCR runtime patches (URL strings, crash NOPs, toxin patch) -
 * identical in effect to libZombieCafeExtension's JNI_OnLoad. */
bool engine_apply_zcr_patches(uc_engine *uc);

/* Resolve an engine export name -> emulated address. */
uint32_t engine_export_addr(const EngineImage &img, const char *name);
