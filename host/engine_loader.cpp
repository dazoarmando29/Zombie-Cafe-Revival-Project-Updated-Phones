#include "engine_loader.h"

#include <cstring>
#include <cstdio>

#include <unicorn/unicorn.h>

#include "engine_memory.h"
#include "generated/engine_tables.h"
#include "stub_dispatch.h"

/* ---- minimal ELF32 structures (little-endian ARM) ---- */
namespace {

struct Ehdr {
    uint8_t  ident[16];
    uint16_t type, machine;
    uint32_t version, entry, phoff, shoff, flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
};

struct Phdr {
    uint32_t type, offset, vaddr, paddr, filesz, memsz, flags, align;
};

struct Dyn { uint32_t tag, val; };

struct Rel { uint32_t offset, info; };

struct Sym {
    uint32_t name, value, size;
    uint8_t  info, other;
    uint16_t shndx;
};

constexpr uint32_t DT_HASH = 4, DT_STRTAB = 5, DT_SYMTAB = 6, DT_RELA = 7,
                   DT_RELASZ = 8, DT_RELAENT = 9, DT_STRSZ = 10, DT_SYMENT = 11,
                   DT_INIT = 12, DT_FINI = 13,
                   DT_REL = 17, DT_RELSZ = 18, DT_RELENT = 19, DT_PLTREL = 20,
                   DT_JMPREL = 23, DT_PLTRELSZ = 2,
                   DT_INIT_ARRAY = 25, DT_INIT_ARRAYSZ = 27,
                   DT_FINI_ARRAY = 26, DT_FINI_ARRAYSZ = 28;
constexpr uint32_t PT_LOAD = 1, PT_DYNAMIC = 2;

constexpr uint32_t R_ARM_ABS32 = 2, R_ARM_GLOB_DAT = 21, R_ARM_JUMP_SLOT = 22,
                   R_ARM_RELATIVE = 23;

constexpr size_t kImageMax = 4u * 1024u * 1024u;

template <typename T>
T read_at(const std::vector<uint8_t> &b, size_t off) {
    T v;
    std::memcpy(&v, b.data() + off, sizeof(T));
    return v;
}

#include <android/log.h>
#include <cstdarg>

void log(const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    __android_log_print(ANDROID_LOG_INFO, "zcr-engine", "%s", buf);
}

std::vector<uint32_t> g_init_routines;

}  // namespace

const std::vector<uint32_t> &engine_init_routines() { return g_init_routines; }

bool engine_load_image(const std::string &path, EngineImage *out) {
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || (size_t)sz > kImageMax) { fclose(f); return false; }
    out->bytes.resize((size_t)sz);
    size_t rd = fread(out->bytes.data(), 1, (size_t)sz, f);
    fclose(f);
    if (rd != (size_t)sz) return false;

    const auto &b = out->bytes;
    if (b.size() < sizeof(Ehdr) || b[0] != 0x7f || b[1] != 'E' ||
        b[2] != 'L' || b[3] != 'F' || b[4] != 1 /* ELFCLASS32 */) {
        return false;
    }
    out->entry_rva = read_at<uint32_t>(b, 24);
    return true;
}

uint32_t engine_export_addr(const EngineImage &img, const char *name) {
    for (uint32_t i = 0; i < ENGINE_JNI_COUNT; i++) {
        if (strcmp(kEngineJniExports[i].name, name) == 0)
            return ENG_ADDR(kEngineJniExports[i].rva);
    }
    return 0;
}

