# Asset Expansion Guide — Zombie Café Revival

How to add new content (characters, sheets, furniture, food, …) to the
game without touching unrelated engine components, and exactly where the
hard engine limits currently sit.

## 1. Pipeline overview

```
src/assets/images/characterParts*/<art>.png     human-editable sources
src/assets/data/*.bin.mid.json                  human-editable data
        │
        ▼  go run ./tool/build_tool -i src -o build   (runs the validator first)
build/assets/...*.cct.mid / *.bin.mid           packed game formats
        │
        ▼  apktool + libs (build.ps1)
out.apk
```

* `tool/asset_validator` — validates every source file against the real
  format limits before packing. The build fails on `[error]` with the
  offending file and reason. Run standalone:
  `go run ./tool/asset_validator -root src/assets` (add `-strict` to also
  fail on warnings).
* `tool/resource_manager/serialization` — packs/unpacks data and texture
  atlases (`pack`, `unpack`, `pack_characters`, `unpack_characters`,
  `pack_textures`, `validate_save`, ...).
* `tool/cctpacker` — rect-packs PNG pieces into the `.cct.mid` atlas
  format the engine reads.

## 2. Adding characters (no new sheet)

1. Add art folders `src/assets/images/characterParts/<art>-human/` and
   `<art>-zombie/` with the **same 27 PNG pieces** as the existing
   folders (piece count must be uniform per sheet; the validator enforces
   this).
2. Add entries to `src/assets/data/characterData.bin.mid.json`:
   * `CharacterArtStringHead` / `CharacterArtString` must match a folder
     name or its `<name>-` prefix (e.g. `"oldman"` resolves to
     `oldman-human`/`oldman-zombie`). Body art may be empty for
     head-only characters.
3. Run `build.ps1`. The packer regenerates `characterParts.cct.mid`,
   `characterOffsets.bin.mid` and `characterArt.bin.mid`.

**Capacity**: `characterData` has a 1-byte count → **255 characters max**
(219 in the shipped data). The validator fails the build if exceeded.

## 3. Adding a new character texture sheet (the "third sheet") — IMPLEMENTED

Tool side — **data driven** (since the engine upgrade):

1. Create `src/assets/images/characterParts3/` with character folders.
2. `build_tool` globs `characterParts*` and packs each sheet to
   `characterParts3.cct.mid`, `characterOffsets3.bin.mid`,
   `characterArt3.bin.mid` automatically (`characterSheetOutputs()` in
   `tool/resource_manager/serialization/serialization.go`).
3. Point `CharacterArtStringHead`/`CharacterArtString` at folders inside the
   new sheet. The validator already resolves art strings across every
   `characterParts*` folder.

Engine side — **runtime support implemented** (2026-11-01, `feature/content-expansion`).
Verified mechanics (ELF dynsym + Thumb disassembly of the shipped
`libZombieCafeAndroid.so`, then confirmed on device):

* `ZombieCafe::loadImagePack(String, byte)` (RVA 0x125d04) resolves
  per-character art, not sheets: it formats `"%s-human"` / `"%s-zombie"`
  from the art string and calls `getImageSubpack`. The byte argument is the
  character form selector (`0`=human, `1`=zombie, `0xFF`=both — the `U4`
  field of `characterData`).
* `ZombieCafe::initData` (RVA 0x12b850, 13-step `Loadstep` state machine)
  loads the two shipped sheets with **two unrolled code blocks** (sheet 1 in
  load step 5, sheet 2 in load step 11): each parses
  `data/characterArtN.bin.mid` (`PiecesPerString` byte + UTF names until an
  empty string), keeps the name list on the `ZombieCafe` object
  (head +0x13c / +0x15c, tail +0x140 / +0x160, count +0x148 / +0x168) and
  calls `PackedImage::createPackedImage(atlas, 0, offsets, 0)`, storing the
  `PackedImage*` at **+0x14c / +0x16c** (`PiecesPerString` byte at
  +0x150 / +0x170). The two blocks differ only in GOT string slots and
  destination fields.
* `ZombieCafe::getImageSubpack(String)` (RVA 0x125ac8) searches the two
  name lists and, on a hit at list index `i`, returns
  `PackedImage::createSubpack(i * PPS, PPS)` on the matching sheet's
  `PackedImage` — subpacks are index-range views into the atlas (rects are
  addressed by position, one `PiecesPerString`-sized group per art name).
  Its single not-found entry is RVA 0x125bbe; every exit passes through the
  shared epilogue at RVA 0x125bcc with the result in `r4`.
* Per-character resolution order at runtime (confirmed via `[filetrace]`):
  `characterArt` → `characterParts.cct` + `characterOffsets` →
  `characterArt2` → `characterParts2.cct` + `characterOffsets2` →
  `characterData`.

