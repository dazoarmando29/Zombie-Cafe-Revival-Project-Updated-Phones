# Engine & Memory Audit — Zombie Café Revival (64-bit host)

Status: verified against source and on-device measurements (Android 15,
arm64 host, 2026-11-01). Findings are marked **confirmed** (verified in code
or on device) or **hypothesis** (plausible but not yet proven).

## 1. How the original engine is loaded

**Confirmed** (`host/zcr_host.cpp`, `host/engine_loader.cpp`):

1. ART loads `libZombieCafeAndroid.so` from `lib/arm64-v8a/` — this is the
   `zcr_host` library, built with the *same soname* as the original game
   library. `JNI_OnLoad` runs `host_init()`.
2. `host_init()` reads the untouched 32-bit engine binary from the APK asset
   `assets/engine/game.so` (`load_engine_from_assets()`).
3. `uc_open(UC_ARCH_ARM, UC_MODE_ARM)` creates a Unicorn ARM32 core.
4. `engine_map()` maps the ELF32 PT_LOAD segments at `ENG_BASE` (0x10000000),
   applies `R_ARM_RELATIVE` / `R_ARM_ABS32` / `R_ARM_GLOB_DAT` relocations,
   and redirects every imported symbol (151 imports) to a 16-byte trap slot
   on `STUB_PAGE` (0xF0000000). `generated/engine_tables.h` holds the
   generated import/export tables (`host/tools/gen_tables.py`).
5. `engine_apply_zcr_patches()` writes the revival patches into emulated
   memory (URL strings, quest patches, money-buy patch, Thumb NOP at
   0x9dee8). A code cave at 0x11000000 carries the
   `getObjectiveCompletion()` replacement.
6. Global constructors (`DT_INIT` + `DT_INIT_ARRAY`) and the engine's own
   `JNI_OnLoad` are executed under emulation, then control returns to Java.

