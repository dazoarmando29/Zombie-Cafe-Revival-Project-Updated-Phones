/* Extra character-sheet support (characterParts3 and beyond).
 *
 * Engine mechanics verified against the shipped libZombieCafeAndroid.so
 * (ELF dynsym + Thumb disassembly, see docs/ASSET_EXPANSION_GUIDE.md §3):
 *
 *  - ZombieCafe::initData loads the two shipped sheets from two unrolled
 *    blocks: each parses data/characterArtN.bin.mid (PiecesPerString byte +
 *    UTF names, empty string terminates), keeps the name list at
 *    ZombieCafe+0x13c/0x15c and calls
 *    PackedImage::createPackedImage(atlas, 0, offsets, 0), storing the
 *    PackedImage* at +0x14c / +0x16c (PiecesPerString at +0x150 / +0x170).
 *  - ZombieCafe::getImageSubpack(String) searches those two name lists and,
 *    on a hit at list index i, returns PackedImage::createSubpack(i*PPS,
 *    PPS) on the matching sheet's PackedImage. The single not-found entry
 *    is RVA 0x125bbe; every path leaves through the shared epilogue at
 *    0x125bcc with the result in r4.
 *  - Per-character art resolution runs through ZombieCafe::loadImagePack
 *    (art, form) which formats "%s-human"/"%s-zombie" and calls
 *    getImageSubpack; the form byte is 0=human, 1=zombie, 0xFF=both.
 *
 * This module therefore only has to (a) create the extra PackedImages the
 * same way the unrolled blocks do and (b) answer getImageSubpack's
 * not-found path from the extra sheets - no engine code is modified.
 */

#include "zcr_sheets.h"

#include <android/asset_manager.h>
#include <android/log.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <unicorn/unicorn.h>

#include "diag.h"
#include "emu_mem.h"
#include "engine_memory.h"
#include "jni_bridge.h"
#include "stub_dispatch.h"

