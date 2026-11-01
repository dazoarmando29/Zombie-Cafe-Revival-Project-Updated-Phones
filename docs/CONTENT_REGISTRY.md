# Content Registry — Zombie Café Revival

Stable content identifiers and append-only enforcement layered over the
engine's native data files. Implemented by `tool/content_registry`
(Go, stdlib only) and gated in the build (`tool/build_tool` runs it before
packing).

## Why

The engine addresses content rows **by index** (characters, foods and
furniture are referenced positionally from saves and data), so rows must be
appended, never reordered or removed. Nothing enforced that until now. The
registry also gives every content row a **stable id** that future systems
(host-side achievements, quests, themes, collection book, migrations) can
reference without depending on row positions.

## Design

* The `*.bin.mid.json` files remain the single source of truth. Each row
  gets a `ZcrId` field; the packer ignores unknown fields (verified:
  `tool/file_types` decodes only known fields), so this is save- and
  engine-neutral.
* `src/assets/data/zcr_registry.lock.json` pins every id to its row index.
  The build fails if a pinned row moves, disappears, or changes id.
  Content *edits* are fine (the lock stores a content hash for information
  only); *moves and deletions* are errors.
* No new serialization format is introduced: the lockfile is plain JSON
  managed by the tool.

## Id convention

```
<category>.<slug>[.<slug>...]
```

Categories in use: `char`, `recipe`, `furniture` (tracked engine data).
Future systems should use: `theme`, `enemy_cafe`, `raid`, `achievement`,
`quest`, `event`, `collectible`.

Rules (enforced):

* lowercase `[a-z0-9_]` segments separated by dots, first segment starts
  with a letter,
* unique across **all** tracked files,
* never reused after a row is retired (add a new id instead).

Examples: `char.man`, `char.zcr_sheet3_tester`, `recipe.brain_burger`,
`furniture.haunted_table`.

## Row fields

| Field | Type | Meaning |
|---|---|---|
| `ZcrId` | string | stable id (required) |
| `ZcrDependsOn` | list of ids | soft prerequisites (optional); validated to exist and be acyclic |

`ZcrDependsOn` is reserved for content that unlocks on other content
(e.g. a recipe that requires a themed stove). Current data does not use it
yet; the validator already enforces correctness when it appears.

## Workflow

```bash
# one-time (or after pulling content without ids): assign ids + write lock
go run ./tool/content_registry -root src/assets init

# after adding new rows: re-pin the lockfile (appends only)
go run ./tool/content_registry -root src/assets lock

# validate (also runs automatically in build.ps1 via build_tool)
go run ./tool/content_registry -root src/assets check
```

Adding content: append the row to the data JSON **at the end**, give it a
fresh `ZcrId`, run `lock`, run `check`. See `docs/NEW_CHARACTER_GUIDE.md`
and `docs/NEW_RECIPE_GUIDE.md` for end-to-end recipes.

## Save migration

The engine save format is untouched. Row identity rules (append-only,
never reuse indexes) are exactly what keeps existing saves loading. If a
future system adds its own save data (host-side companion file), it must
reference content by `ZcrId` and keep an id→index map regenerated from the
lockfile at load time.

## Testing

`tool/content_registry/registry/registry_test.go` covers id validation,
deterministic id assignment (formatting-preserving), duplicate/missing id
detection, lockfile round-trip, reorder/removal detection, dependency
resolution and cycle detection. The build gate is exercised by every
`build.ps1` run.
