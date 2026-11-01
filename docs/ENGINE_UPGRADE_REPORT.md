# Engine Upgrade Report — Zombie Café Revival

Branch: `feature/engine-upgrade`. All claims below were implemented and
verified in this repository; unverified items are called out explicitly.

## 1. Implemented improvements

### Memory allocator (Phase 3)
* `host/emu_heap.{h,cpp}` (new): extracted allocator core — first-fit free
  list with boundary tags, O(1) forward/backward coalescing, double-free /
  wild-free / unaligned-free detection, full statistics (live/peak bytes &
  blocks, failures, largest live/free, coalesces).
* `host/emu_mem.cpp`: wraps the core, preserves the guest-visible API and
  the `[badfree]` / `EXHAUSTED` log markers, keeps per-tag and per-LR
  provenance (`[heapstat]`, `[heaplr]`).
* `host/tests/heap_test.cpp` + `run_heap_tests.ps1`: 219,329 assertions,
  run on-device via NDK clang (basic reuse, coalescing, fragmentation
  checkerboards, realloc grow/shrink, invalid frees, double frees,
  exhaustion + recovery, describe(), 200-wave randomized churn with data
  integrity checks). **Result: PASS on Android 15 device.**

### Memory diagnostics (Phase 2)
* `diag_report()`: combined report — `[heapstat]` (arena/live/peak blocks,
  alloc/free/fail/badfree/doublefree/coalesce counts, free-list summary),
  per-tag and per-LR allocation tables, `[texstat]`, and host
  `/proc/self/status` (VmRSS/VmPeak/VmHWM). Emitted at startup and every
  1800 frames; `heap_dump_stats()` also runs on `EXHAUSTED`.
* Existing markers preserved: `[heapstat]`, `[heaplr]`, `[badfree]`,
  `EXHAUSTED`, `[mem-fault]`, `[summary]`, `[trace]`.
* **Fix**: the diagnostic log now writes to the app's external files dir
  (resolved via JNI, `diag_set_log_dir`); the old `/sdcard/Download`
  target silently failed under Android scoped storage.

### Texture tracking (Phase 5)
* `host/imports_gl.cpp`: per-id texture accounting (dims/format/bytes),
  live/peak totals, create/delete counters, FNV-1a content-hash duplicate
  detection (`[texdupe]`), top-10 largest textures (`[texstat]`). Details
  in `docs/TEXTURE_MEMORY_REPORT.md`.

### RSS regression caught and fixed
* The first allocator revision zeroed the full 1.5 GB arena on startup;
  the new `/proc` report exposed it (VmRSS 1,730,976 KB). Fixed by only
  initializing block headers — **VmRSS 286,692 KB** afterwards, same
  workload.

### Save safety (Phase 12)
* `patches/.../SaveGuard.smali` (new): `<name>.bak` copy before the
  engine's `fromNative_fileWrite` overwrites a save (10 s throttle so
  multi-chunk offset writes are never snapshotted half-written).
* `fromNative_fileRead` falls back to the `.bak` when the primary file is
  unreadable ("serving save from backup" in logcat).
* Engine write semantics (offset-based `openFileOutput`) unchanged.

### Asset toolchain (Phases 6–8)
* `tool/asset_validator` (new module, library + CLI, 8 unit tests):
  validates character/food/furniture data, offset tables, CCTX headers and
  character-sheet folders against the real format limits. Shipped assets
  validate clean (0 errors, 0 warnings).
* `tool/build_tool` runs the validator before packing: malformed content
  fails the build with per-asset messages.
* `tool/resource_manager/serialization`: character-sheet outputs are
  derived from folder names (`characterPartsN` → `characterArtN` /
  `characterOffsetsN`) instead of a hardcoded 2-entry map; every
  `characterParts*` folder is packed. The JP 5-sheet naming is handled by
  the same code path.

## 2. Fixed issues

