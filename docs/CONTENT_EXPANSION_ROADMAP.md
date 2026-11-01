# Content Expansion Roadmap — Zombie Café Revival

Status codes: **done** (implemented + tested), **dev-tested** (implemented,
statically tested, device test pending), **prototype**, **planned**,
**blocked**. Details and evidence: `docs/CONTENT_EXPANSION_TEST_REPORT.md`.

## Stage A — expansion foundations

| Item | Status |
|---|---|
| N-sheet character packing (tool, data-driven) | done |
| N-sheet runtime loading (sheets 3..12, host hooks) | done (device-tested, sheets 3+4) |
| Character references on new sheets resolve | done (device-tested) |
| Fourth-sheet scalability proof | done (device-tested) |
| Missing-art crash guard (both engine paths) | done (device-tested on arm64; 32-bit path unrunnable on Android 15 for unrelated reasons) |
| Content registry + validation + build gate | done (unit-tested; gated in build.ps1) |
| Content expansion guides | done (NEW_CHARACTER_GUIDE, NEW_RECIPE_GUIDE, CONTENT_REGISTRY, ASSET_EXPANSION_GUIDE §3) |

## Stage B — quality-of-life (order of priority)

| Item | Status |
|---|---|
| Recipe favorites | planned (needs engine UI work or host overlay) |
| One-tap repeat cooking | planned |
| Improved zombie management / staff overview | planned (candidate vehicle: host overlay or mod-menu extension) |
| Recipe sorting | planned |
| Toxin-spending confirmation | planned (engine dialog hook) |
| Raid info / history | planned |
| Multi-stove cooking | planned (investigate stove state machine first) |
| Cooking queue / food expiration options | planned (compatibility layer only if stable) |

Reasoning: the original engine draws all gameplay UI internally; each
item needs either a verified engine patch or a non-invasive host overlay.
Nothing here is blocked, but none of it is safe to claim before the
engine-side interaction points are RE-verified like the sheet work was.

## Stage C — first content expansion

| Item | Status |
|---|---|
| New recipe (`recipe.brain_burger`) | dev-tested (data + art pack verified at load; cookbook/cooking walkthrough pending manual test) |
| New zombie (real, original art) | planned (pipeline proven by test fixtures; original artwork is the missing dependency) |
| New furniture item | planned (same pipeline, furniture packs capped at 3) |
| New enemy café | planned (raid data format to be RE-mapped first) |
| Recipe mastery prototype | planned (needs cooking-completion event hook) |
| Initial achievement system | planned (host-side companion save; see Part 11) |

## Stage D — expand content (blocked on Stage C sign-off)

More zombies (target 10 original characters), 15–20 recipes, themed
furniture sets, more enemy cafés, expanded achievements. Content volume
is gated on the first examples being fully device-tested.

## Stage E — advanced systems

Zombie traits, boss raids, district progression, seasonal events, story
missions, layout presets. All planned; all depend on the host-side
companion-save infrastructure that Stage C's achievement prototype will
introduce.

## Cross-cutting rules

* All core single-player content stays offline; companion saves are
  local, versioned, and never touch the engine save format.
* Content is data-driven and registry-pinned (append-only rows).
* Every engine interaction is RE-verified before patching; no invented
  offsets.
* Device-verified claims only in the test report.