**Runtime extension** (`host/zcr_sheets.cpp`, no engine bytes modified):

* A code hook at RVA 0x12c5ba (the instruction right after sheet 2's
  `createPackedImage` block) creates the `PackedImage` for every extra
  sheet discovered in the APK assets (`characterParts3` … `characterParts12`
  with matching `characterArtN.bin.mid` / `characterOffsetsN.bin.mid`,
  probed via `AAssetManager`) by calling the engine's own
  `PackedImage::createPackedImage`, and copies the device scale fields
  (PackedImage +0x18/+0x1c) from sheet 2.
* A code hook at RVA 0x125bbe (the `getImageSubpack` not-found entry)
  answers misses from the extra sheets via the engine's
  `PackedImage::createSubpack`, returning the subpack through `r4` and
  resuming at the shared epilogue. Exact string match against the art list
  parsed from `characterArtN.bin.mid`.
* A code hook at RVA 0x12b1ad (`ZombieCafe::ReloadTextures` entry)
  re-creates the extra sheets on GL context loss, before the engine
  re-resolves character art. Superseded `PackedImage` objects are
  abandoned on the guest heap (a few KB per context loss).
* Nested engine calls run on a stack frame below the interrupted frame
  (`jni_call_engine_r0_sp`) and **must carry the Thumb bit on the RVA**
  (engine export RVAs are odd; an even entry address selects ARM mode and
  the Thumb code decodes as garbage).

**Verified on device** (Android 15, arm64): sheets 3 and 4 load at startup
(`[sheet] sheet 3 loaded ... pi=...`), all four test art names resolve
(`[sheet] resolved zzsheet3-human via sheet 3 idx=0 ...`), existing
characters and animations render unchanged, the pre-existing Level-9 save
loads and plays, saves write, `emu_errors=0` over 600k+ draws.

The mechanism is **N-sheet capable** (3..12 probed); sheets 3 and 4 are
both populated and exercised by the test characters appended to
`characterData` ("ZCR Sheet3/4 Tester" — recycled development art,
explicitly not final artwork).

Known gap: `ReloadTextures` covers the extra sheets via the entry hook, but
the superseded `PackedImage` heap objects are not freed (bounded, small).

## 4. Adding furniture / food / recipes

* Furniture: add entries to `furnitureData.bin.mid.json` and PNGs to the
  right `furniture*/` folder. `ImagePackIndex` selects the pack
  (0–2; the validator warns beyond that). Count field is int32 — large
  capacity.
* Food: `foodData.bin.mid.json` (1-byte count → 255 max) plus a recipe
  image in `recipeImages*/`.
* New atlas folders for furniture/recipes require the corresponding
  engine filename string (`furniture4.cct.mid` etc.) — same engine-side
  constraint as character sheets, except furniture packs have 3 in the
  shipped engine.

## 5. Stable resource identity

* Art references use **folder names** (`CharacterArtString*`), not load
  order — safe to reorder folders.
* Rect names inside offsets files (`Offset.Name`) are the stable identity
  for UI/texture rects (Type 2 offsets). The validator flags duplicates.
* Data rows are addressed by index in the original engine (character ids
  are implicit order). **Append new rows at the end**; inserting shifts
  ids and will break existing saves that reference them.

## 6. Save compatibility rules

* Never remove or reorder existing `characterData` rows.
* New fields appended to JSON are fine (the packer writes only known
  fields); changing field semantics is not.
* `tool/resource_manager -m validate_save` validates a save blob against
  `tool/file_types/save_game.go` before/after changes.

## 7. Validation cheat sheet

| Check | Tool | Failure |
|---|---|---|
| character count ≤ 255 | `asset_validator` | build error |
| art string resolves to a folder | `asset_validator` | build error |
| uniform piece count per sheet | `asset_validator` | build error |
| offsets rect sizes > 0 | `asset_validator` | build error |
| CCTX magic + sane dims | `asset_validator` | build error |
| save blob parses | `resource_manager -m validate_save` | manual |
| atlas packs cleanly | `cctpacker` (during build) | build error |

## 8. Runtime memory headroom

From measured steady state (see `docs/ENGINE_MEMORY_AUDIT.md`): the guest
heap peaks at ~27 MB of 1536 MB and GPU textures at ~53 MB. Adding tens
of new characters/ sheets is comfortably within budget; hundreds of new
2048A� atlases would add ~16 MB GPU each and should be checked against the
`[texstat]` report after loading.

## 9. Verified engine gameplay transitions (companion layer hooks)