namespace zcr_sheets {
namespace {

/* ---- verified engine RVAs ---- */
constexpr uint32_t RVA_SHEET2_DONE = 0x12c5ba; /* insn right after sheet-2 block */
constexpr uint32_t RVA_GS_MISS = 0x125bbe; /* getImageSubpack not-found head */
constexpr uint32_t RVA_GS_EPI = 0x125bcc; /* getImageSubpack shared epilogue */
constexpr uint32_t RVA_RELOAD_TEX = 0x12b1ad; /* ZombieCafe::ReloadTextures() */
constexpr uint32_t RVA_CREATE_PACKED = 0x140478; /* PackedImage::createPackedImage(String,int,String,int) */
constexpr uint32_t RVA_CREATE_SUBPACK = 0x13fee4; /* PackedImage::createSubpack(int,int) const */
constexpr uint32_t RVA_STRING_CTOR = 0x141f1c; /* String::String(const char*) */

/* ---- verified ZombieCafe / PackedImage fields ---- */
constexpr uint32_t ZC_S1_LIST = 0x13c; /* sheet-1 art list head (sanity) */
constexpr uint32_t ZC_S2_PI = 0x16c; /* sheet-2 PackedImage* */
constexpr uint32_t PI_SCALE = 0x18; /* device scale, set after createPackedImage */
constexpr uint32_t PI_SCALE_INV = 0x1c; /* inverse scale */

constexpr int kMaxSheet = 12;

struct Sheet {
    int number = 0;
    uint8_t pps = 0;
    uint32_t packed_image = 0;
    uint32_t atlas_str = 0, offsets_str = 0; /* persistent engine Strings */
    std::string art_file, atlas_file, offsets_file;
    std::vector<std::string> names;
};

std::mutex g_mu;
std::vector<Sheet> g_sheets;
bool g_loaded = false;
bool g_loading = false;
uc_engine *g_uc = nullptr;
AAssetManager *g_amgr = nullptr;

void logi(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    __android_log_print(ANDROID_LOG_INFO, "zcr-sheet", "%s", buf);
    diag_log("[sheet] %s", buf);
}

/* ---- asset access (pre-scan only; runtime loads go through the engine) ---- */

bool read_asset(const char *path, std::string *out) {
    AAsset *a = AAssetManager_open(g_amgr, path, AASSET_MODE_STREAMING);
    if (!a) return false;
    char tmp[4096];
    out->clear();
    for (;;) {
        int n = AAsset_read(a, tmp, sizeof(tmp));
        if (n <= 0) break;
        out->append(tmp, (size_t)n);
    }
    AAsset_close(a);
    return !out->empty();
}

bool asset_exists(const char *path) {
    AAsset *a = AAssetManager_open(g_amgr, path, AASSET_MODE_STREAMING);
    if (a) {
        AAsset_close(a);
        return true;
    }
    return false;
}

/* characterArtN.bin.mid: byte PiecesPerString, then modified-UTF strings
 * (int16 big-endian length + bytes) terminated by an empty string. */
bool parse_art_list(const std::string &data, Sheet *out) {
    if (data.size() < 3) return false;
    size_t pos = 0;
    out->pps = (uint8_t)data[pos++];
    for (;;) {
        if (pos + 2 > data.size()) return false;
        int len = ((uint8_t)data[pos] << 8) | (uint8_t)data[pos + 1];
        pos += 2;
        if (len < 0 || pos + (size_t)len > data.size()) return false;
        if (len == 0) break;
        out->names.emplace_back(data.data() + pos, (size_t)len);
        pos += (size_t)len;
    }
    return !out->names.empty();
}

void discover() {
    static const char *kFamilies[] = {"images/", "", "iphone4/", "iphone3/"};
    for (int n = 3; n <= kMaxSheet; n++) {
        char art[80], atlas[80], offs[80];
        snprintf(art, sizeof(art), "data/characterArt%d.bin.mid", n);
        if (!asset_exists(art))
            snprintf(art, sizeof(art), "characterArt%d.bin.mid", n);
        std::string buf;
        if (!read_asset(art, &buf)) continue;

        Sheet s;
        s.number = n;
        s.art_file = art;
        if (!parse_art_list(buf, &s)) {
            logi("sheet %d: %s failed to parse (len=%zu)", n, art, buf.size());
            continue;
        }
        bool ok = false;
        for (const char *fam : kFamilies) {
            snprintf(atlas, sizeof(atlas), "%scharacterParts%d.cct.mid", fam, n);
            snprintf(offs, sizeof(offs), "%scharacterOffsets%d.bin.mid", fam, n);
            if (asset_exists(atlas) && asset_exists(offs)) {
                s.atlas_file = atlas;
                s.offsets_file = offs;
                ok = true;
                break;
            }
        }
        if (!ok) {
            logi("sheet %d: art list found but no characterParts%d.cct.mid/"
                 "characterOffsets%d.bin.mid pair",
                 n, n, n);
            continue;
        }
        logi("sheet %d discovered: %zu names pps=%u atlas=%s", n, s.names.size(),
             s.pps, s.atlas_file.c_str());
        g_sheets.push_back(std::move(s));
    }
}

/* ---- register snapshot around nested engine calls ---- */

struct Regs {
    uint32_t r[16] = {0}; /* r0..r12, sp, lr, pc */
    uint32_t cpsr = 0;

    void save(uc_engine *uc) {
        static const int kIds[16] = {
            UC_ARM_REG_R0,  UC_ARM_REG_R1,  UC_ARM_REG_R2,  UC_ARM_REG_R3,
            UC_ARM_REG_R4,  UC_ARM_REG_R5,  UC_ARM_REG_R6,  UC_ARM_REG_R7,
            UC_ARM_REG_R8,  UC_ARM_REG_R9,  UC_ARM_REG_R10, UC_ARM_REG_R11,
            UC_ARM_REG_R12, UC_ARM_REG_SP,  UC_ARM_REG_LR,  UC_ARM_REG_PC};
        for (int i = 0; i < 16; i++) uc_reg_read(uc, kIds[i], &r[i]);
        uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    }

