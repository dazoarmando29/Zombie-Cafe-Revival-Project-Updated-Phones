# Known Limitations — Zombie Café Revival (engine upgrade branch)

## Engine / content

1. **Third (and N-th) character texture sheet: IMPLEMENTED** (2026-11-01,
   `feature/content-expansion`). Extra sheets (`characterParts3` ..
   `characterParts12`) load through host-side engine hooks
   (`host/zcr_sheets.cpp`) that call the engine's own
   `createPackedImage`/`createSubpack`; `getImageSubpack` misses are
   answered from the extra sheets. Verified on device with sheets 3 and 4
   and two appended test characters. See
   `docs/ASSET_EXPANSION_GUIDE.md` §3 for the verified mechanics.
   Remaining gap: on GL context loss the superseded `PackedImage` heap
   objects of the extra sheets are not freed (a few KB per loss).
2. **255 characters / 255 foods format cap** — the count fields are one
   byte (`tool/file_types`). 221 characters ship today (219 original + 2
   test fixtures). Lifting this requires an engine-side format change
   (risky for saves).
3. **Furniture packs: 3 loaded by the engine** (furniture/furniture2/
   furniture3 filename literals). Same engine-side constraint the
   character sheets had; the character-sheet hook technique has not yet
   been applied to furniture.
4. **Data row order is load-bearing.** The engine addresses characters/
   furniture by implicit index; inserting rows shifts ids and breaks saves.
   Append-only is enforced by convention and checked by the asset
   validator's duplicate/art checks, but not by a dedicated row-identity
   tool yet.

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
    (V2318). Android 14, low-RAM and x86 emulators are untested in this
    branch. The **armeabi-v7a path does not load on Android 15 at all**
    (pre-existing: the original 32-bit engine has text relocations, which
    `dlopen` rejects since API 23); it remains functional only on legacy
    devices (Android < 6), where characters referencing `characterParts3+`
    art now render with missing art instead of crashing (null-guard patch,
    see `docs/CONTENT_EXPANSION_TEST_REPORT.md` §5).
14. **Long-session behavior** (multi-hour) is unverified; the periodic
    reports exist precisely to catch slow leaks during such sessions.
15. **Mod-menu variant** builds were not exercised by the new tests
    (inert code paths in vanilla build).
16. The engine's known String-copy crash sites (0x10141bd4/0x10141c42)
    have full diagnostics (`[strchain]`) but were not reproduced here;
    the root cause is still open.
