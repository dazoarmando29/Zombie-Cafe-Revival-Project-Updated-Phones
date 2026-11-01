# Known Limitations — Zombie Café Revival (engine upgrade branch)

## Engine / content

1. **Third character texture sheet: engine-side loading is not yet
   wired.** The tool chain packs any number of `characterPartsN` folders
   (proven with a `characterParts3` PoC), and the engine's structures are
   generic (`loadImagePack` accepts pack indexes 1..254 with a dynamic
   table), but `ZombieCafe::initData` loads the two sheets from two
   unrolled code blocks storing to fixed object fields (+0x14c/+0x16c).
   A third sheet needs a cave patch replicating that block (or discovery
   of the data-driven manifest source). See `docs/ASSET_EXPANSION_GUIDE.md`
   §3 for the full RE findings and the exact patch plan.
2. **255 characters / 255 foods format cap** — the count fields are one
   byte (`tool/file_types`). 219 characters ship today. Lifting this
   requires an engine-side format change (risky for saves).
3. **Furniture packs: 3 loaded by the engine** (furniture/furniture2/
   furniture3 filename literals). Same engine-side constraint as sheets.
4. **Data row order is load-bearing.** The engine addresses characters/
   furniture by implicit index; inserting rows shifts ids and breaks saves.
   Append-only is enforced by convention, not by tooling yet.

## Memory / graphics

5. **Duplicate texture uploads are detected but not eliminated** (7 at café
   load). Sharing GL objects across engine-visible texture ids would
   change per-id parameter/lifetime behavior; needs engine-side id
   remapping to fix safely.
6. **Texture byte accounting is a host-side estimate** (dimensions ×
   format); driver-side usage (EGL mtrack) runs ~2× higher due to driver
   copies/mips/alignment.
7. **Heap size is fixed at 1536 MB virtual.** Measured peak is 27 MB, so
   growing it is neither needed nor justified. On very low-RAM devices the
   lazy-zero reservation is virtual-only (RSS tracks touched pages), but
   the choice has not been tested below 4 GB RAM.
8. **No 4444 re-encoding of the large atlases** — would save ~24 MB GPU
   but risks visible color banding; deliberately not changed.

## Saves / offline

9. **`.bak` backup throttle is 10 s** — in the worst case the backup is up
   to ~10 s older than the last save. A crash inside a *new* save burst can
   still lose that burst's changes (the previous save survives in `.bak`).
10. **Interrupted-save recovery is implemented but not fault-injected**
    (no kill-during-save test has been run yet).
11. **Clock manipulation**: offline cooking/recovery timers use wall-clock
    time (`CCSecondsSince1970`). Moving the device clock forward completes
    timers instantly; this is inherent to offline timers and is not
    clamped (clamping would break legitimate time changes).
12. **No online services** are provided; friend/online quest types are
    completed by engine patches (existing behavior, unchanged).

## Testing / compatibility

13. **Device coverage**: verified on one Android 15 arm64 device
    (V2318). Android 14, low-RAM, x86 emulators and the armeabi-v7a path
    are untested in this branch.
14. **Long-session behavior** (multi-hour) is unverified; the periodic
    reports exist precisely to catch slow leaks during such sessions.
15. **Mod-menu variant** builds were not exercised by the new tests
    (inert code paths in vanilla build).
16. The engine's known String-copy crash sites (0x10141bd4/0x10141c42)
    have full diagnostics (`[strchain]`) but were not reproduced here;
    the root cause is still open.