    /* Restore all registers; skip_r4/leave_pc allow deliberate overrides.
     * CPSR is restored before PC and the PC gets the Thumb bit (bit 0):
     * writing an even PC selects ARM mode and the engine's Thumb code would
     * decode as garbage (verified regression: UC_ERR_INSN_INVALID at the
     * resume address). */
    void restore(uc_engine *uc, bool skip_r4 = false, bool leave_pc = false) const {
        static const int kIds[16] = {
            UC_ARM_REG_R0,  UC_ARM_REG_R1,  UC_ARM_REG_R2,  UC_ARM_REG_R3,
            UC_ARM_REG_R4,  UC_ARM_REG_R5,  UC_ARM_REG_R6,  UC_ARM_REG_R7,
            UC_ARM_REG_R8,  UC_ARM_REG_R9,  UC_ARM_REG_R10, UC_ARM_REG_R11,
            UC_ARM_REG_R12, UC_ARM_REG_SP,  UC_ARM_REG_LR,  UC_ARM_REG_PC};
        uc_reg_write(uc, UC_ARM_REG_CPSR, &cpsr);
        for (int i = 0; i < 16; i++) {
            if (skip_r4 && i == 4) continue;
            if (leave_pc && i == 15) continue;
            uint32_t v = r[i];
            if (i == 15) v |= 1u; /* Thumb */
            uc_reg_write(uc, kIds[i], &v);
        }
    }
};

/* Nested stack frame below the interrupted engine frame (its SP points at
 * the bottom of the live frame; everything below is free). */
uint32_t nested_sp(const Regs &snap) { return (snap.r[13] - 256u) & ~7u; }

/* ---- engine calls (RVA | 1 selects Thumb - engine export RVAs carry the
 * Thumb bit and uc_emu_start picks the mode from PC bit 0) ----
 * String construction: String = {StringRef* ref; int16 start; int16 len}
 * with ref->data the char buffer (verified from String::String/getBytes). */

uint32_t make_string(uc_engine *uc, uint32_t sp, const std::string &s) {
    uint32_t obj = emu::heap_alloc_tagged(uc, 8, "zcr-sheet");
    uint32_t cbuf = emu::heap_alloc_tagged(uc, s.size() + 1, "zcr-sheet");
    emu::write(uc, cbuf, s.c_str(), s.size() + 1);
    uint32_t words[3] = {cbuf, 0, 0};
    jni_call_engine_r0_sp(uc, RVA_STRING_CTOR | 1u, obj, words, 1, sp);
    emu::heap_free(uc, cbuf);
    return obj;
}

bool read_string(uc_engine *uc, uint32_t str_addr, std::string *out) {
    out->clear();
    if (!str_addr) return false;
    uint32_t ref = 0;
    uint16_t hl[2] = {0, 0};
    if (!emu::read(uc, str_addr, &ref, 4)) return false;
    if (!emu::read(uc, str_addr + 4, hl, 4)) return false;
    uint16_t start = hl[0], len = hl[1];
    if (len == 0) return true;
    if (len > 1024 || ref == 0) return false;
    uint32_t data = 0;
    if (!emu::read(uc, ref, &data, 4) || !data) return false;
    std::vector<char> buf(len + 1, 0);
    if (!emu::read(uc, data + start, buf.data(), len)) return false;
    out->assign(buf.data(), len);
    return true;
}

/* ---- engine calls ---- */

uint32_t create_packed(uc_engine *uc, uint32_t sp, uint32_t atlas_str,
                       uint32_t offs_str) {
    uint32_t words[3] = {0, offs_str, 0};
    return jni_call_engine_r0_sp(uc, RVA_CREATE_PACKED | 1u, atlas_str, words,
                                 3, sp);
}

uint32_t create_subpack(uc_engine *uc, uint32_t sp, uint32_t pi, int idx,
                        int pps) {
    uint32_t words[2] = {(uint32_t)(idx * pps), (uint32_t)pps};
    return jni_call_engine_r0_sp(uc, RVA_CREATE_SUBPACK | 1u, pi, words, 2, sp);
}

/* Engine pointers live in the emulated heap (0x70000000..0xD0000000). */
bool plausible_ptr(uint32_t p) {
    return p >= HEAP_BASE && p < HEAP_BASE + HEAP_SIZE && (p & 3u) == 0;
}

/* ---- hook: run right after the engine loaded sheet 2 ---- */

void on_sheet2_done(uc_engine *uc, uint64_t, int32_t, void *) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_loaded || g_loading) return;
    if (g_sheets.empty()) return;
    g_loading = true;

