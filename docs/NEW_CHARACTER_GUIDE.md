# New Character Guide — Zombie Café Revival

How to add a new zombie character end-to-end. Prerequisites: Go toolchain,
`build.ps1` toolchain (see README), and this repo's asset pipeline.

## 1. Art

1. Create a folder pair per art set:
   * `src/assets/images/characterParts<N>/<art>-human/`
   * `src/assets/images/characterParts<N>/<art>-zombie/`
   `<N>` is the sheet number (1 = `characterParts`, then `characterParts2`,
   `characterParts3`, ... — any sheet from 3 up loads through
   `host/zcr_sheets.cpp`, see `docs/ASSET_EXPANSION_GUIDE.md` §3).
2. Each folder needs the **same 27 PNG pieces** as the existing folders
   (piece count must be uniform per sheet; the validator enforces this).
   Copy `generic-human/` as a structural reference. Optional
   `<piece>.png.json` files carry piece pivot offsets
   (`XOffset/YOffset/XOffset2/YOffset2`) and should be provided for
   animated pieces.
3. `<art>` may be reused across `-human`/`-zombie` only as the folder
   prefix; the engine resolves `"<art>-human"` and `"<art>-zombie"` as
   separate subpacks.

**Artwork rights**: use original or appropriately licensed art. Reused
pieces from existing folders are acceptable as clearly-labelled development
placeholders only.

## 2. Data

Append one row to `src/assets/data/characterData.bin.mid.json` (**at the
end** — rows are index-addressed, see `docs/CONTENT_REGISTRY.md`):

| Field | Notes |
|---|---|
| `ZcrId` | stable id, e.g. `char.plague_doctor` (required) |
| `CafeLevelRequired` | 0 = available from the start |
| `Name` | display name |
| `CharacterArtStringHead` | `<art>` of the head art set |
| `CharacterArtString` | `<art>` of the body art set (may be `""`) |
| `U4` | form byte: `0` human only, `1` zombie only, `255` both |
| `Energy`, `Speed`, `AttackStrength`, `TipRating` | stats (compare with similar characters) |
| `Cost`, `PurchaseWithToxin` | store purchase |
| `CookSpeedBonus`, `TipMultiplier`, `RegenBoost`, `CookXPBonus` | multipliers, `1` = neutral |
| `HumanDescription`, `ZombieDescription` | flavor text |

Keep new-character power within the range of existing characters of the
same tier (see `docs/GAMEPLAY_BALANCE.md`).

## 3. Validate and build

```bash
go run ./tool/content_registry -root src/assets lock   # pin the new row
go run ./tool/asset_validator -root src/assets         # 0 errors required
.\build.ps1                                            # gates on both
```

## 4. Test

1. Install and boot; watch `adb logcat -s zcr-sheet:I` for sheet loads
   and (for sheet 3+ art) `resolved <art>-human via sheet N`.
2. Confirm the character renders in game (customers spawn from
   `CafeLevelRequired` rows; staff appear after infection).
3. Save/load round-trip: force-stop, relaunch, confirm the character state
   persists.
4. 32-bit note: characters with `characterParts3+` art render with missing
   art on legacy armeabi-v7a devices (the supported modern path is arm64).

## 5. Checklist

- [ ] 27 PNGs per form, uniform across the sheet
- [ ] row appended at end, `ZcrId` set, lock regenerated
- [ ] stats balanced vs same tier
- [ ] descriptions written
- [ ] validator + build pass
- [ ] device test: art resolves, character behaves, save round-trip
