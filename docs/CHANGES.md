# Changes

What this project changed relative to [LAS Recomp](https://github.com/peiikai/LASRecomp) (its recompiler and Switch
runtime), why, and where. It also lists the debugging switches, the test routes, the known gaps and the next steps.
It started as DOAX3 Recomp (the sections from "Project setup" to "Test routes" are that work, which MUA3 inherits:
both games are Koei Tecmo's engine) and was retargeted at MUA3 on the `mua3` branch.

## Retargeted at MARVEL ULTIMATE ALLIANCE 3 4.0.1 (`mua3` branch)

- **The game:** title 010060700AC50000, the update's code (4.0.1, NintendoSDK 6.5.0, main build ID E853C44FDF18B88F).
  The DLC characters and stories ship in the update; the Expansion Pass (010060700AC51001) is an entitlement whose
  data is one empty file.
- **`tools/nsp_extract.py`:**
  - `--update`: the update's exefs, and its RomFS as a Bktr patch over the base game's (indirect storage remapping
    ranges to the base or the patch, and AesCtrEx: the patch's counter generation by range);
  - `--dlc`: each add-on's RomFS image to `extracted/aoc/<id>.romfs`;
  - `extracted/romfs.src` instead of an 18.4 GB copy: where the image is in the NSPs, the keys and both tables
    (`--romfs extract` still copies every file; `--files <text>` copies the matching ones). Checked against the RomFS's
    own SHA-256 level: 602 random 16 KB blocks from both storages, no mismatch.
- **`runtime/romfs_nca.c`:** reads the image in place (AES-128-CTR on AES-NI); `runtime/services/fs.c` uses it when
  `romfs.src` is there.
- **Module layout:** `main` is 0x4fc1000 bytes, so `sdk` moves to 0x7006000000 and glslc to 0x7007000000. glslc is
  `subsdk1` here (`GLSLC_NSO`); `subsdk0` is the multimedia library, still host code.
- **Services:** aoc reports the add-ons whose images were extracted and fsp-srv's OpenDataStorageByDataId serves
  them; GetDisplayVersion reads `extracted/control/control.nacp` (the title shows "Ver. 4.0.1").
- **Movie:** `movie::Buffer::SetNvnDev` / `SetNvnSync` are no-ops (the host decoder copies into guest memory).
- **Shader translator:** TLD4S (2D gather, with .AOFFI / .DC), HSETP2's constant-buffer and immediate forms, and VOTE
  (subgroup ballot / all / any / all-equal; needs `GL_KHR_shader_subgroup_vote` / `_ballot`).

## Project setup (ae4fa76)

- **Retargeted at DOAX3 Scarlet 1.0.0** (title 01009CC00C97C000, NintendoSDK 6.4.2). The Link's Awakening specific
  code is removed (warp, mirror mode, widescreen camera, pots, pets, randomizer, save editor, extra flags).
- **Game hooks:** `runtime/hle/game_hooks.c` and `runtime/ui/game_panels.mm`. Each hook point the shared runtime calls
  answers "nothing special" until a DOAX3 counterpart is written. `game_in_play()` returns 0, because the game's
  in-play state isn't located yet.
- **Three recompiled modules** (`Makefile`: `make gen`, then `make -j16`):

  | Module | What it is | Output |
  |---|---|---|
  | `main` | the game | `generated/game` |
  | `sdk` | NintendoSDK | `generated/sdk` |
  | `subsdk0` | NVIDIA glslc, base 0x7005000000 | `generated/glslc` |

  `subsdk1` (multimedia) is not loaded; `movie::` is host code instead.
- **Tools:**
  - `tools/nsp_extract.py`: extracts exefs, romfs and control from an NSP using your own `prod.keys`. It handles a
    base game only (no update/BKTR patch support yet).
  - `tools/sym.py` and `tools/addr2sym.py`: name guest addresses.
  - `tools/run.sh [secs]`: runs the game; the log goes to `doax3.log`.

## Kernel, HLE and services (ae4fa76)

- **Fibers:** `nn::os` fibers, in `runtime/hle/fiber.c`.
- **Offline network** (`runtime/services/net.c`), as on a console with no connection:
  - nifm turns every connection request down;
  - bsd fails to create sockets;
  - ssl hands out contexts that never get used.
- **Library applets** (controller strap guide, error dialogs): skipped and logged, in `runtime/hle/applets.c`.
- **`movie::MediaData::FindRect`**, in `runtime/hle/movie.c`.
- **Ids:** the program id and the add-on content base id (0x01009CC00C97D000) are DOAX3's, in
  `runtime/kernel.c` (GetInfo) and `runtime/services/misc.c` (aoc). aoc reports no DLC: the base dump is the *Basic Free Edition*.

## Audio (`runtime/services/audio.c`)

- **ae4fa76:**
  - The audren work buffer is sized as the SDK expects.
  - Splitter routing works; voices routed through splitters were silent before.
  - The BufferMixer effect is implemented.
- **3aceb94: Aux effect (type 2)**, ported from the SDK module's own code (`nn::audio::server::AuxInfo::Update`,
  `CommandGenerator::GenerateAuxCommand`, `dsp::WriteAuxBufferDsp` / `ReadAuxBufferDsp`):
  - The effect's input goes into the send ring.
  - Its output is replaced by what the game wrote into the return ring.
  - A disabled effect resets both rings and passes the input through dry.
  - Verified: the return is the send delayed by 2400 samples, correlation ~1.0, no dropouts.

  Parameter layout:

  | Offset | Field |
  |---|---|
  | 0 | in[24] |
  | 0x18 | out[24] |
  | 0x30 | channels |
  | 0x34 | rate |
  | 0x38 | ring sample count |
  | 0x40 | send info address |
  | 0x50 | return info address |

  Ring state is at info+0x40 (read offset, then write offset); int32 samples start at info+0x80.

## Graphics: NVN on Vulkan, Maxwell shader translator (ae4fa76)

- **Shader translator** (`runtime/nvn/maxwell/translate.c`):
  - separable programs;
  - shared memory (LDS/STS);
  - image load/store (SULD/SUST);
  - stencil sampling, in compute shaders only (`ShaderKey.tex_stencil_mask`).
- **Textures** (`runtime/nvn/vulkan/textures.c`):
  - storage image views (`vk_storage_view`);
  - buffer textures (target 10, made a 1D image);
  - 32-bit integer formats viewed as float;
  - a 1x1 2D texture bound in a cube slot becomes a 1x1 cube of its texel (`cube_of_1x1`); before, it was a black dummy.
- **Present pacing:** `present_pace` in `runtime/nvn/objects.c` paces to the game's present interval (2, so 30 fps).
- **Resolution:** native 1x by default. Scaled targets broke the deferred lighting compute pass, which stores at the
  game's pixel coordinates.

## Shader preparation at startup (efffec6)

- **Problem:** LAS Recomp's preparation (build every pipeline the game used before, ahead of the game) never ran.
  - It started from LA's launcher.
  - It looked for shader code where DOAX3 doesn't keep it; DOAX3's shaders are inside its packed data archive.
- **Code store:** each stage's code is kept in `cache/shader_code/<hash>.bin` the first time the game builds it.
  This is on the player's machine only and never committed.
- **Preparation:** it runs at device init from `cache/shader_code` and `cache/shader_seen.txt`.
- **Compute records** carry their stencil slots (` st=`), so the lighting shader is prepared too.
- **Result:** a second visit to the hotel prepares 40 variants in 0.24 s, with no first-visit compiles. Before, the
  first visit skipped ~1250 draws over 2 s.

## GPU waits the game can't observe (b18b47d, `runtime/nvn/vulkan/exec.c`)

- **Problem:** DOAX3's loader copies each texture out of a staging texture and calls `nvnQueueFinish` ~350 times per
  load.
- **Fix:** the copies snapshot guest data when translated. So the game's waits (QueueFinish, SyncWait) now stop at the
  latest submission with observable work: draws, dispatches or ReportCounter.
  - `obs_seq` records that submission; `note_observable()` updates it; `observable_limit()` applies it.
- **Result:** load-screen stalls dropped from 115–240 ms to 60–110 ms.
- **Also:** `runtime/nvn/slowframe.c` can log slow frames during loads.

## Opening movie (67d7a94, `runtime/hle/movie.c`)

- **Problem:** the opening started black, then crashed on its first frame. SDK 6.4.2's movie API differs from LA's.
- **Method:** the vtable order came from resolving `subsdk1`'s `_ZTVN5movie*` relocations.
- **Extractor:** slot 2 is `SetDataSource(const char *path)`, with the path in x1.
- **Decoder:**
  - The game creates its textures and buffers on the "format changed" event (event 2). So the first video frame
    signals event 2, and output is held until the game has called `GetOutputFormat`.
  - The format includes `stride`.
  - Frames are copied as packed NV12 (Y at stride = width, then UV) into the buffer the game passes to
    `GetOutputBuffer`.
  - Slot 17, `GetOutputBuffer(int, Buffer*, MediaData*)`, is implemented.
- **Debugging aids added:**
  - `LARECOMP_TRACE_CALL` (`runtime/dispatch.c`).
  - `LARECOMP_FNPROF` now writes its profile on a crash (`runtime/fnprof.c`, `runtime/platform/windows.c`).
- **When it plays:** only on a fresh save, when the first vacation starts.

## Test routes (7443143, `tools/route.sh`)

DOAX3's routes, since replaced by MUA3's (see "MUA3: into the first level").

`tools/route.sh <route> [secs] [shot frames]` runs the game with scripted input; shots go to `build/frames/`.

| Route | Save | Path | Time |
|---|---|---|---|
| `menus` | existing | title, hotel room, casino | ~160 s |
| `volleyball` | fresh | opening movie, Kasumi's arrival, Niki Beach, Beach Volleyball | ~420 s |
| `relax` | fresh | as `volleyball`, then the shower scene, evening, night | ~330 s |

Fresh-save routes move `save/` to `save.route-backup/` and put it back afterwards.

## Debugging switches (environment variables)

Added in this project:

| Variable | Effect |
|---|---|
| `LARECOMP_FENCE_ALL=1` | full GPU waits (turns off the observable-work shortcut) |
| `LARECOMP_SLOW_FRAME_ALL=1` | log slow frames during loads too |
| `LARECOMP_TRACE_CALL=<hex>` | log a guest function's arguments x0–x3 and result (first 200 calls) |
| `LARECOMP_AUX_DRY=1` | leave the Aux effect out |
| `LARECOMP_AUX_LOG=1` | Aux send/return levels every 1000 frames |
| `LARECOMP_AUX_CAPTURE=<file>` | save Aux channel 0's input and output |
| `LARECOMP_AUDIO_LEVEL=1` | output peak and RMS about once a second |
| `LARECOMP_LOG_STAGES=<n>` | the first n shaders' headers and their stages |
| `LARECOMP_SHADER_SURVEY=1` | log every unknown instruction in a shader |
| `LARECOMP_PRESENT_PACE=0` | uncapped frame rate |

Inherited from LAS Recomp and used here:

| Variable | Effect |
|---|---|
| `LARECOMP_INPUT="frame:BUTTON:frames,..."` | scripted input |
| `LARECOMP_SHOT_AT=<frames>` | screenshots at those frames, to `build/frames/` |
| `LARECOMP_COPY_TRACE=<frame>` | per-draw and per-copy trace of one frame |
| `LARECOMP_SAVE_MSL=1` | keep translated shaders in `build/glsl/` |
| `LARECOMP_FNPROF=<from>:<to>` | guest function-entry profile |
| `LARECOMP_SHADER_PREP=0` | no shader preparation at startup |
| `LARECOMP_SYNC_SHADERS=1` | compile on first use, blocking, instead of skipping draws for a few frames |

## MUA3: into the first level (2026-11-01)

- **Saving:** the game's saves never reached disk: the stub `flags_ext_save_written` hook (`runtime/hle/game_hooks.c`)
  returned false, which `fs.c` takes as a failed write of any file named `*savedata*` (MUA3's `savedatainfo`).
- **Shader translator:**
  - LEA / LEA.HI (64-bit address math);
  - CAL / RET: a short subroutine without control flow of its own is inlined at the call, others go through a return
    stack in the `switch (pc)` machine (the big effect compute programs have ~3000 calls);
  - RED (global reduction, through the ATOM path);
  - DMUL / DADD / DFMA / DSETP: GLSL doubles in register pairs (`shaderFloat64` is enabled).
- **BRX jump tables in programs over 64 KB:** the table reader took any constant offset of 0x10000 or more for "no
  constants known", so the effect compute programs (~90 KB) had every instruction as a `switch` case (9000 cases) and
  the driver never finished building them; the startup preparation waited on them forever (no window). Now 214
  cases, built in ~7 s once, cached after.
- **Movies from the romfs image:** with the game data read in place from the NSPs there is no host file for FFmpeg
  to open. `romfs_file_find` / `romfs_image_read` (`fs.c`) give the movie player the file's range in the image, read
  through an FFmpeg custom reader. The opening (EV_0101_01.mp4, H.264 + AAC, ~3.5 minutes) plays; + pauses it and X
  skips it.
- **Indirect draws:** `DrawArraysIndirect` / `DrawElementsIndirect` were recorded and then dropped. They now draw
  with their arguments read on the GPU from the imported heap (`vkCmdDraw(Indexed)Indirect`, the index buffer bound in
  place without the vertex cache). `LARECOMP_VK_INDIRECT_CPU=1` reads them on the CPU at replay instead.
- **Also replayed now:** `ClearBuffer` and `SignalEvent` (both were dropped). The no-op command buffer stubs log
  their first call ("first call, not implemented").
- **`tools/route.sh`:** the MUA3 routes `menus` and `kree` (fresh save to the first level, the Kree ship, in ~130 s).

### The dark first level (open)

The level runs at 29-30 fps with no skipped draws, but characters are black silhouettes and the scene has no surface
detail: only fog, the spotlight's volume and emissive screens show. What was found:

- The frame is a depth prepass (456 direct draws, a fragment shader that writes nothing), a G-buffer pass with five
  colour targets, then lighting. The G-buffer pass gets **one direct draw**; every other object is drawn there as
  `SetRenderEnableConditional(mode 1, record)` + `DrawElementsIndirect(record + 0x10)` + `SetRenderEnable(1)`
  (game `f_70004360f0` -> `f_7000840e70`; the direct path `f_70012305f0` is taken only for parts without an
  occlusion slot).
- The records (a 0x18000-byte buffer, 48 bytes each: a 16-byte conditional pair `{0, 1, 0, 0}`, the arguments
  `{count 0, instances 1, 0, 0, 0}`, padding) are written once by the CPU at load (`f_7000445440`, seen with a write
  watch) and **never again**: no CPU write, and none of the 13 compute shaders, no graphics shader, copy, clear,
  inline update, event, counter report or transform feedback touches them. The counts stay 0, so the G-buffer stays
  black. On the console something fills those counts on the GPU; it hasn't been identified. Next: what the game
  does with the occlusion slots (`[x21 + 0x10]` entries in `f_70004360f0`) and whether a GPU occlusion mechanism NVN
  offers (counters, ZCull, conditional render) is what it relies on.
- Tools for it, all off by default: `LARECOMP_LOG_INDIRECT=1` (arguments at replay, a record scan every 2 s),
  `LARECOMP_WATCH_INDIRECT=1` (write-protect the first record's page and log each writer's module and stack),
  `LARECOMP_DEBUG_OPS=1` (with `LARECOMP_DEBUG_FRAME`: every non-state command of the debug frame).

## Status (MUA3, 2026-11-01)

- **Runs at 29-30 fps at 1920x1080** (i9-10900, RTX 3080 Ti) from the title through save creation, the opening movie
  and into the first level (the Kree ship), with saving working. No crash in runs of up to 5.5 minutes.
- **Checked correct:** title, update history, save slot list and dialog, difficulty, save creation, the opening movie,
  loading tips, level HUD and dialogue.
- **Wrong:** the level's lighting and surfaces (see above).

## Known gaps and next steps

- **The dark level:** above.
- **Frame rate:** the title and menus hold 29-30 fps when nothing else uses the GPU. The 3-15 fps stretches seen in
  testing lined up with other recomps running on the same GPU (~60 ms GPU time slices in `LARECOMP_GPUPROF_SLOW`
  profiles), so check for them before trusting a measurement.
- **Compute:** all three title-screen compute shaders now translate (ATOMS, ATOM, imageSize through TXQ; lane id and
  lane masks were 0). The tiled light culling shader (`cs 72b4b35500`) takes 1.5-10 ms; its light lists are read by
  the lighting pass from the guest heap in place, which is worth moving to device-local memory. The lane masks
  assume 32-lane subgroups (NVIDIA); AMD's wave64 would need subgroup size control.
- **60 fps and above:** the game runs at 30 fps (present interval 2). Finding its frame step and whether its logic
  is tied to the frame rate comes once gameplay runs; any unlock will be a setting that can be turned off.
- **Build time:** `generated/glslc/glslc_188.c` alone took over 15 minutes at -O1; splitting that chunk would cut the
  first build a lot.
- From DOAX3, still open: `game_in_play` and level hooks (slow-frame classification treats every frame as a load),
  stencil sampling in graphics stages, 8- and 16-bit integer texture formats, swkbd offsets.
