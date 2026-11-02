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
| Missing-art crash guard (host + 32-bit extension) | implemented + device-tested (arm64); 32-bit path unrunnable on Android 15 (pre-existing) |
| Content registry (ZcrId, lockfile, build gate) | implemented + unit-tested (6 tests); exercised by every build |
| New recipe `recipe.brain_burger` | implemented; data + 146-image recipe atlas verified at load on device; cookbook/cooking walkthrough pending manual test |
| Content expansion guides (Part 2.3) | done (NEW_CHARACTER_GUIDE, NEW_RECIPE_GUIDE, CONTENT_REGISTRY) |
| New characters (Stage C/D, original art) | scaffolding ready + pipeline proven; original artwork is the missing dependency |
| Recipe mastery (Part 5.1) | prototype: host companion layer implemented + unit-tested (116 tests); cook-count + spend hooks device-verified (§8) |
| Cooking QoL (Part 4) | planned (see QUALITY_OF_LIFE_FEATURES.md) |
| Zombie management UI (Part 3.3) | planned (candidate: host overlay / mod menu) |
| Raid QoL (Part 7.1) | planned |
| Achievements / objectives (Part 8) | prototype: 15 data-driven achievements, progress + rewards pipeline implemented + unit-tested; live progress + persistence device-verified (see §8) |
| Offline events (Part 9) | planned |

## 2.1 Test-data incident (2026-11-01)

While forcing a 32-bit install to test missing-art behavior, the package
was **uninstalled**, which wiped the app's internal storage and with it
the device's existing Level-9 café save (the one used in §1.1). The
post-reinstall testing therefore ran on a fresh save (Level 3). Lessons
recorded: never `adb uninstall` a device carrying saves — use
`adb install -r` (keeps data) or copy the save out first
(`run-as`/backup). The engine save format itself was never at risk; no
save compatibility change was made by this work.

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

## 6. Stage C � first content expansion (2026-11-01)

| Check | Result |
|---|---|
| `recipe.brain_burger` row appended (217th food), registry-pinned | PASS (tooling) |
| New recipe art packed (`recipeImages/145-brain burger.png` -> 146-rect atlas) | PASS (build pipeline) |
| Game boots and runs with 217 foods + 146 recipe images, `emu_errors=0` | PASS (device) |
| Recipe balance inside level-8 band (180 profit/h vs band 133-420; 8.6 XP/h vs 4.5-25) | PASS (data audit) |
| Cookbook listing / cooking walkthrough | pending manual test |
| Original art for the recipe | pending (art currently recycled from `134-ReactorBurger.png`, labelled as dev placeholder) |

The content registry captured the new row (922 locked entries) and the
build gate runs on every `build.ps1`.

## 7. Save-survival layer (uninstall-proofing) - 2026-11-01

Answering "can saves survive uninstall + reinstall":

