/* Extra character-sheet support (characterParts3 and beyond).
 *
 * The engine binary loads exactly two character texture sheets from two
 * unrolled blocks in ZombieCafe::initData and resolves art names through
 * ZombieCafe::getImageSubpack, which searches those two sheets' art-name
 * lists. This module adds host-side loading for any number of extra sheets
 * packed by the (data-driven) asset pipeline, without patching engine code:
 *
 *   - a code hook right after sheet 2's load block runs the same
 *     PackedImage::createPackedImage() call the engine uses, and
 *   - a code hook on getImageSubpack's not-found entry answers lookups from
 *     the extra sheets via PackedImage::createSubpack().
 *
 * All engine offsets below were verified against the shipped
 * libZombieCafeAndroid.so (ELF dynsym + Thumb disassembly); see
 * docs/ASSET_EXPANSION_GUIDE.md section 3 for the full findings.
 */
#pragma once

#include <cstdint>

struct uc_struct;
typedef struct uc_struct uc_engine;

namespace zcr_sheets {

/* Pre-scan APK assets for characterArtN/characterPartsN/characterOffsetsN
 * sets (N = 3..12) and register the engine hooks. Call once from host_init
 * after engine_apply_zcr_patches(); `asset_mgr` is the app's
 * AAssetManager (as void* to keep this header free of NDK headers). */
bool init(uc_engine *uc, void *asset_mgr);

/* True once the extra sheets were loaded at runtime. */
bool loaded();

}  // namespace zcr_sheets