The 32-bit `lib/armeabi-v7a/` APK path keeps the *original* library and the
original `libZombieCafeExtension.so` (Airyzz's runtime patcher) and is
unaffected by the host.

## 2. Emulated address space

**Confirmed** (`host/engine_memory.h`):

| Region | Base | Size | Notes |
|---|---|---|---|
| Engine image | 0x10000000 | ~2 MB (ELF) | PT_LOAD segments, relocated |
| Code cave | 0x11000000 | 4 KB | quest patches (within BL range) |
| Emulated heap | 0x70000000 | 1536 MB | `emu::Heap`, zero-copy host alias |
| Emulated stack | 0xE0000000 | 8 MB | zero-copy host alias |
| Return trampoline | 0xEFFFF000 | 4 KB | host→engine call returns here |
| Import stubs | 0xF0000000 | 64 KB | 151 × 16-byte trap slots |
| JNI stubs | 0xF1000000 | 64 KB | fake JNIEnv vtable traps |
| JavaVM vtable | 0xF1008000 | — | fake JavaVM |
| JNI env object | 0xF2000000 | 4 KB | fake JNIEnv object |
| FILE* tokens | 0xF5000000 | grows | host FILE* handle tokens |

The heap spans 0x70000000–0xD0000000 and cannot collide with the stack at
0xE0000000. Guest pointers are **always** 32-bit emulated addresses; host
access goes through `emu::host_ptr()` region aliasing or
`uc_mem_read/write`. No guest pointer is reinterpreted as a native 64-bit
pointer.

## 3. Allocations and deallocations

**Confirmed** (`host/emu_heap.cpp`, `host/emu_mem.cpp`, `host/imports_posix.cpp`):

* The engine's `malloc/free/realloc` imports are served by `emu::Heap`
  over the aliased 1.5 GB buffer (no host malloc per allocation).
* Block layout: 8-byte header (span + state), user pointer = header+8,
  8-byte aligned; free blocks carry fd/bk links and a size footer.
* First-fit free list with **O(1) forward/backward coalescing** (boundary
  tags, `PREV_INUSE` bit). Replaced the original never-merge free list
  (Phase 3 upgrade; see `host/tests/heap_test.cpp`, 219k assertions pass on
  device).
* Invalid frees (wild/unaligned/corrupt), double frees and >64 MB requests
  are rejected and counted (`[badfree]`, `REJECTED huge alloc`, `EXHAUSTED`
  markers preserved).
* `strdup` and JNI string marshaling allocate from the same heap
  (`intern_string`, tag `jni-string`).
* Host-side metadata (`g_blocks` in `emu_mem.cpp`) records size, tag, and
  the LR of alloc/free for provenance (`heap_describe()` in crash dumps).

## 4. Guest pointer translation

**Confirmed**: `emu::map_alias()` registers host buffers with
`uc_mem_map_ptr`; `emu::host_ptr()` resolves an emulated address to a host
pointer only for those regions, otherwise callers fall back to
`uc_mem_read/uc_mem_write`. `read_str()` clamps to the aliased region size
(the UI string table contains 18 KB single strings).

## 5. Graphics pipeline

**Confirmed** (`host/imports_gl.cpp`): GLES1 fixed-function calls are
forwarded to the real `libGLESv1_CM`; shader calls fail on the GLES1
context exactly like the original devices and the engine falls back to
fixed-function rendering. Client-array pointers resolve through
`host_ptr` zero-copy; otherwise they are copied into reused scratch
buffers at draw time. Texture uploads (`glTexImage2D` /
`glCompressedTexImage2D`) are tracked per GL id (see
`docs/TEXTURE_MEMORY_REPORT.md`).

## 6. Content indexing and hardcoded limits

**Confirmed** (string table of `src/lib/armeabi/libZombieCafeAndroid.so`,
`.rodata` at file offsets listed):

* The engine loads **exactly two character texture sheets**:
  `images/characterParts.cct.mid` + `images/characterOffsets.bin.mid` +
  `data/characterArt.bin.mid` (0x1a4c08/0x1a4c28/0x1a4d80) and the `2`
  variants (0x1a4c64/0x1a4c84/0x1a4d9c). The string table contains exactly
  these two triplets — no sheet 3+.
* Furniture has three sheets (`furniture`, `furniture2`, `furniture3`) and
  recipes two (`recipeImages`, `recipeImages2`), each with hardcoded
  filename strings.
* The Japanese 1.7 build shipped **five** character sheets
  (`characterParts3..5`; see `tool/resource_manager/serialization/`),
  proving the engine family's data structures support more than two sheets;
  the English binary's load sequence references only two.
* `characterData` count is a **1-byte field (255 characters max)** — the
  shipped data has 219. `foodData` count is also 1 byte. `furnitureData`
  uses an int32 count. Offsets files use an int16 count (32767 rects).
* `characterArt` files store `PiecesPerString` as a byte (255 pieces per
  character max; shipped sheets use 27).

String references inside the binary are PIC (GOT-relative or PC-relative);
no absolute pointer or literal pool entry points at the filename strings
(verified by exhaustive byte-level scan of the image), which is why naive
patching of a filename constant is not possible — the strings are consumed
through computed addresses that have not yet been traced to their call
sites. See `docs/ASSET_EXPANSION_GUIDE.md` for the exact blocker status.

## 7. Residency and unloadability

**Confirmed** (on-device `[heapstat]`/`[texstat]`, café loaded):

* Guest heap: **7.4 MB live / 27.4 MB peak**, 228k live blocks (the engine
  retains a large number of tiny allocations, ~33 bytes average),
  0 allocation failures, 1941 free blocks, largest free 1.52 GB.
* Textures: **22 live GL textures, 52.9 MB** (two 2048×2048 RGBA atlases
  dominate), 7 duplicate uploads detected by content hash.
* The 1.5 GB arena is a lazy-zero `calloc`; resident memory is what is
  actually touched (VmRSS ≈ 280 MB including GPU driver accounting).

Everything the engine allocates on the guest heap is owned by the engine;
the host must not free it behind the engine's back. GL textures are
deleted through the engine's own `glDeleteTextures` path (destructors are
intentionally kept alive — see `engine_apply_zcr_patches()` comments).

## 8. Where crashes / corruption / exhaustion may occur

**Confirmed** (diagnostics and code review):

1. **Save writes are non-atomic** (`CC_Android.fromNative_fileWrite` uses
   offset-based `openFileOutput` writes). A process kill mid-save can leave
   a half-written save. Mitigated by `SaveGuard.smali` backups + read
   fallback (Phase 12).
2. **Scoped storage broke the diagnostic log** (writes to
   `/sdcard/Download` fail on Android 10+). Fixed: the log now goes to the
   app's external files dir (`diag_set_log_dir`, verified on device).
3. **String-copy crash sites** (0x10141bd4 / 0x10141c42 in engine VA,
   `zcr_host.cpp on_mem_fault`) have a dedicated diagnosis chain
   (`[strchain]` dumps). Not reproduced in current testing; instrumentation
   is in place to capture the exact state when it happens.
4. **Engine heap exhaustion** is structurally unlikely (peak 27 MB of 1536
   MB) but is loudly reported (`EXHAUSTED` + full stats dump).
5. **Duplicate texture uploads** waste GPU memory (52.9 MB total uploaded,
   7 duplicates at café load) but are not a crash risk.

## 9. Baseline process measurements (before this work)

From `dumpsys meminfo` on the pre-upgrade build (same device, café loaded):

| Metric | Baseline | After upgrade (steady café) |
|---|---|---|
| Native heap RSS | 113 MB | 72 MB |
| EGL mtrack (GPU) | 92 MB | 106 MB |
| Total PSS | 250 MB | 227 MB |
| /proc VmRSS | n/a (no report) | 280 MB |

The EGL figure grows with textures on screen; the native figure differs
mainly by allocator metadata and what is resident at sampling time.