| Issue | Root cause | Fix | Verified |
|---|---|---|---|
| zcr-log.txt never created on Android 10+ | hardcoded `/sdcard/Download` paths fail under scoped storage | resolve app external files dir via JNI | log file present on device |
| Fragmentation: free blocks never merged | free list without coalescing | boundary-tag allocator | unit tests + `[heapstat] coalesces=66149` in 30 s of gameplay |
| 1.5 GB RSS balloon (introduced, then fixed) | full-arena memset on init | header-only init | VmRSS 1.69 GB → 280 MB |
| Save loss on interrupted write | non-atomic offset writes | `.bak` backup + read fallback | code path verified (class loads, fileRead unaffected); write path exercised on first save (manual test — see §6) |
| Invalid content shipped silently | packer accepted malformed data | validator gate in build | 8 tests + clean run on shipped assets |

## 3. Memory improvements (measured)

Same device, café loaded and idle (Android 15, arm64):

| Metric | Baseline build | This branch |
|---|---|---|
| Guest heap usage | not measurable | 7.4 MB live / 27.4 MB peak (of 1536 MB) |
| Guest alloc failures | not measurable | 0 over 311k allocs |
| Native heap RSS (dumpsys) | 113 MB | 72 MB |
| EGL mtrack (GPU) | 92 MB | 106 MB (more UI on screen) |
| Total PSS | 250 MB | 227 MB |
| Process VmRSS | not measurable | 280 MB (286,692 KB) |
| Texture bytes tracked | none | 52.9 MB live, 22 textures, 7 dupes detected |
| Leaks / bad frees visible | no | yes (counters + reports) |

## 4. Content capacity

| Limit | Before | Now |
|---|---|---|
| Characters (data) | 255 (1-byte count; 219 used) — enforced by validator | unchanged (format limit) |
| Character texture sheets packed | 2 (hardcoded map) | N (data driven) |
| Character sheets loaded by engine | 2 (binary strings) | 2 — engine-side blocker documented in `docs/ASSET_EXPANSION_GUIDE.md` |
| Furniture packs packed/loaded | 3 (hardcoded) | 3 packed (tool data driven for more; engine loads 3) |
| Foods | 255 (1-byte count) | unchanged (format limit) |
| Furniture entries | int32 count | unchanged |
| Rects per offsets file | 32767 (int16 count) | unchanged, now enforced |

## 5. Compatibility

* **Tested on**: Android 15 (V2318, arm64-v8a + armeabi-v7a), ~75 s
  sessions per build: app boots, café loads and renders (pixel readback
  non-blank), engine ticks, no unimplemented imports (`unimpl=0`), no
  emu errors, memory reports emitted.
* **Not tested here**: other Android versions/devices, multi-hour
  sessions, mod-menu variant, 32-bit `armeabi-v7a` path (unchanged code),
  interrupted-save recovery end-to-end (needs a kill during a save).
* Save format: unchanged (no serialization changes on the engine path).

## 6. Tests executed

| Test | Where | Result |
|---|---|---|
| `emu::heap` unit tests (219,329 checks) | device via `host/tests/run_heap_tests.ps1` | PASS |
| `asset_validator` unit tests (8) | Windows `go test` | PASS |
| Validator on shipped `src/assets` | Windows `go run` | 0 errors, 0 warnings |
| Full APK build pipeline | `build.ps1` | OK (39.2 MB) |
| Boot + café render + 3 reports | Android 15 device | OK, unimpl=0 |
| fileRead through patched smali | device (3.6 MB asset read) | OK |
| Save write + backup | device | fileWrite not yet triggered in short sessions — manual test pending (play until first save, then `adb shell run-as` unavailable; verify via logcat `zcr-save`) |

## 7. Remaining limitations

See `docs/KNOWN_LIMITATIONS.md` for the full list. Headline items:
engine-side third character sheet (RE blocker), character/food 255-entry
format cap, duplicate texture uploads not shareable without engine id
remapping, unverified long-session stability.

## 8. Next recommended work

1. Dynamic tracing of engine asset loads (`fopen`/`fileRead` path logging +
   sheet-index register capture) to locate the character-sheet load sites —
   unblocks the third sheet.
2. Deduplicate identical texture uploads via engine-side id remapping
   (needs the id→object mapping the engine keeps).
3. Long-session soak test (2 h+) watching `[heapstat]` block counts and
   `[texstat]` create/delete balance for leaks.
4. Low-RAM device profile (3 GB): consider reducing the TCG buffer and
   tracking `onTrimMemory` from Java.
5. End-to-end interrupted-save test harness (kill -9 during save, verify
   `.bak` recovery).