bool engine_map(uc_engine *uc, const EngineImage &img) {
    const auto &b = img.bytes;
    const Ehdr eh = read_at<Ehdr>(b, 0);

    /* 1. map + copy PT_LOAD segments */
    for (int i = 0; i < eh.phnum; i++) {
        const Phdr ph = read_at<Phdr>(b, eh.phoff + i * eh.phentsize);
        if (ph.type != PT_LOAD) continue;

        uint32_t seg_lo = ph.vaddr & ~0xFFFu;
        uint32_t seg_hi = (ph.vaddr + ph.memsz + 0xFFFu) & ~0xFFFu;
        uc_err err = uc_mem_map(uc, ENG_BASE + seg_lo, seg_hi - seg_lo,
                                UC_PROT_ALL);
        if (err != UC_ERR_OK) {
            log("uc_mem_map failed: %s", uc_strerror(err));
            return false;
        }
        if (ph.filesz) {
            uc_mem_write(uc, ENG_BASE + ph.vaddr, b.data() + ph.offset,
                         ph.filesz);
        }
    }

    /* 2. locate DYNAMIC */
    uint32_t dyn_off = 0, dyn_sz = 0;
    for (int i = 0; i < eh.phnum; i++) {
        const Phdr ph = read_at<Phdr>(b, eh.phoff + i * eh.phentsize);
        if (ph.type == PT_DYNAMIC) { dyn_off = ph.offset; dyn_sz = ph.filesz; }
    }
    if (!dyn_off) { log("no PT_DYNAMIC"); return false; }

    uint32_t strtab_va = 0, symtab_va = 0, rel_va = 0, rel_sz = 0,
             jmprel_va = 0, pltrelsz = 0, pltrel = 0, relent = 8, syment = 16;
    uint32_t init_va = 0, init_array_va = 0, init_array_sz = 0;
    for (uint32_t o = 0; o + 8 <= dyn_sz; o += 8) {
        const Dyn d = read_at<Dyn>(b, dyn_off + o);
        if (d.tag == 0) break;
        switch (d.tag) {
            case DT_STRTAB: strtab_va = d.val; break;
            case DT_SYMTAB: symtab_va = d.val; break;
            case DT_REL: rel_va = d.val; break;
            case DT_RELSZ: rel_sz = d.val; break;
            case DT_RELENT: relent = d.val; break;
            case DT_JMPREL: jmprel_va = d.val; break;
            case DT_PLTRELSZ: pltrelsz = d.val; break;
            case DT_PLTREL: pltrel = d.val; break;
            case DT_SYMENT: syment = d.val; break;
            case DT_INIT: init_va = d.val; break;
            case DT_INIT_ARRAY: init_array_va = d.val; break;
            case DT_INIT_ARRAYSZ: init_array_sz = d.val; break;
            default: break;
        }
    }

    /* VA -> file offset via PT_LOAD */
    auto va_to_off = [&](uint32_t va) -> uint32_t {
        for (int i = 0; i < eh.phnum; i++) {
            const Phdr ph = read_at<Phdr>(b, eh.phoff + i * eh.phentsize);
            if (ph.type == PT_LOAD && va >= ph.vaddr &&
                va < ph.vaddr + ph.filesz)
                return ph.offset + (va - ph.vaddr);
        }
        return 0;
    };

    auto sym_name = [&](uint32_t sym_idx) -> const char * {
        uint32_t sym_off = va_to_off(symtab_va) + sym_idx * syment;
        const Sym s = read_at<Sym>(b, sym_off);
        uint32_t n_off = va_to_off(strtab_va) + s.name;
        return (const char *)(b.data() + n_off);
    };

    /* 3. relocations: redirect imported symbols to stub traps */
    auto apply_rel = [&](uint32_t rel_va_arg, uint32_t rel_sz_arg) {
        for (uint32_t o = 0; o + sizeof(Rel) <= rel_sz_arg; o += relent) {
            const Rel r = read_at<Rel>(b, va_to_off(rel_va_arg) + o);
            uint32_t type = r.info & 0xFF;
            uint32_t sym_idx = r.info >> 8;
            uint32_t where = ENG_BASE + r.offset;
            uint32_t value = 0;

            if (type == R_ARM_RELATIVE) {
                uint32_t addend = 0;
                uc_mem_read(uc, where, &addend, 4);
                value = ENG_BASE + addend;
            } else if (sym_idx != 0) {
                const char *nm = sym_name(sym_idx);
                bool found = false;
                for (uint32_t s = 0; s < ENGINE_IMPORT_COUNT; s++) {
                    if (strcmp(kEngineImportNames[s], nm) == 0) {
                        value = stubs_slot_addr(s);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    /* engine-internal symbol */
                    uint32_t sym_off =
                        va_to_off(symtab_va) + sym_idx * syment;
                    const Sym s2 = read_at<Sym>(b, sym_off);
                    value = ENG_BASE + s2.value;
                }
                /* R_ARM_ABS32: value = S + A (addend lives at the slot) */
                if (type == R_ARM_ABS32) {
                    uint32_t addend = 0;
                    uc_mem_read(uc, where, &addend, 4);
                    value += addend;
                }
            }
            uc_mem_write(uc, where, &value, 4);
        }
    };
    apply_rel(rel_va, rel_sz);
    if (jmprel_va && pltrel == DT_REL) apply_rel(jmprel_va, pltrelsz);

    /* validate PLT relocations: report any GOT slot left as 0 */
    {
        uint32_t zero_slots = 0, total = 0;
        for (uint32_t o = 0; o + 8 <= pltrelsz; o += 8) {
            const Rel r = read_at<Rel>(b, va_to_off(jmprel_va) + o);
            uint32_t where = ENG_BASE + r.offset;
            uint32_t val = 0;
            uc_mem_read(uc, where, &val, 4);
            total++;
            if (val == 0 || val == 0xFFFFFFFFu) {
                uint32_t nm = read_at<uint32_t>(
                    b, va_to_off(symtab_va) + (r.info >> 8) * syment);
                const char *s = (const char *)(b.data() + va_to_off(strtab_va) + nm);
                log("PLT reloc UNAPPLIED: got@0x%x type=%u sym=%s", r.offset,
                    r.info & 0xFF, s);
                if (++zero_slots > 10) break;
            }
        }
        log("PLT reloc check: %u slots, %u zero", total, zero_slots);
    }

    /* Collect global constructors (DT_INIT + DT_INIT_ARRAY) - a real
     * dlopen runs these before JNI_OnLoad; static vtables/singletons live
     * here. Values are relocated emulated code addresses (bit0 = Thumb). */
    g_init_routines.clear();
    if (init_va) g_init_routines.push_back(init_va);
    if (init_array_va && init_array_sz) {
        uint32_t count = init_array_sz / 4;
        for (uint32_t i = 0; i < count; i++) {
            uint32_t fn = 0;
            uc_mem_read(uc, ENG_BASE + init_array_va + i * 4, &fn, 4);
            if (fn && fn != 0xFFFFFFFFu) g_init_routines.push_back(fn);
        }
    }
    log("engine mapped at 0x%x (entry rva 0x%x), %u constructors",
        ENG_BASE, img.entry_rva, (unsigned)g_init_routines.size());
    return true;
}

/* Patches originally applied by libZombieCafeExtension (Airyzz). Applied as
 * emulated-memory writes instead of live mprotects. See
 * src/lib/cpp/ZombieCafeExtension.cpp for the provenance of each offset. */
bool engine_apply_zcr_patches(uc_engine *uc) {
    struct StrPatch { uint32_t rva; const char *text; };
    static const StrPatch kStrs[] = {
        {0x1a14dc, "ZCR - %s"},
        {0x1a6610, "https://zc.airyz.xyz/v1/updater/%s"},
        {0x1a839c, "https://zc.airyz.xyz/v1/x"},
        {0x1a842c, "https://zc.airyz.xyz/v1/zca"},
    };
    for (const auto &p : kStrs) {
        uc_mem_write(uc, ENG_ADDR(p.rva), p.text, strlen(p.text) + 1);
    }

    /* Thumb NOP = 00 bf. Upstream also NOPed the texture destructors as a
     * workaround for a scudo double-free crash on real devices - that crash
     * mechanism doesn't exist here (we control the allocator, and the
     * magic-tagged free list rejects bad frees), so we keep the destructors
     * ALIVE. NOPing them leaks every texture forever and OOMs the cafe load. */
    static const uint32_t kNops[] = {
        0x9dee8,
    };
    for (uint32_t rva : kNops) {
        uint8_t nop[4] = {0x00, 0xBF, 0x00, 0xBF};
        uc_mem_write(uc, ENG_ADDR(rva), nop, 4);
    }

    /* Money-buy -> toxin-buy: movs r3, #0xb8 and NOP the following lsls. */
    uint8_t movs_r3_b8[2] = {0xB8, 0x23};
    uc_mem_write(uc, ENG_ADDR(0xab018), movs_r3_b8, 2);
    uint8_t nop2[2] = {0x00, 0xBF};
    uc_mem_write(uc, ENG_ADDR(0xab01a), nop2, 2);

    /* Missing-art safety: loadImagePack() logs "RM_HUMAN/RM_ZOMBIE ERROR"
     * and then calls PackedImage::getImages(NULL) when getImageSubpack()
     * cannot resolve "%s-human"/"%s-zombie", which faults at this+0x20.
     * Redirect both tails to store a null image array instead:
     *   0x125ffe: ldr r0,[r6,#8] / b 0x125e4a  ->  movs r0,#0 / b 0x125e50
     *   0x12600e: ldr r0,[r6,#0xc] / b 0x125e9e ->  movs r0,#0 / b 0x125ea4
     * (0x125e50 / 0x125ea4 are the corresponding str r0,[r6,#0x10]/#0x14
     * sites, so the character keeps null art instead of crashing.) */
    static const uint8_t kNullArtH[4] = {0x00, 0x20, 0x26, 0xE7};
    static const uint8_t kNullArtZ[4] = {0x00, 0x20, 0x48, 0xE7};
    uc_mem_write(uc, ENG_ADDR(0x125ffe), kNullArtH, 4);
    uc_mem_write(uc, ENG_ADDR(0x12600e), kNullArtZ, 4);

    static const bool kPatchCompletionHook = true;
    static const bool kPatchLoadNewSet = true;
    static const bool kPatchRaidType = true;

    /* ---- offline-friendly objectives (no friend/online quests) ----
     * Objective types: 0 raid-enemy, 1 raid-friend, 2 raid-any, 3 serve,
     * 4 cook, 5 invite-friends, 6 spend, 7 collect-fees, 8 gifts, 9 orders.
     * Types 1/5/7/8/9 need friends online and block solo play. */

    /* (1) getObjectiveCompletion() returns the goal for friend types, so a
     * stuck friend objective in an existing save shows as done and becomes
     * claimable (tap it in the goals dialog). Cave code lives on a dedicated
     * executable page; the original 12-byte accessor branches there. */
    static const uint16_t kCave[] = {
        0x68C2,             /* ldr r2, [r0, #0xc]   ; objective array      */
        0x0089,             /* lsls r1, r1, #2                              */
        0x588A,             /* ldr r2, [r1, r2]     ; obj                   */
        0x8813,             /* ldrh r3, [r2, #0]    ; type                  */
        0x2B01, 0xD009,     /* cmp r3, #1 / beq goal (raid-friend)          */
        0x2B05, 0xD007,     /* cmp r3, #5 / beq goal (invite)               */
        0x2B07, 0xD005,     /* cmp r3, #7 / beq goal (collect-fees)         */
        0x2B08, 0xD003,     /* cmp r3, #8 / beq goal (gifts)                */
        0x2B09, 0xD001,     /* cmp r3, #9 / beq goal (orders)               */
        0x6850, 0x4770,     /* ldr r0, [r2, #4] / bx lr   ; completion      */
        0x6890, 0x4770,     /* ldr r0, [r2, #8] / bx lr   ; goal           */
    };
    const uint32_t cave_addr = 0x11000000u; /* within BL range of the engine */
    if (kPatchCompletionHook) {
        uc_mem_map(uc, cave_addr, 0x1000, UC_PROT_ALL);
        uc_mem_write(uc, cave_addr, kCave, sizeof(kCave));
        /* getObjectiveCompletion (0xeae24): b.w cave; nops. Must be an
         * unconditional BRANCH (not bl) - bl would clobber LR and the cave's
         * bx lr would return to the branch itself, looping forever. With b.w
         * the cave's bx lr returns straight to the original caller. */
        uint8_t getcomp[12] = {0x15, 0xF3, 0xEC, 0x90,   /* b.w 0x11000000     */
                               0x00, 0xBF, 0x00, 0xBF,   /* nop nop            */
                               0x00, 0xBF, 0x00, 0xBF};  /* nop nop            */
        uc_mem_write(uc, ENG_ADDR(0xeae24), getcomp, 12);
    }

    /* (2) loadNewSet: replace createSocialObjective with
     * createServingObjective so new quest sets never ask for friends. */
    if (kPatchLoadNewSet) {
        uint8_t bl_serving[4] = {0xFF, 0xF7, 0x6F, 0xFA}; /* bl 0xeaf54 */
        uc_mem_write(uc, ENG_ADDR(0xeba72), bl_serving, 4);
    }

    /* (3) createRaidObjective: NOP the branch to the friend/any variant so
     * raid quests are always "raid enemy cafes" (local rival cafes). */
    if (kPatchRaidType) {
        uc_mem_write(uc, ENG_ADDR(0xeafec), nop2, 2);
    }

    log("ZCR quest patches: hook=%d loadnewset=%d raid=%d",
        kPatchCompletionHook, kPatchLoadNewSet, kPatchRaidType);
    log("ZCR patches applied");
    return true;
}
