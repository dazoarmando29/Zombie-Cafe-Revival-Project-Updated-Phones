# Quality-of-Life Features — Zombie Café Revival

Status tracker for Part 4/7/10 improvements. Nothing here is marked
complete until device-tested; see `docs/CONTENT_EXPANSION_TEST_REPORT.md`
for evidence and `docs/CONTENT_EXPANSION_ROADMAP.md` Stage B for order.

## Cooking

| Feature | Design notes | Status |
|---|---|---|
| Auto-cook | Idle stoves (engine-accurate pick: recipe cook-type list + busy flag) automatically cook the most recently cooked recipe **with the add-on you pick** (per-recipe variant recorded from your manual cooks; default BULK = variant 5, 2x cost/double servings). Coins short -> "Your cafe needs you!" notification (throttled 30 min). Runs hands-free (AFK/idle) and does a catch-up pass when you leave; dishes cooking by wall-clock continue while the app is away. | implemented + device-tested (2026-11-02: `variant=5 cost=16 money=13572->13556`) |
| Auto-serve | The chef loop (GameStateCafe+0x54 assignment) walks the chef to the stove to cook and **carries the done dish to the serving counter automatically**; staff table-serving and dirty-appliance cleanup gates are forced eligible. | implemented + device-tested (chef at stove with cooking pot, dish carry path armed) |
| Serve overwrite fix | `ServeTask` placed dishes without checking the spot's in-use flag (CafeObject+0x2e) - a second dish landed on an occupied spot and the new dish vanished. Guard hook diverts busy-spot serves to the engine's own return-to-counter path. | implemented + built; live repro pending (guard log lines) |
| Recipe favorites | The engine already ships the star UI (toggle, favorites tab, 14-slot list) but state round-trips through the engine save with save-timing staleness. Companion save now records toggles write-through and reconciles at book-open (adopt-on-empty / companion-wins). | implemented + device-tested (2026-11-02, test report A9) |
| One-tap repeat cooking | Remember last recipe per stove (companion file); re-issue the cook command after affordability checks. Must not spend currency before confirmation. | planned |
| Multi-stove cooking | Extend repeat-cooking to apply to selected idle stoves; validate each stove independently; never touch active cook slots. | planned (investigate stove state machine first) |
| Recipe sorting | Sort options (time/profit/XP/servings/mastery/favorites) with data-order as default. | planned |
| Quick sale (optional) | Sell completed food at 50–70% revenue; configurable; normal sales remain more profitable; must not bypass recipe progression. | planned |
| Cooking queue | One queued recipe per stove; only if the cook state machine tolerates it; no server timers. | planned (feasibility unknown) |
| Food expiration grace | Casual mode with grace period selectable alongside classic behavior; never silently change existing saves. | planned |

## Staff management

| Feature | Design notes | Status |
|---|---|---|
| Staff overview screen | One-screen health/energy/role list. | planned (host overlay candidate) |
| Sorting staff | By health, energy, speed, role. | planned |
| Favorite/lock zombies | Protect valuable staff from accidental actions. | planned |
| Rest-all-exhausted | Batch rest command. | planned |
| Raid team presets | Save/load team compositions locally. | planned |
| Zombie collection book | Encyclopedic discovery list (uses `ZcrId`s). | planned |

## Economy

| Feature | Design notes | Status |
|---|---|---|
| Toxin-spend confirmation | Audit (ASSET_EXPANSION_GUIDE §9.6): 19/20 engine spend paths already confirm via the shared ConfirmDialog (cook instant-finish device-observed). The one gap (zookdex character purchase) is now gated with the engine's own dialog + cave re-entry (§9.7). | implemented + RE-verified; device verification pending (test report §10) |
| Earnable toxins | Limited rewards via achievements/mastery/quests; never infinitely farmable. | prototype (achievement rewards implemented: bounded, non-repeatable, see CONTENT_EXPANSION_TEST_REPORT.md §8) |
| Daily/weekly objectives | Local tracker + reward claiming with save/reload exploit protection. | planned |

## Raids

| Feature | Design notes | Status |
|---|---|---|
| Difficulty preview / estimated rewards | Show generated values only after the game generates them. | planned |
| Raid history + result summaries | Local log. | planned |
| Interrupted-raid recovery | Resume or refund flow. | planned |

## Accessibility & polish

| Feature | Status |
|---|---|
| UI scaling / larger touch targets | planned |
| Separate sound/music volume | planned |
| Optional notifications | planned |
| Reduced-animation / battery-saving modes | planned |
| Cooking completion indicators | planned |

## Implemented quality-of-life (this branch)

| Feature | Evidence |
|---|---|
| Missing-art crash guard (both engine paths) | `docs/CONTENT_EXPANSION_TEST_REPORT.md` §5 |
| Save backup + recovery (previous branch) | `docs/ENGINE_UPGRADE_REPORT.md` |
| Offline objective patches (previous branch) | `docs/ENGINE_UPGRADE_REPORT.md` |
| Recipe mastery tracking (per-recipe cook counts, ZcrId-keyed, companion save) | `docs/CONTENT_EXPANSION_TEST_REPORT.md` §8 |
| Achievements (15 data-driven defs, progress + bounded toxin rewards, claim latches) | `docs/CONTENT_EXPANSION_TEST_REPORT.md` §8 |
| Toxin-spend/earn tracking (foundation for spend confirmation + economy telemetry) | `docs/CONTENT_EXPANSION_TEST_REPORT.md` §8 (spend hook device-verified) |
| Companion save export/restore via SaveGuard | `docs/CONTENT_EXPANSION_TEST_REPORT.md` §8 |
| Recipe favorites persistence (write-through + book-open reconcile) | `docs/CONTENT_EXPANSION_TEST_REPORT.md` §9 |