### Implemented and device-verified
* **Save export to Download/ZombieCafeRevival/** via MediaStore on every save
  write (per-file 10s throttle): device-verified that the exported files
  (BACKUP*.dat/.caf, playerCafe.caf, ServerData.dat, globalData.dat,
  raid*.dat) **survive db uninstall** across multiple uninstall cycles.
  A user can also copy this folder manually.
* **Internal .bak backup FIXED** - the previous branch's `backupMaybe` had
  three inverted branches (throttle, exists-check, read loop) and had never
  actually written a backup; all three are corrected and the throttle is now
  per-file so every save slot gets backed up.
* App-media-dir + legacy /sdcard export copies where the OS permits them.

### Verified NOT working on Android 15 (documented, code in place for older)
* **Automatic restore after reinstall**: the exported files survive, but the
  reinstalled app cannot read them back automatically -
  * File-API access to /sdcard/Download/ZombieCafeRevival/ is denied
    (`EACCES` under scoped storage, even for the app's own MediaStore
    creations);
  * MediaStore queries return 0 rows: legacy-target apps' inserts get
    `owner_package_name=NULL` and scoped visibility hides them (the shell
    can see the rows).
* The restore code path (`readExternal`: media-dir -> Download folder ->
  MediaStore, with independent fallbacks) is in place and automatically
  active on pre-Android-10 devices where the legacy File path works.

### Remaining to investigate
* A null-selection MediaStore probe is built in (`restore29: allrows` log)
  to distinguish a selection-args bug from provider visibility; the driving
  of the save-read moment needs a manual device session (the fresh-save
  tutorial blocks UI automation).
* If visibility is confirmed as the blocker, the viable path is an explicit
  one-time user action (SAF folder grant or a manual restore button reading
  the Download folder via SAF).

### Incidents during this work
* VerifyError from an unchecked `HashMap.get()` cast rejected the whole
  SaveGuard class for one build (fixed with `check-cast`).
* `String.concat` used as a static method crashed one build (fixed).
* Several inverted conditionals were found and corrected; each was caught
  by device logs, not by the build.

## 8. Recipe mastery + achievements companion layer (2026-11-02)

Host-side companion save (`host/companion_state.*` + `host/zcr_companion.*`)
keyed by ZcrIds, versioned JSON next to the engine saves
(`files/zcr_companion.json`), exported through the tested SaveGuard path.
It never touches the engine save format. Engine hooks are read-only and
land on disassembly-verified transitions (see
`docs/ASSET_EXPANSION_GUIDE.md` §9).

### 8.1 Implemented

| Component | Status |
|---|---|
| Companion state (cook counts per recipe, totals, achievement latches) | implemented + unit-tested (116 checks) |
| JSON serialization/parse (tolerant, tamper guards, deterministic) | implemented + unit-tested |
| Mastery levels (10/50/200 cooks = level 1/2/3, data-driven) | implemented + unit-tested |
| 15 data-driven achievements (`src/assets/data/zcr_achievements.json`, ZcrId params) | implemented + unit-tested |
| Reward pipeline (persist-then-grant; claim latch prevents double-claim; loss-safe direction) | implemented + unit-tested |
| Registry row -> ZcrId map from `data/zcr_registry.lock.json` (217 recipe rows) | implemented + device-verified (init log) |
| Cook-count hook (`DishToCounterTask::completeObjective` = engine's own `cookFood` accounting) | implemented + device-verified (see 8.2) |
| Toxin-spend hook (`ZombieCafe::spendToxin`) | implemented + device-verified (see 8.2) |
| Toxin-earn hook (`ZombieCafe::addToxin`, flag=0 gameplay only) | implemented, not device-tested |
| Reward grant (toxin field write, clamped 999) | implemented + unit-tested; on-device grant fired with `ach.cook_first` (state shows claimed) |
| SaveGuard export/restore integration for the companion file | implemented + device-verified (export to Download; `restoreInternal` called OK) |
| Flush safety net (render-tick flush; Java `onPause` never calls the native pause entry - found this session) | implemented, not device-tested |
| Export retry (mirrors SaveGuard's 10s throttle so a throttled copy is retried instead of lost) | implemented, not device-tested |

### 8.2 Device evidence (Android 15, 2026-11-02)

* Init (two cold boots): `defs: 15 achievements, mastery thresholds=3`,
  `registry: 217 recipe rows mapped to ZcrIds`, `restoreInternal -> 1`,
  `selftest: pipeline ok (probe total=1 roundtrip=1)`,
  `hooks installed (cooked@0x82048 spend@0x11fc94 add@0x122134)`.
* **Persistence round-trip**: companion file created+exported at 01:12
  (boot 1), read back at 01:15 (boot 2, `state loaded:` matching the file
  contents).
* **Live event pipeline**: in-game toxin instant-finish (tutorial) fired
  `toxin spent: 1 (total 1)` at 02:05:26; the exported companion file
  updated at 02:05 with `"toxin_spent":1`, `ach.toxin_spent_10/100`
  `progress:1`, all 15 achievement records present.
* **Cook-count hook device-verified end-to-end** (diag file evidence):
  the tutorial's first dish (Mystery Meat) cooked, toxin-finished and
  carried to the counter produced
  `cooked: row=0 variant=0 key=recipe.mystery_meat total=1` (recipe row
  byte 0 resolved through the 217-row registry map to its ZcrId),
  then `achievement unlocked: ach.cook_first (reward 1 toxin)` and
  `reward granted: +1 toxin -> 6`. The next cold boots loaded
  `state loaded: cooked=1 recipes=1 ... ach=15 pending-claims=0` - the
  count, the ZcrId key, the unlock and the claim latch all survived the
  process restart with no double-claim.
* Export edge found + fixed: the export copy can lag one write behind
  (SaveGuard's Java-side 10s throttle silently drops a copy in the same
  window); the host now mirrors the throttle and retries the export from
  the render tick. The internal file stayed authoritative throughout.
* No regressions: `emu_errors=0` over 490k+ draws across the session,
  sheets load, save writes (`BACKUP2.*`, `ServerData.dat`) normal, no
  dex verify errors.

### 8.3 Anti-exploit design

* Counters are monotonic; achievement unlock flags latch (each id returns
  exactly once from evaluation); claims are latched in the companion save
  BEFORE the reward is granted (a crash between the two loses the reward
  instead of allowing a re-claim).
* Toxin rewards are bounded (max 5 per achievement, ~31 total across the
  current set), non-repeatable, and respect the monthly-award cap design
  (`docs/GAMEPLAY_BALANCE.md` §3).
* Purchased toxin (`GiveBoughtToxin` -> `addToxin` flag=1) is not counted
  as gameplay earnings.

### 8.4 Save-state incident (2026-11-02, forensics)

While driving the game for cook-hook verification, the device showed a
fresh-game tutorial (Level 1) instead of the previous test save. Decisive
checks (file timestamps, `filetrace`, export-folder forensics with
`tool/file_types`):

* The previous session's save-survival testing (§7) ran **multiple
  `adb uninstall` cycles** by design; each wiped the app's internal
  storage. The last internal state was already the post-reinstall state;
  `playerCafe.caf` was a 2-byte stub at 00:06.
* **No save data was lost this session** - the pre-session internal state
  was already the fresh-game state (PLAY went to chef-select before any
  UI action). The old save data remains in the uninstall-surviving
  exports: `Download/ZombieCafeRevival/` and the app media dir
  (`Android/media/<pkg>/ZombieCafeRevival/saves/`), e.g. `BACKUP1.caf`
  (19851 B, 00:06) + `ServerData (1)-(5).dat` (22:44-00:06).
* Recovery of those exports into internal storage on Android 15 remains
  the known blocked item (§7 / SAF or manual restore button). On this
  device the app-media-dir copy is still present and readable by the app
  (no uninstall happened this session), so `SaveGuard.restoreInternal`
  can serve it once the internal files are missing (e.g. via the planned
  manual-restore flow).
* The new game created during testing writes the normal save set
  (`BACKUP2.dat/.caf`, `ServerData.dat`) and is itself export-verified.

### 8.5 Outstanding

* None for the core pipeline (cook hook, spend hook, achievements,
  persistence, export all device-verified; the `cooked:` log line was
  recovered from the host diag file after vivo suppressed app logcat
  following a package replacement - `zcr-log.txt` is the reliable
  evidence channel on this device).
* Forced GL context loss, multi-hour soak (carried over from §4).

## 9. Recipe favorites persistence (2026-11-02, Stage B QoL)

The original game's cookbook already has a favorite-star system (star
toggle per recipe, favorites tab, 14-favorite slot list) - but its state
round-trips through the engine save with save-timing staleness: toggles
made after the last engine save resurrect on reload (device-observed).
This milestone makes favorites write-through reliable via the companion
save.

### 9.1 Implemented

| Component | Status |
|---|---|
| `set_favorite` in companion state (ZcrId-keyed, JSON `"favorites"` map) | implemented + unit-tested (15 checks in the 131-check suite) |
| `RecipeBook::updateFavorite` hook (records toggles instantly) | implemented + device-verified (`favorite: row=0 key=recipe.mystery_meat -> 0` etc.) |
| `RecipeBook::loadCookBook` reconcile (companion-wins with 1/0 writes; adopt engine-restored favorites when companion is empty) | implemented + device-verified |
| Favorites slot list rebuild (14-cap, grow via engine primitive 0xfb7c0) | implemented + device-verified (`list=1`) |
| UI verification (tile stars persist across force-stop + relaunch) | implemented + device-verified (screenshots) |

### 9.2 Device evidence

* Toggle on/off captured in the companion file instantly:
  `favorite: row=4 key=recipe.green_eggs_sam -> 1`,
  `favorite: row=0 key=recipe.mystery_meat -> 0`.
* After force-stop + relaunch + cookbook open:
  `favorites reconcile: adopted=0 set=0 cleared=1 list=1` - the stale
  engine flag (un-favorite made after the last engine save) was cleared
  per the companion, the surviving favorite (row 4) kept its star, and
  the engine's favorites slot list was rebuilt. Re-opening the book is
  idempotent (`cleared=0 list=1`).
* RE findings recorded in `docs/ASSET_EXPANSION_GUIDE.md` §9.5
  (Recipe+0x1e flag, updateFavorite/loadCookBook entry points, the
  favorites widget layout, grow primitive).

### 9.3 Known quirks (engine-side, cosmetic)

* The detail panel's ADD/REMOVE-star button can show the engine's stale
  default label on the auto-selected recipe immediately after opening the
  book; it refreshes on any recipe selection (tile stars - the actual
  favorite markers - are always correct after the reconcile).
* Adopt-from-engine path (companion empty + engine restored favorites)
  is implemented but not yet exercised on device (the test companion was
  non-empty by then).

## 10. Toxin-spend confirmation (2026-11-02, Stage B QoL)

### 10.1 Audit (RE-verified, ASSET_EXPANSION_GUIDE A9.6)

Complete 20-site `spendToxin` map with per-site gating evidence:
19 of 20 spends are already behind the engine's ConfirmDialog (GOT-wired
yes-callbacks verified per site; `ConfirmDialog::tick` fires the callback
on the affirmative button only). `CharacterDetails::heal` is dead code
(no xrefs). The single real gap was the zookdex character purchase
(`GameStateZukan::buyCharacter` common path: one-tap spend after a
balance check).

### 10.2 Implemented

| Component | Status |
|---|---|
| Zookdex purchase confirm gate (entry hook + engine ConfirmDialog + Thumb re-entry cave) | implemented + built (out-mod.apk) |
| Dialog wording ("DO YOU WISH TO BUY THIS CHARACTER?" BUY/CANCEL) - neutral, covers toxin- and coin-priced characters | implemented |
| Failure-safe pass-through (no dialog instance -> vanilla behavior) | implemented |
| Device verification (dialog appears, YES buys, CANCEL aborts) | PENDING (phone unplugged at build time) |

### 10.3 Design notes

No engine binary patching: the spend path itself is untouched engine
code; the gate only intercepts the entry, queues the engine's own dialog
(ConfirmDialog* at zukan+0xb0) and re-enters through a host-mapped cave
that restores the purchase index. Log lines: `zukan buy: confirm queued
(index=N ...)` and `toxin spent: N via zukan_buy` after YES.

## 11. Cooking/serving QoL: auto-cook, auto-serve, serve guard (2026-11-02)

### 11.1 Implemented (engine RE: ASSET_EXPANSION_GUIDE A9.8)

| Component | Status |
|---|---|
| Auto-cook: idle stove -> cook last-cooked recipe (companion `last_cooked_row`), engine `GameStateCafe::cook` handles cost + deduction | implemented + built |
| Unaffordable -> Android notification "Your cafe needs you!" (throttled 30 min) via `javaSendNotification` -> AlarmManager | implemented + built |
| Auto-serve: `characterCanFollow` gate forced eligible (staff carry dishes at full pace) | implemented + built |
| Serve guard: busy-spot (CafeObject+0x2e) serves divert to the engine's return-dish path (overwrite fix) | implemented + built |
| Unit tests (155 checks incl. `last_cooked_row` roundtrip) | PENDING (phone unplugged at test time; 153/153 passed pre-change) |
| Device verification (auto-cook log lines, serve-guard lines, notification visible) | PENDING (phone unplugged) |

### 11.2 Honest labels

- RE-verified: cook ABI, cost/money fields, stove list, ServeTask flow,
  +0x2e flag lifecycle (set by FindSeatTask/ServeTask, cleared by EatTask),
  notification AlarmManager semantics.
- Code paths built clean; behavior on device not yet observed (the hook
  points redirect PC inside engine frames - same technique as the zukan
  gate, but ServeTask/characterCanFollow have not been exercised live).
- Risk notes: auto-serve forces eligibility for all staff/customer pairs
  (original is probabilistic); if live testing shows over-eager task
  churn, the gate needs pacing. The serve guard assumes +0x2e semantics
  hold for all target types (verified for seats + the ServeTask target).

## 11.1 Device verification update (2026-11-02, later session)

| Component | Status |
|---|---|
| Auto-cook with player add-on (bulk): `variant=5 cost=16 money=13572->13556 foodstack=...` | DEVICE-TESTED (cost 16 = the cook UI's BULK $16 exactly; coins deducted; cooking started) |
| Chef loop (walk/cook/carry): `chef assigned to stove=...`; chef visibly tending the cooking pot | DEVICE-TESTED |
| Auto-serve (done dish -> counter): armed via GameStateCafe+0x54; chef loop carry path | implemented + armed (carry event visual confirmation pending) |
| Hands-free/idle operation + background catch-up (pause leaves stoves cooking; wall-clock progress continues) | implemented + built |
| Serve guard (busy-spot divert) | implemented + built (live repro pending) |
| Staff cleanup gate (dirty fridge/sink "!") forced eligible (`auto-serve gate: forced N eligible checks`) | implemented + firing on device |
| Unit tests | 158/158 PASS |

### 11.2 RE corrections (verified against raw bytes)

- Stove selection must use `Cafe::createDistanceSortedStoveList(cafe,
  recipe+0x1a cookType, 0)` and the **+0x2e busy flag** - the generic
  furniture list matched the wrong appliance (the fridge), and
  Stove+0x60 goes stale after a dish is carried (false "busy").
- The chef loop (GameStateCafe::tick): `Stove::isBurned` -> CleanupTask;
  `Stove::isDoneCooking` -> DishToCounterTask (the auto-serve carry);
  else CookTask (tend). Driven by the stove assignment at +0x54.
- The tickStaff CleanupTask gate (dirty furniture, incl. fridge/sink)
  is `characterCanFollow` - covered by the forced auto-serve gate.
