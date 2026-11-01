# Content Expansion Test Report — Zombie Café Revival

Branch: `feature/content-expansion`. Every claim below is marked with its
verification level:

* **implemented + device-tested** — code in the repo, exercised on a real
  Android device with observed expected behavior,
* **implemented, not device-tested** — code in the repo, statically or
  unit-tested only,
* **prototype** — works under narrow conditions, not production-ready,
* **blocked** — known engine/asset limitation,
* **planned** — not started.

## 1. Stage A — expansion foundations (2026-11-01)

### 1.1 N-sheet character loading — implemented + device-tested

| Check | Result |
|---|---|
| `characterParts3` packs through `build_tool` into `characterArt3.bin.mid` + `characterParts3.cct.mid` + `characterOffsets3.bin.mid` | PASS (build pipeline) |
| `characterParts4` (scalability proof) | PASS (same pipeline) |
| Engine creates `PackedImage` for sheets 3+4 at runtime (`[sheet] sheet N loaded ... pi=...`) | PASS (device) |
| Characters referencing sheet-3 art resolve (`[sheet] resolved zzsheet3-human/-zombie via sheet 3`) | PASS (device) |
| Characters referencing sheet-4 art resolve | PASS (device) |
| Existing 219 characters unchanged (café populated with original staff/customers, animations running) | PASS (device, screenshots) |
| Pre-existing Level-9 save loads and plays | PASS (device) |
| Save writes (`BACKUP1.dat`/`.caf`, `ServerData.dat`) after gameplay | PASS (device) |
| App restart round-trip (kill + relaunch + PLAY → same café state, sheets reload, art re-resolves) | PASS (device) |
| Emulator health (`[summary] emu_errors=0`, `unimpl=0` over 600k+ draws) | PASS (device) |
| GL context-loss reload path (`ReloadTextures` hook re-creates extra sheets) | implemented, not device-tested (no forced context loss performed) |
| `asset_validator` accepts the new sheets and appended characters | PASS (0 errors, 0 warnings) |
| Go unit tests (`asset_validator`, `resource_manager/serialization`) | PASS |

Device: Android 15 (V2318, arm64-v8a). APK 39.5 MB built by `build.ps1`.

Known deviations from the plan:

* Test characters ("ZCR Sheet3 Tester", "ZCR Sheet4 Tester") use recycled
  pieces from `generic-human`/`generic-zombie` as **development fixtures**,
  explicitly not final artwork. They are appended as `characterData` rows
  220–221 (append-only, save-compatible).
* On GL context loss the superseded extra-sheet `PackedImage` objects leak
  a few KB of guest heap per loss (bounded).

### 1.2 Verified engine findings (RE corrections)

The previously documented hypothesis that `loadImagePack(String, byte)` is
a generic *sheet* loader whose byte argument is a pack index is
**incorrect**. Verified behavior (disassembly + runtime):

* The byte is the character **form** selector (`0`=human, `1`=zombie,
  `0xFF`=both), matching the `U4` field of `characterData` (observed
  values in shipped data: exactly `{0, 1, 255}`).
* `loadImagePack` resolves `"%s-human"`/`"%s-zombie"` subpacks inside the
  already-loaded sheets via `getImageSubpack`; it does not load sheets.
* Sheets are loaded only by the two unrolled `initData` blocks (load steps
  5 and 11 of the `Loadstep` machine); the engine's name search spans
  exactly those two sheets' art lists.

See `docs/ASSET_EXPANSION_GUIDE.md` §3 for the field-level layout and the
host-side extension design.

## 2. Feature status tracker

| Feature | Status |
|---|---|
| N-sheet character packing (tool) | implemented + device-tested |
| N-sheet character loading (runtime, 3..12) | implemented + device-tested |
| Test characters on sheets 3 and 4 | implemented + device-tested |
| `ReloadTextures` coverage for extra sheets | implemented, not device-tested |
| Content registry (Part 2.2) | planned (next milestone) |
| Content expansion guides (Part 2.3) | partial (`ASSET_EXPANSION_GUIDE.md` updated) |
| New characters (Stage C/D) | scaffolding ready; original artwork pending |
| New recipes (Stage C/D) | planned |
| Recipe mastery (Part 5.1) | planned |
| Cooking QoL (Part 4) | planned |
| Zombie management UI (Part 3.3) | planned (candidate: host overlay / mod menu) |
| Raid QoL (Part 7.1) | planned |
| Achievements / objectives (Part 8) | planned |
| Offline events (Part 9) | planned |

## 3. Regression testing performed

1. `go test ./tool/asset_validator/... ./tool/resource_manager/...` — PASS.
2. `go run ./tool/asset_validator -root src/assets` — 0 errors, 0 warnings.
3. Full APK build (`build.ps1`) — OK.
4. Device boot to menu, PLAY into café, gameplay observation — OK.
5. Save write during gameplay — observed in `[filetrace]`.
6. Force-stop + relaunch + PLAY — same café state restored.
7. `[heapstat]`/`[texstat]` reports reviewed — no leak trend over ~10 min
   sessions; guest heap ~7 MB live of 1536 MB.

## 4. Outstanding device tests

* Forced GL context loss (developer option "Don't keep activities" +
  background/foreground) to exercise the `ReloadTextures` hook.
* Multi-hour soak for slow leaks.

## 5. 32-bit (armeabi-v7a) path findings — 2026-11-01

Tested by forcing `adb install --abi armeabi-v7a` on the Android 15 device:

* **Pre-existing limitation (not caused by this work):** the original
  32-bit engine refuses to load on Android 15 —
  `dlopen failed: libZombieCafeAndroid.so has text relocations` (text
  relocations are rejected since API 23). The arm64 emulation host exists
  precisely because of this class of problem. The 32-bit path remains
  functional only on legacy devices (Android < 6).
* **Missing-art crash (fixed):** on the 32-bit path an art name that does
  not resolve (e.g. sheet-3 art — that engine only knows two sheets) made
  `loadImagePack` call `PackedImage::getImages(NULL)`, faulting at
  `this+0x20` (verified in disassembly: `ldrsh r2, [r0, #0x20]`). A
  4-byte null-guard patch now redirects both error tails to keep null art
  instead of crashing, applied on **both** paths
  (`host/engine_loader.cpp` and `src/lib/cpp/ZombieCafeExtension.cpp`).
  Effect: characters whose art cannot resolve on the running engine load
  with missing art instead of crashing the game.
* Consequence for new content: characters referencing `characterParts3+`
  art render on the arm64 path (the supported path on modern devices); on
  legacy 32-bit devices they appear with missing art instead of crashing.
  New content that must render on 32-bit legacy devices should use art on
  sheets 1–2.