    Regs snap;
    snap.save(uc);
    uint32_t sp = nested_sp(snap);

    uint32_t this_obj = snap.r[5]; /* r5 = ZombieCafe* through initData */
    uint32_t s1_list = 0, pi2 = 0, scale = 0, scale_inv = 0;
    emu::read(uc, this_obj + ZC_S1_LIST, &s1_list, 4);
    emu::read(uc, this_obj + ZC_S2_PI, &pi2, 4);
    if (pi2) {
        emu::read(uc, pi2 + PI_SCALE, &scale, 4);
        emu::read(uc, pi2 + PI_SCALE_INV, &scale_inv, 4);
    }
    logi("sheet2 done: this=0x%x list1=0x%x pi2=0x%x scale=%08x/%08x",
         this_obj, s1_list, pi2, scale, scale_inv);

    if (!s1_list || !pi2) {
        /* unexpected engine state: leave everything alone */
        logi("unexpected state, extra sheets not loaded");
        snap.restore(uc);
        g_loading = false;
        return;
    }

    for (Sheet &s : g_sheets) {
        if (!s.atlas_str) s.atlas_str = make_string(uc, sp, s.atlas_file);
        if (!s.offsets_str) s.offsets_str = make_string(uc, sp, s.offsets_file);
        s.packed_image = create_packed(uc, sp, s.atlas_str, s.offsets_str);
        if (!plausible_ptr(s.packed_image)) {
            logi("sheet %d FAILED to load (%s / %s) ret=0x%x", s.number,
                 s.atlas_file.c_str(), s.offsets_file.c_str(), s.packed_image);
            s.packed_image = 0;
            continue;
        }
        emu::write(uc, s.packed_image + PI_SCALE, &scale, 4);
        emu::write(uc, s.packed_image + PI_SCALE_INV, &scale_inv, 4);
        logi("sheet %d loaded: names=%zu pps=%u pi=0x%x", s.number,
             s.names.size(), s.pps, s.packed_image);
    }

    g_loaded = true;
    snap.restore(uc);
    g_loading = false;
}

/* ---- hook: ZombieCafe::ReloadTextures() entry (GL context loss) ----
 * The engine re-creates sheets 1-2 there and re-resolves per-character art
 * through loadImagePack; the extra sheets must be re-created first so those
 * resolutions bind to fresh PackedImages (old ones keep stale textures).
 * The superseded PackedImage objects are abandoned on the guest heap (a few
 * KB per context loss). */

void on_reload_textures(uc_engine *uc, uint64_t, int32_t, void *) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_loaded || g_loading || g_sheets.empty()) return;
    g_loading = true;

    Regs snap;
    snap.save(uc);
    uint32_t sp = nested_sp(snap);

    uint32_t pi2 = 0, scale = 0, scale_inv = 0;
    emu::read(uc, snap.r[0] + ZC_S2_PI, &pi2, 4); /* entry: r0 = ZombieCafe* */
    if (pi2) {
        emu::read(uc, pi2 + PI_SCALE, &scale, 4);
        emu::read(uc, pi2 + PI_SCALE_INV, &scale_inv, 4);
    }
    for (Sheet &s : g_sheets) {
        uint32_t old = s.packed_image;
        s.packed_image = create_packed(uc, sp, s.atlas_str, s.offsets_str);
        if (!plausible_ptr(s.packed_image)) {
            logi("sheet %d reload FAILED ret=0x%x", s.number, s.packed_image);
            s.packed_image = old;
            continue;
        }
        emu::write(uc, s.packed_image + PI_SCALE, &scale, 4);
        emu::write(uc, s.packed_image + PI_SCALE_INV, &scale_inv, 4);
        logi("sheet %d reloaded: old_pi=0x%x new_pi=0x%x", s.number, old,
             s.packed_image);
    }

    snap.restore(uc);
    g_loading = false;
}

