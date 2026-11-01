# Texture Memory Report — Zombie Café Revival

Measurements: Android 15 device (V2318, arm64), café loaded and idle,
using the `[texstat]` diagnostics added to `host/imports_gl.cpp`.

## Pipeline (as implemented)

1. The emulated engine decodes `.cct.mid` atlases (zlib-compressed
   RGBA4444/RGBA8888) in guest memory and calls `glTexImage2D` /
   `glCompressedTexImage2D` through the import bridge.
2. `host/imports_gl.cpp` resolves the pixel pointer zero-copy when it
   lives in an aliased region (heap/stack), otherwise copies once into a
   reused scratch buffer (`g_tmp_misc`) — the scratch buffer is never
   retained after the call.
3. The real GLES1 driver uploads to GPU memory. Decoded host-side buffers
   are not kept by the host; only the engine's own guest-heap copies
   remain until the engine frees them.

## Tracking added (this upgrade)

* Per-texture records keyed by GL id: dimensions, format, type, upload
  bytes, upload count (`tex_track_upload`).
* Global counters: live/peak bytes, created/deleted, cumulative uploaded
  bytes.
* **Duplicate detection**: FNV-1a content hash + dimensions/format at
  upload time; duplicates are logged (`[texdupe]`, first 8) and counted.
* `[texstat]` report (emitted with the periodic memory report): totals and
  the 10 largest live textures.
* `glDeleteTextures` decrements accounting and drops hash entries — safe
  GPU deletion is the engine's own call path, now observable.

## Measured steady state (café loaded, idle)

```
[texstat] live=22 peakKB=52904 liveKB=52904 created=22 deleted=0 dupes=7 uploadedKB=52904
[texstat] id=20 2048x2048 fmt=0x1908 type=0x1401 16384KB   (RGBA8888 atlas)
[texstat] id=18 2048x2048 fmt=0x1908 type=0x8033 8192KB    (4444 atlas)
[texstat] id=19 2048x2048 fmt=0x1908 type=0x8033 8192KB
[texstat] id=21 2048x1024 fmt=0x1908 type=0x1401 8192KB
[texstat] id=22 2048x1024 fmt=0x1908 type=0x1401 8192KB
[texstat] id=1  1024x1024 fmt=0x1907 type=0x1401 3072KB
```

* 22 live textures, **52.9 MB estimated GPU texture bytes** (host-side
  estimate from dimensions/format; driver overhead/mips not included).
* GPU side (`dumpsys meminfo`): **EGL mtrack 106 MB** at the same moment —
  roughly 2× the raw upload estimate, consistent with driver-side copies,
  alignment and mipmap state. Both figures are reported for comparison.
* **7 duplicate uploads** at café load (`[texdupe]`): identical pixel
  content uploaded under different GL ids.

## Findings

1. **The two 2048×2048 atlases (character sheets) dominate** (32 MB of the
   52.9 MB). They use RGBA8888 (0x1401) and one 4444 variant. The packer
   (`tool/cctpacker`) already supports rgba4444 (`make_4444` in
   `src/assets/images/`) — switching the 8888 atlases to 4444 would halve
   their cost at the price of color precision. **Not changed**: visual
   fidelity must be preserved (see below).
2. **Duplicate uploads (7) are waste** but cannot simply be shared: the
   engine may set different `glTexParameteri` per id and expects distinct
   object lifetimes. Sharing GL objects across engine-visible ids would
   change behavior; recorded here as a future optimization that requires
   engine-side id remapping, not a host-side alias.
3. **No decoded buffers are retained by the host** — the scratch copy is
   transient (`g_tmp_misc` reused per call). The engine's guest-heap copy
   of decoded pixels is freed by the engine's own destructors, which the
   ZCR patches intentionally keep alive (NOP-ing them — as upstream did as
   a scudo workaround — leaks every texture forever; see
   `engine_apply_zcr_patches()`).
4. Texture creation/deletion balance: 22 created / 0 deleted in the idle
   café — no texture churn leak observed in this scenario. The counters
   will show leaks in longer sessions (report every 1800 frames).

## Before / after

| Metric | Before upgrade | After upgrade |
|---|---|---|
| Texture tracking | none | per-id records + `[texstat]` |
| Duplicate detection | none | FNV content hash, `[texdupe]` |
| Measured GPU (EGL mtrack) | 92 MB (idle café) | 106 MB (café + more UI) |
| Estimated upload bytes | unknown | 52.9 MB, measured |
| Leaks detectable? | no | yes (created/deleted counters) |

No visual format was changed; "after" numbers are measurement capability,
not re-encoding. The format experiments above are deliberately deferred so
the classic look is preserved.

## Reproducing

1. Build and install (`build.ps1 -Install`).
2. Launch the game, load the café, stay idle ~30 s.
3. `adb logcat -s zcr:I | findstr texstat` (or pull
   `Android/data/com.capcom.zombiecafeandroid/files/zcr-log.txt`).
4. `adb shell dumpsys meminfo com.capcom.zombiecafeandroid` for the
   driver-side EGL mtrack figure.
