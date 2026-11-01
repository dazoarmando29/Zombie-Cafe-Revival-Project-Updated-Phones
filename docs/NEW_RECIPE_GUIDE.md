# New Recipe Guide — Zombie Café Revival

How to add a new recipe end-to-end (worked example: `recipe.brain_burger`).

## 1. Art

1. Add a plate icon to `src/assets/images/recipeImages/` named
   `<NNN>-<slug>.png`, where `NNN` is the **next free 3-digit number**
   (the packer sorts by this leading number and the recipe's `ImageID`
   equals its sorted position — shipped data is contiguous `000`..`144`,
   so a new recipe uses `145-...png`).
2. Icons are small plated dishes (~128 px, transparent background); copy
   an existing icon's dimensions for consistency.
3. `recipeImages2` holds variant art referenced by other fields — do not
   mix numbering between the folders.

Artwork must be original or appropriately licensed; reusing an existing
icon is acceptable only as a clearly-labelled development placeholder.

## 2. Data

Append one row to `src/assets/data/foodData.bin.mid.json` (**at the end**):

| Field | Meaning | Balance notes |
|---|---|---|
| `ZcrId` | stable id, e.g. `recipe.brain_burger` | required |
| `Name` | display name | keep the punny style of existing recipes |
| `Price` | stove cost to start the batch | scale with unlock level |
| `UnlockLevel` | player level gate | |
| `CookTimeMinutes` | batch duration | real minutes |
| `Servings` | servings per batch | |
| `PricePerSeving` | revenue per serving | total earnings = servings × this |
| `ExperiencePoints` | XP per batch | |
| `ImageID` | rect index in `recipeImages` (= filename number) | |
| `U7` | tracks unlock-level band (0..N in shipped data) | copy from a same-level recipe |
| `U8`, `U9`, `U10`, `U11`, `U12` | unknown/variant fields | copy from a same-level recipe |

Balance example (level 8 band, `recipe.brain_burger`): Price 400, 420
minutes, 420 servings, 3 per serving, 60 XP — i.e. between the existing
level-8 recipes "Mac & Fleas" and "Wormicelli". Compare profit/hour and
XP/hour against the two nearest-level recipes before committing.

## 3. Validate and build

```bash
go run ./tool/content_registry -root src/assets lock
go run ./tool/asset_validator -root src/assets
.\build.ps1
```

## 4. Test

1. Boot the game; confirm it reaches the café with `emu_errors=0`
   (bad `ImageID` values fault during `foodData` parse).
2. Open the cookbook: the recipe appears at the end of the book
   (recipes are listed in data order).
3. Cook it on a stove once unlocked; confirm timer, servings and
   earnings match the data.
4. Save/load round-trip.

## 5. Checklist

- [ ] icon added at the next free number
- [ ] row appended at end, `ZcrId` set, lock regenerated
- [ ] profit/XP per hour within the surrounding level band
- [ ] U-fields copied from a comparable recipe
- [ ] validator + build pass
- [ ] device test: loads, cooks, sells, persists
