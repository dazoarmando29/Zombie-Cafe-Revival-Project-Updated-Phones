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

## 3. Adding a new character texture sheet (the "third sheet")

Tool side — **already supported** (data driven since this upgrade):

1. Create `src/assets/images/characterParts3/` with character folders.
2. `build_tool` globs `characterParts*` and packs each sheet to
   `characterParts3.cct.mid`, `characterOffsets3.bin.mid`,
   `characterArt3.bin.mid` automatically (`characterSheetOutputs()` in
   `tool/resource_manager/serialization/serialization.go`).

Engine side — **structures are generic; two unrolled load blocks are the
limit** (Phase 6 investigation, dynamic tracing + symbol-table RE):

**What the engine binary actually contains** (verified 2026-11-01):

* The engine exports its **full C++ symbol table** (~6,270 dynsyms —
  `llvm-readelf --dyn-syms`). Classes like `ZombieCafe`, `PackedImage`,
  `CCAssetManager`, `ResourceManager`, `Constants` are all named.
* `ZombieCafe::loadImagePack(String name, unsigned char index)` (RVA
  0x125d05) is a **generic pack loader**: it validates the pack index in
  **1..254**, appends to a dynamically grown table (24-byte records,
  `operator new[]` growth), and resolves packs **by name** through
  `ZombieCafe::getImageSubpack(String)` (failure logs
  `DID NOT FIND PACK NAME` / `getImageSubPack FAILED` exist in the binary).
* `PackedImage::createPackedImage(String, int, String, int)` is the
  generic atlas constructor; filename variants are selected through
  device-indexed global `String` arrays (`SmDev()` returns the device
  index; `iphone3_`/`iphone4_`/`images/` prefixes are per-device entries).
* In `ZombieCafe::initData` (RVA 0x12b850, a 13-step load state machine,
  `Loadstep=%d` logged), the two character sheets are loaded by **two
  unrolled code blocks** (NOT a loop): block 1 parses
  `data/characterArt.bin.mid`, calls `createPackedImage`, and stores the
  `PackedImage*` at `ZombieCafe+0x14c`; block 2 does the same for sheet 2
  and stores at `ZombieCafe+0x16c`. The blocks differ only in GOT string
  slots and destination fields.
* Runtime `[filetrace]` (added in this branch) confirms the startup order:
  `characterArt` → `characterParts.cct` + `characterOffsets` →
  `characterArt2` → `characterParts2.cct` + `characterOffsets2`, then
  `characterData`. Each `fileRead` trace carries the emulated caller PC
  and stack, which is how the load blocks were located.

**Proof of concept (tool side, done):** a synthetic `characterParts3/`
tree (54 real pieces reused as a fixture) packs through the actual
pipeline into `characterParts3.cct.mid` (23.5 KB atlas),
`characterOffsets3.bin.mid` and `characterArt3.bin.mid`. The tool chain
fully supports N sheets.

**Exact remaining blocker (engine side):** `initData` contains exactly
two unrolled sheet blocks. Adding a third sheet requires replicating
block 2 in a code cave (the cave page pattern used by the quest patches
at 0x11000000 works and is within BL range), wiring:

1. three new filename `String`s (`data/characterArt3.bin.mid`,
   `images/characterParts3.cct.mid`, `images/characterOffsets3.bin.mid`)
   plus a device-variant entry (can reuse the `images/` variant only);
2. the art-list parse + `createPackedImage` sequence (identical to
   block 2, ~150 Thumb instructions including the readUTF list loop);
3. a third `PackedImage*` field — `ZombieCafe` object layout has not
   been fully mapped beyond +0x14c/+0x16c, so a safe location (e.g. a
   host-side side table keyed by the returned pointer, or a verified
   free field) must be established;
4. every consumer that iterates sheets 1..2 (character art resolution
   uses the two art lists; `ReloadTextures` at RVA 0x12b1ad has its own
   `loadImagePack` call at 0x12b514) must also cover sheet 3.

Alternative path: the data-driven manifest loop in `initData` (reads
`readUTF, readUTF, readByte` entries from a `DataInputStream` and calls
`loadImagePack(name, index)` for each) is the *intended* generic
mechanism — identifying its source data file (the stream is opened
earlier in the same load step; it is one of `constants.bin.mid` /
`globalData.dat`, neither of which contains the names as plain text)
would make a third sheet a **data-only** addition. This is the preferred
next investigation.

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
of new characters/sheets is comfortably within budget; hundreds of new
2048² atlases would add ~16 MB GPU each and should be checked against the
`[texstat]` report after loading.