/* ---- hook: getImageSubpack() not found in sheets 1-2 ---- */

void on_gs_miss(uc_engine *uc, uint64_t, int32_t, void *) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_loaded || g_loading || g_sheets.empty()) return;

    Regs snap;
    snap.save(uc);

    /* fp (r11) holds the requested name String* at the not-found entry */
    std::string name;
    if (!read_string(uc, snap.r[11], &name) || name.empty()) {
        snap.restore(uc);
        return;
    }

    for (Sheet &s : g_sheets) {
        if (!s.packed_image) continue;
        for (size_t i = 0; i < s.names.size(); i++) {
            if (s.names[i] != name) continue;
            uint32_t sub = create_subpack(uc, nested_sp(snap), s.packed_image,
                                          (int)i, s.pps);
            if (!plausible_ptr(sub)) {
                logi("createSubpack failed for %s (sheet %d idx=%zu ret=0x%x)",
                     name.c_str(), s.number, i, sub);
                snap.restore(uc);
                return;
            }
            logi("resolved %s via sheet %d idx=%zu sub=0x%x", name.c_str(),
                 s.number, i, sub);
            /* skip the engine's failure log and its r4=0 result: restore the
             * interrupted state but return the subpack through r4 (the shared
             * epilogue moves r4 into r0 before popping) */
            snap.restore(uc, /*skip_r4=*/true, /*leave_pc=*/true);
            uint32_t r4 = sub;
            uc_reg_write(uc, UC_ARM_REG_R4, &r4);
            emu_set_pc(uc, ENG_ADDR(RVA_GS_EPI) | 1u);
            return;
        }
    }
    /* genuinely unknown art: let the engine log + return null */
    snap.restore(uc);
}

}  // namespace

bool init(uc_engine *uc, void *asset_mgr) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_uc = uc;
    g_amgr = (AAssetManager *)asset_mgr;
    if (!g_amgr) {
        logi("no AAssetManager, extra sheets disabled");
        return false;
    }
    discover();
    if (g_sheets.empty()) {
        logi("no extra character sheets found (characterParts3+)");
        return true;
    }

    uc_hook h1 = 0, h2 = 0, h3 = 0;
    uc_err e1 = uc_hook_add(uc, &h1, UC_HOOK_CODE, (void *)on_sheet2_done,
                            nullptr, ENG_ADDR(RVA_SHEET2_DONE),
                            ENG_ADDR(RVA_SHEET2_DONE));
    uc_err e2 = uc_hook_add(uc, &h2, UC_HOOK_CODE, (void *)on_gs_miss, nullptr,
                            ENG_ADDR(RVA_GS_MISS), ENG_ADDR(RVA_GS_MISS));
    uc_err e3 = uc_hook_add(uc, &h3, UC_HOOK_CODE, (void *)on_reload_textures,
                            nullptr, ENG_ADDR(RVA_RELOAD_TEX),
                            ENG_ADDR(RVA_RELOAD_TEX));
    if (e1 != UC_ERR_OK || e2 != UC_ERR_OK || e3 != UC_ERR_OK) {
        logi("hook add failed: %s / %s / %s", uc_strerror(e1),
             uc_strerror(e2), uc_strerror(e3));
        return false;
    }
    logi("hooks installed (sheet2done@0x%x gsmiss@0x%x reload@0x%x), %zu "
         "sheet(s) pending",
         RVA_SHEET2_DONE, RVA_GS_MISS, RVA_RELOAD_TEX, g_sheets.size());
    return true;
}

bool loaded() {
    std::lock_guard<std::mutex> lk(g_mu);
    return g_loaded;
}

}  // namespace zcr_sheets