Ground truth for `host/zcr_companion.cpp` (recipe mastery + achievements).
All findings verified by Thumb disassembly of the shipped
`libZombieCafeAndroid.so` (2026-11-02, capstone) and cross-checked against
runtime behavior; do not re-derive, and re-verify before changing.

### 9.1 "A dish was cooked" - `DishToCounterTask::completeObjective` (RVA 0x82048)

* The engine's own "you cooked this" accounting lives here: the function
  calls `ObjectiveSystem::cookFood(recipeRow)` (`ldrb r1, [r3]` passes the
  recipe row byte) from exactly this spot, then grants recipe XP.
* `DishToCounterTask+0x3c` = the carried `FoodStack*`; `FoodStack+0` =
  recipe row (byte), `FoodStack+8` = variant (byte), `FoodStack+4` =
  servings. Verified from `FoodStack::FoodStack(uchar,uchar)` (0x8a33c).
* Both delivery paths flow through this task: staff carry
  (`GameStateCafe::onCookCommandBarServe` 0xa5dc4 creates the task after
  `findStaffForCookEndTask`) and player-tap serve. `CookCommandBar::finishDish`
  (toxin instant-finish) and `CookingOrder` (friend orders) converge on the
  same dish state.
* Call sites: tick branches 0x82c8a / 0x82d7e and stop-immediately branches
  0x83056 / 0x830f4 - matching the engine's own `cookFood` semantics (burned
  unclaimed dishes are not counted, aborted carries are).

### 9.2 Stove / dish state (verified fields)

| Field | Meaning |
|---|---|
| `Stove+0x2e` (byte) | cooking-active flag (set by `startCooking`) |
| `Stove+0x60` | `FoodStack*` of the dish being cooked |
| `Stove+0x64` | accumulated wall-clock ms (advanced by `Stove::exist`) |
| `Stove+0x68` | ready marker: 0 while cooking, cook-time ms once `autoFinishCooking` ran |
| `Stove+0x3c` | `Furniture*` back-pointer (task routing) |

* `Stove::startCooking(uchar row, uchar variant, Furniture*)` (0x113c58)
  builds the `FoodStack` (0x24 bytes) at +0x60.
* `Stove::isDoneCooking` (0x77ecc): `[0x68] >= cookTimeMs`.
* `Stove::autoFinishCooking` (0x113bb0) is the ready transition (sound +
  notification); called from `CookCommandBar::finishDish` (0x7eace) and
  `CookingOrder::finish` (0xf2a54) only - the timer path transitions
  lazily via `isDoneCooking`.

### 9.3 Toxin

* `ZombieCafe+0xb8` (int32) = toxin balance. `spendToxin(int)` (0x11fc94)
  is a 4-instruction subtract; `getToxin` (0x11fca0) loads the same field;
  `addToxin(int,bool)` (0x122134) adds it and, when `flag != 0` (purchase
  flow - `GiveBoughtToxin` passes 1), reports through a side channel.
  The mod-menu natives (`DebugSetToxin` etc.) write the same field.
* Hook points used by the companion layer: `spendToxin` entry (r1=amount)
  and `addToxin` entry (r1=amount, r2=flag; gameplay earnings = flag 0).

### 9.4 Game instance lookup

* `CapcomApp**` global at RVA 0x1daf80; `*(app+4)` = `ZombieCafe*`
  (same verified indirection as the mod-menu natives in `zcr_host.cpp`).

### 9.5 Recipe favorites (cookbook stars)

* `Recipe+0x1e` (byte) = favorite flag; `Recipe+0x1f` = companion "seen"
  byte. Toggle paths in `RecipeBook::tick` write the flag and call
  `RecipeBook::updateFavorite(bool)` (0xfb8e8) with the selected row at
  `RecipeBook+0x214`; `RecipeBook::manageAllRecipe` re-applies the star
  icons from `+0x1e` on every view build.
* Favorites slot list widget at `RecipeBook+0x64`: capacity at +0x38,
  count at +0x3c, `int*` rows at +0x40 (entries at 1..count), engine cap
  14 entries. Grow primitive at RVA 0xfb7c0: `grow(list+0x38, newCap)`.
* The flag DOES round-trip through the engine save (observed on device),
  but with engine-save timing: toggles made after the last engine save
  resurrect on reload. The companion layer
  (`host/zcr_companion.cpp`) therefore treats its own write-through state
  as truth: `updateFavorite` hook records toggles instantly and
  `RecipeBook::loadCookBook` (0xffa1c) reconciles before the UI builds
  (adopting engine-restored favorites when the companion has no records,
  writing both 1s and 0s otherwise).
* Note: the detail panel's ADD/REMOVE-star button label can show the
  engine's stale default on the auto-selected recipe right after opening
  the book; it refreshes on any recipe selection (self-correcting).
