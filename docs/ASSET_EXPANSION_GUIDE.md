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

Engine side — **blocked, with evidence** (Phase 6 investigation):

* The English engine binary loads exactly two sheet triplets; the
  filenames are literal strings in `.rodata`
  (`images/characterParts.cct.mid` at RVA 0x1a4c08 and the `2` variant at
  0x1a4c64, plus offsets/art filenames). Exhaustive analysis shows the
  strings are referenced through PIC-computed addresses (no literal pool
  entries, no pointer table, no `adr`/`movw+movt` materialization found),
  so the load sites cannot be patched by swapping a filename constant.
* The Japanese 1.7 build shipped five sheets (`characterParts3..5` in the
  tool maps), proving the engine family's runtime structures support more
  than two sheets — the difference is inside the binary's load/indexing
  code, not the file formats.
* **Exact blocker**: the call sites that consume those two filename
  strings (and any loop bound / sheet-indexed arrays behind them) are not
  yet identified. Static reference tracing failed (see above); the next
  step is dynamic tracing (log every `fopen`/`fileRead` path and the
  sheet-index registers at load time) or a full disassembly of the asset
  manager class in `engine_loader`-mapped memory.

Until that is solved, a packed `characterParts3` is ignored by the engine
(harmless: it is simply never loaded).

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
