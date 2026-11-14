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
| `LARECOMP_AUDIO_MIXLEVEL=1` | each mix's peak (everything it received) about once a second |
| `LARECOMP_AUDIO_SPLIT_LOG=1` | each splitter info and destination (mix, volumes) as the game sends it |
| `LARECOMP_SHADER_PREP_WAIT=<ms>` | how long startup waits for the last prepared shaders' builds (15000; -1: no limit) |
| `LARECOMP_LOG_STAGES=<n>` | the first n shaders' headers and their stages |
| `LARECOMP_SHADER_SURVEY=1` | log every unknown instruction in a shader |
| `LARECOMP_PRESENT_PACE=0` | uncapped frame rate |
| `LARECOMP_PACE_LOCK=0` | frames not locked to the display's refresh (the pace's own 60 Hz clock, as before) |
| `LARECOMP_LOG_PACE_LOCK=1` | each frame's slot, lead and correction (the display lock) |
| `LARECOMP_PRESENT_TIMING=0` / `=2` | no `[present]` cadence lines / one line per frame (when it reached the screen, where in the refresh, uploads and pipelines since, the render thread's time and its texture part) |

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

### The dark first level: a geometry shader that was dropped

The level first ran with black character silhouettes and no surface detail (only fog, the spotlight's volume and
emissive screens). The cause:

- The frame is a depth-only pass (456 direct draws), a G-buffer pass with five colour targets, then lighting. In the
  G-buffer pass each object is `SetRenderEnableConditional(mode 1, record)` + `DrawElementsIndirect(record + 0x10)` +
  `SetRenderEnable(1)` (game `f_70004360f0` -> `f_7000840e70`), and the indirect draws' counts and index buffers
  were all zero.
- The depth-only draws' program (vs 72b4b92f00, fs 72b4b94e00) has a **geometry shader** (72b4b94f00) that emits no
  vertices: per triangle it tests visibility, reserves room with a warp-aggregated atomic add on storage buffer 0
  (the object's record: the indirect count) and stores the triangle's three vertex indices (`gl_VertexIndex`, the
  vertex shader's output 0) to storage buffer 1 (the indirect draw's index buffer). The Vulkan backend draws only
  the vertex and fragment stages, so it was silently dropped.
- **Fix:** a geometry shader that stores to memory and has no OUT is replayed on the CPU when its draw is replayed:
  the draw's indices plus base vertex go to storage buffer 1 and the count is set (every triangle kept: no culling),
  and the draw rasterizes nothing, as the shader emits nothing. `LARECOMP_GS_CULL_EMU=0` turns it off. Programs
  whose geometry / tessellation stages are dropped are logged once each.
- Diagnostics added on the way, all off by default: `LARECOMP_LOG_INDIRECT=1` (indirect arguments and indices at
  replay, a record scan every 2 s), `LARECOMP_WATCH_INDIRECT=1` / `=2` (write-protect / no-access the first record's
  page and log each writer's / the game's readers' module and stack), `LARECOMP_DEBUG_OPS=1` (with
  `LARECOMP_DEBUG_FRAME`: every non-state command of the debug frame), `LARECOMP_LOG_DEVICE_INTEGER=1`,
  `LARECOMP_DUMP_GS=1` (the first dropped geometry shader's code to `build/gs_code.bin`).

## Frame rate, launcher, window modes, icon (2026-11-01)

### Frame rate setting (30 / 60 / 120)

- **How the game paces itself:** MUA3 never asks for the vsync event. At the title `present_pace` (interval 2) holds it
  at 30; in levels its own limiter (f_7000839000) sleeps until frame start + 1 / its target rate (30.0 at
  `0x70045a7158 + 0x98`, a global frame object; its logic frame count at + 0x68).
- **Its time step:** `dt = max(measured, step)` capped at 1/30 s, where `step` is 1/60 s for its frame interval 1 and
  1/30 s for 2 (a table at 0x70019f33d8; the interval at `[[0x7002188a00] + 0x60]`, 2 in MUA3). Just running faster
  therefore played at double speed (every frame still advanced 1/30 s). A separate step count (`0x1e566c` in the
  world, forced to 2) only sets the present interval and a render-thread wait timeout.
- **The setting** (`frame_rate` in larecomp.ini; the launcher; "Frame rate" on the menu's Display tab;
  `LARECOMP_FPS=30/60/120` overrides; an old `fps60 = 1` reads as 60): the display period is half the frame time
  (`g_vsync_period`, runtime/hle/vi.c: `present_pace` keeps its interval-2 grid), and every frame
  (game_hooks.c `game_frame_rate`) the limiter target becomes the rate, the frame interval 1 above 30, and at 120
  the interval-1 step 1/120 s.
- **Measured** (`LARECOMP_LOG_GAMETIME=1`: game time per 4 s of real time, from the logic frames): 3.90 s at 30
  (117 frames), 3.88 s at 60 (228), 3.78-3.86 s at 120 (~105 logic frames a second, ~65-100 shown: see Known gaps).
  Six minutes of play at 60 had no crash.
- **Render thread costs cut on the way:** CPU-written linear textures (six 1024x1024 RGBA8, two a frame) are copied
  to their images by the GPU from the imported heap and not hashed when the OS's write tracking says they changed;
  buffer textures (the light lists) are copied the same way instead of hashed. Texture time 525 -> 58 ms per 2 s,
  render thread ~10-12 -> ~5 ms a frame.
- **Test switches:** `LARECOMP_FPS_AT=<frame>:<hz>` changes the display rate at a presented frame (a frame-scripted
  route still gets through the loads); `LARECOMP_LOG_SLEEPS=1` (the game's sleeps and timed waits of 5 ms or more),
  `LARECOMP_LOG_LIMITER=1`, `LARECOMP_LOG_DYNTEX=1`.

### Launcher (`launcher/launcher.cpp`, `build/mua3launcher.exe`)

- SDL 3 + Dear ImGui (SDL renderer). Window mode (windowed / borderless fullscreen / exclusive fullscreen),
  resolution (the window's size, or the display mode at its highest refresh rate for exclusive), render resolution,
  upscaler, anti-aliasing, aspect ratio, frame rate, the FPS counter; Play saves and starts `mua3recomp.exe` from the
  data folder (the one with `extracted/`, next to the launcher or one up) as `tools/run.sh` does.
- It changes only its own keys in larecomp.ini and keeps every other line.
- Test switches: `MUA3_LAUNCHER_SHOT=<file.bmp>` (save its window, quit), `MUA3_LAUNCHER_PLAY=1` (press Play).

### Window modes (runtime/nvn/vulkan/device.c `ui_apply_window_mode`)

- New keys `exclusive_fullscreen`, `window_width`, `window_height` next to `fullscreen`: windowed at the chosen size;
  borderless (the desktop's mode); exclusive (the display mode closest to the chosen size, the desktop's if none, at
  that size's highest refresh rate). F11 / Alt+Enter still toggle fullscreen, in the chosen fullscreen kind.
- Checked: windowed 1600x900 from the launcher's settings. The fullscreen kinds weren't run here (they take the
  display over); they're SDL's own calls.

### Icon

- `tools/make_icon.py` makes `build/icon.png` (the window's and the launcher's, loaded at run time) and
  `build/icon.ico` (in both executables, `windres`) from `extracted/control/icon_*.dat`: the game's own home-menu icon
  from your dump, never committed. The window's title is "Marvel Ultimate Alliance 3 Recomp".

## Releases, RecompLauncher, the launcher's setup and controls (2026-11-13)

Ported from ARMS Rearmed's release work (the same LAS Recomp base), with MUA3's third module (`glslc`) and its
Expansion Pass.

### The release (`make dist`)

- `build/dist/MUA3-Recomp-v<VERSION>-Windows-x64.zip` (85 MB, top folder `MUA3 Recomp/`): the runtime and the launcher
  built with `RELEASE=1` (no console: the log is `mua3recomp.log` next to the exe; no debug info; the original icon),
  an LGPL-only FFmpeg (`tools/build_deps_windows.sh`: H.264 + AAC in MP4 is what the game's 127 movies use, checked
  with ffprobe), `mua3gen` (`tools/nsp_extract.py` + `tools/gen.py` frozen by PyInstaller, `tools/mua3gen.spec`; Control
  Flow Guard cleared, `tools/pe_no_cfg.py`), zig 0.17.0 slimmed with its runtime built ahead of time
  (`tools/fetch_zig.sh`, `tools/zig_slim.sh`) and `gamelib-src/` (the headers and the `GAMELIB_ID` the game code is
  compiled against). It holds no game code and no game files: the build fails if game code, game files, keys, GPL
  codec libraries or paths of the build machine end up in it.
- `make dlls`: the MSYS2 runtime DLLs next to a source build's programs, so they start from Explorer and the launcher.
- Build notes: MSYS2's make drops `USERPROFILE` from its recipes (PyInstaller then can't find a home folder), so
  `make gen-helper USERPROFILE="$USERPROFILE"`; FFmpeg's configure needs a POSIX `TMPDIR` when `TEMP` is a Windows path.
- Player-facing names: "MUA3 Recomp" instead of "LAS Recomp" in message boxes, crash explanations (their advice is
  MUA3's: version 4.0.1, GAME FILES / RecompLauncher's Set up instead of LAS Recomp's start screen) and the log's
  `[info]` line; the Windows logs are `mua3recomp*.log`. The version is the Makefile's `VERSION` (`-dev` in a source
  build).
- The release's icon is original (`tools/make_original_icon.py` -> `docs/icon/`): a source build still uses the game's
  own from your dump.

### The launcher (`launcher/launcher.cpp`)

- Pages: PLAY (status: files set up, the NSPs still where `romfs.src` says, the PC version built), GAME FILES (base game
  NSP or XCI, the 4.0.1 update, the optional Expansion Pass, prod.keys; INSTALL / REBUILD / CANCEL with a live log;
  in a source checkout: venv, `nsp_extract.py`, `make gen`, `make`; in the release: `mua3gen`, then the game code
  compiled by zig in parallel and linked against `mua3recomp.dll.a`), DISPLAY (as before, plus volume and movie sound),
  CONTROLS, ABOUT.
- CONTROLS: every Switch input on the keyboard (24: buttons and both sticks' directions) and on a controller (16
  buttons), click then press; a binding already in use swaps, Esc or a click cancels, 5 s timeout. The same tables,
  defaults and `bind_key_*` / `bind_pad_*` lines as the game's own menu (`runtime/hle/bindings.c`): only bindings that
  differ from the defaults are written. Controller names follow the connected pad (Xbox / PlayStation / Nintendo);
  controller navigation is held off while a rebind waits and until its button is let go. Rumble, its strength and
  background input are there too.
- Command lines (RecompLauncher): `--prepare --game=<nsp|xci> --update=<nsp> [--dlc=<nsp>] --keys=<prod.keys>` (no
  window, `progress <0..1> <text>` lines, `prepare.log`; each option also as two arguments; `--base=` = `--game=`) and
  `--play` (the game, waited for; its exit code is the launcher's; the window opens only when a release update needs
  the game code rebuilt). Without them the launcher opens as before.
- Test switch: `MUA3_LAUNCHER_REBIND=k<slot>:<key name>|p<slot>:<button>,...` (rebinds through the capture path, then
  saves and quits), next to `MUA3_LAUNCHER_SHOT`, `MUA3_LAUNCHER_TAB` and `MUA3_LAUNCHER_PLAY`.

### `tools/nsp_extract.py`

- XCI as well as NSP (the root HFS0's secure partition), ARMS Rearmed's version; the ARMS-only shader unpacking is left
  out. `--title` / `--build` / `--want` refuse another game, another version (no update, or another one) and an add-on
  of another game in one line; a prod.keys without the keys needed, or with a damaged key, is said in one line too.
  Without these options nothing changed.

### RecompLauncher (`recomplauncher.json`, in every release zip)

- Easy Setup inputs: the game (NSP / XCI, with the shared `switch.prod-keys`), the 4.0.1 update, the optional Expansion
  Pass; `prepare` runs `mua3launcher --prepare`, `start` runs `mua3launcher --play`. Settings on the game's page
  (window, size, screen shape, render resolution, upscaling, anti-aliasing, frame rate, FPS counter, volume, movie
  sound, face buttons by position or by label, rumble and its strength, background input) and the keyboard as
  bindings; saves, logs, the shader cache. New installs start with background input off (the launcher's overlay
  uses the controller). Checked against the schema in RecompLauncher's agent guide: 0 errors.

### Checked

- A fresh checkout builds (`make gen` 75 s, `make -j16` 26 min 47 s on an i7-11700F, no errors) and
  `tools/route.sh kree` plays into the first level at 29 fps.
- The release, unzipped to a new folder and run with Windows' own PATH only (no MSYS2, Python or clang):
  `mua3launcher --prepare` as RecompLauncher runs it (paths with spaces and brackets, the Expansion Pass included)
  finished in 2399 s (extract 0 s, the three modules' C in 57 s, zig: 1023 files 13 at a time, then the link) with the
  same game code library id as the source build; `mua3launcher --play` with the kree route: the opening movie on the
  LGPL FFmpeg, then the first level at 29 fps (51.7k draws a frame, as the source build), saves in `save/`, the log in
  `mua3recomp.log`. (Halved since: "Setup time" below.)
- The launcher's rebinding through `MUA3_LAUNCHER_REBIND` (a swap on the keyboard, a free button on the controller,
  a key taken from another input): the bindings written as the game writes them, every other line of larecomp.ini
  (comments, unknown keys) kept.

## Setup time, Big Picture, Recomp Achievements, the launcher's look, the menu (2026-11-13)

### Setup time: glslc without optimization (`tools/gen.py --optnone`)

- **Found:** single-threaded, the generated C compiles at about 5 s per MB with MSYS2's clang at -O1; 515 MB of it.
  glslc (NVIDIA's runtime GLSL compiler, `subsdk1`) is 96 MB of that but took 797 s of the 8-job build, half of it:
  `glslc_188.c` is one 750 KB function (154 s alone) and others like it. `LARECOMP_FNPROF` over `route.sh kree` (boot
  through the first level) entered 11,061 functions, none of them in glslc (0x7007...): the game doesn't run it.
- **Changed:** `gen.py --optnone` puts `#pragma clang optimize off` in each chunk; the Makefile's `gen` and the release's
  setup pass it for glslc only. Correct if it ever runs, just slower there. The game code library's id changes with
  gen.py, so old libraries are rebuilt.
- **Result:** glslc's 214 files in 44 s instead of 797 s (8 jobs); the game code from source (`make -j16`) in 631 s
  instead of about 25 minutes; the release's setup in 1180 s instead of 2399 s on
  the same machine and files.
- **Also:** zig compiles with `-g0` (it adds CodeView debug info by default: objects 3x the size, nothing the link
  keeps). Measured, not changed: zig's clang is ~2x slower per file than MSYS2's, and this 8-core CPU compiles only ~3x
  faster with 8 jobs than with 1 (no gain past 8).
- `make dist` refuses a `mua3gen` frozen before a change to the generator's sources (`make gen-helper` again).

### RecompLauncher's Big Picture and controller

- `RECOMPLAUNCHER_MODE=bigpicture`: full screen for that session (`runtime/settings.c`); larecomp.ini keeps the
  player's own choice (settings_save writes it while the session's is in effect).
- `RECOMPLAUNCHER_PAD=xbox|playstation`: the recomp's menus name that controller's buttons until the game has opened a
  controller of its own (`runtime/hle/input.c input_pad_family`).

### Recomp Achievements (`runtime/achievements.c`, `third_party/rl_achievements`)

- The SDK (events mode) vendored unchanged with its license; rcheevos and cJSON built into the exe. Without
  RecompLauncher (no `RL_SESSION`) every call returns at once.
- Events: `movie_EV_<chapter><scene>_<cut>` for each story movie the game opens (`runtime/hle/movie.c`), `chapter_NN` the
  first time a chapter's movies start, and the profile line "Story: chapter N"; a read of a save slot (save00...) is
  reported as a save loaded. Chapters 01 to 11 and the last movie (EV_1105_01) come from the game's own file names.
- `docs/achievements.json`: the proposed events list for RecompLauncher to host (12 achievements, 200 points: start the
  story, reach chapters 2 to 11, finish it). Not verified past chapter 1: nothing here has played further.
- Checked on Windows with a stand-in for RecompLauncher's pipe: hello, list_ok, `movie_EV_0101_01` and `chapter_01` at
  frame 3040 (the opening), the presence line - from the source build and from the release.

### The launcher's look

- Drawn in code, in the game's colours (no game art, logos or fonts): an animated cosmic background (indigo to violet,
  nebula glows, stars), six gem colours - red, orange, blue, purple, green, gold - one for each page's tab, card edge,
  headings' underline and buttons, faceted gems in the header, the title in gold in Baloo 2 (OFL, shipped). Every page
  still works with the mouse, the keyboard and a controller. The release's icon is redrawn in the same colours.

### The in-game menu

- Only what MUA3 has: Controls, Video, Audio, Quit. LAS Recomp's save editor, randomizer, warp, cheats, gameplay and
  cosmetics pages (Link's Awakening's; stubs here) aren't listed, nor its tilt-shift blur, Hero Mode emblem,
  360-degree movement and health light options. "D-pad moves your hero"; Button icons names the recomp's menus.

## Dialogue voices, and a black window at startup (2026-11-13)

### No voices in dialogue: splitter destinations were never read (`runtime/services/audio.c`)

- **Seen:** the dialogue's text shows ("We should probably get moving.") and the game reads its voice data
  (`SOUND/Stream_Voice.ktsl2stbin`, KTSR like the music) as each line starts, and a renderer voice plays it (ADPCM,
  32 kHz mono) into a sub mix of its own (14, 15, 16, ... in turn) - but nothing is heard.
- **Found:** those sub mixes send through splitters (mix 14: splitter 5), and the splitter destinations they use were
  never set: `LARECOMP_AUDIO_SPLIT_LOG` showed every RequestUpdate's splitter section read only as far as its first
  'SNDI'. Each splitter info is followed by unused bytes after its destination ids (Ryujinx notes 12), so read as
  packed, the next record wasn't where it was looked for and every destination after it was dropped.
- **Changed:** the next 'SNDI' / the first 'SNDD' are found by their tag right after each info. Now every update reads
  in full; a line's mix goes to the voice bus (mix 4, at 0.251) and on through mix 9 to the final mix:
  `LARECOMP_AUDIO_MIXLEVEL` shows the dialogue's mixes at ~16000 and mix 4 at ~4100 while a line plays (it got
  nothing before). Splitter 4's sound (mix 13, to mix 1) was silent the same way.

### Black window at startup: one shader build of ~100 s (`runtime/nvn/shader_prep.c`, `vulkan/exec.c`, `device.c`)

- **Seen:** after a session that reached the first level, every start stayed on a black window (the player's frame
  rate change only restarted it). The hang watch: no frame presented; the main thread in `nvnDeviceInitialize`.
- **Found** (gdb on the stuck process, the data folder bisected): the boot's shader preparation (the player's
  `cache/shader_seen.txt` with `cache/shader_code/`) waited for its last builds, and one pipeline sat in NVIDIA's
  compiler (`nvgpucomp64.dll`) for ~100 s on an RTX 3060; preparation then finished and the game ran. Once the driver
  had cached that pipeline, starts took under a second.
- **Changed:** startup waits for the last builds at most `LARECOMP_SHADER_PREP_WAIT` ms (15000); the rest finish in
  the background, and a draw whose prepared pipeline isn't ready is skipped, as with the game's own first builds. The
  window's title shows "preparing shaders N%" while it runs. Also: a program whose translation fails is marked failed
  at once (it stayed "not built", and a later record with the same program would have waited for it forever).
- Checked: the reproducing cache (copied to a scratch data folder) with `LARECOMP_SHADER_PREP_WAIT=0`: 6 builds left to
  the background, the title at 29 fps; the player's own settings and cache: preparation 690 ms, 57 fps at 60;
  `route.sh kree` into the first level, no crash.

## 120 fps checked; every frame rate at its target (2026-11-13)

- **Checked at 120** (RTX 3060, i7-11700F): boot and title (`frame_rate = 120`: the display at 240 Hz), the opening
  movie (video 29.97 fps and audio at their own rates: the decoders' frames 76 a second, as 29.97 + 46.9), the first
  level with its dialogue voiced (`LARECOMP_FPS_AT=3550:240`: note the display rate is twice the frame rate) - no
  crash, game time at real time (`LARECOMP_LOG_GAMETIME`).
- **Found:** every setting ran a little under its rate - 108 at 120, 57 at 60, 29.3 at 30: the game's frame limiter
  sleeps until its frame start + 1 / rate, and SleepThread's nanosleep rounds up to the system timer, ~0.9 ms a frame.
- **Changed** (`runtime/kernel.c`): the limiter's sleep (its call site, `game_frame_limiter_sleep`) waits on
  `plat_sleep_until_ns` (a high-resolution timer, the last 0.2 ms spun); other sleeps are as before. Now the title at
  ~117 at 120, 59.3 at 60, 29.8 at 30; the first level 117 logic frames a second at 120, 59 fps at 60, 29.8 at 30.
- **Not changed - the first level at 120 shows ~72 fps here:** the GPU's work is ~8.8 ms a frame (`LARECOMP_GPUPROF_AT`;
  the tiled light culling and the lighting passes ~1.0-1.3 ms each), and frames come every ~14 ms with the render thread
  waiting on the GPU half the time: the game reads compute results back each frame, so CPU and GPU work don't overlap.
  At a third of the resolution it's still ~87 fps, so dynamic resolution (which also aims at 60: `DYN_BUDGET`) can't
  reach 120 either. On a 60 Hz display (this PC's) frames above 60 aren't shown anyway; the launcher says so.

## Game time exactly real time; audio dropouts watched (2026-11-13)

- **Found:** with the precise sleep the limiter still drifted: each frame waits until its own start + 1 / rate, so the
  time the frame's work takes past that is never caught up and game time ran ~1-2 % slow (and an odd late frame a
  little fast after).
- **Changed** (`runtime/kernel.c`, `game_frame_period_ns` in `runtime/hle/game_hooks.c`): the limiter's wakeups are
  held to a grid of 1 / the game's target rate - a wakeup within a quarter period of the next grid point lands on it,
  anything further off (a load, a long frame) starts a new grid. Now `LARECOMP_LOG_GAMETIME` shows 4.00 s of game
  time in 4.00 s at 30, 60 and 120, the title at exactly 30.0 / 60.0 / 120.0 fps, and the dialogue lines take the
  same time at every rate (no speed-up, no slow-down).
- **Added** (`runtime/services/audio.c`): `[audren] lost audio: voice ...` when a voice decodes sound at a volume
  above zero but its route (sub mixes, splitters) never reaches the final mix - the way the dialogue was silent - and
  `[audren] output ran dry` when the device runs out of queued audio (a dropout), with where the renderer thread's
  time went (waiting for the lock, rendering, or itself late).
- **Checked** (RTX 3060, i7-11700F): the first level from its start to "Follow the Scanner" (5.5 minutes, moving,
  attacking, the dialogue) at 30, 60 and 120: no crash, no lost audio; one dropout in one 30 fps run, during the
  level's first dialogue lines loading, not repeated in the next runs. At 120 the logic runs 120 frames a second with
  ~91 presented in the level.
- **The audio thread at time-critical priority** (`plat_thread_realtime`, as the vsync thread): playing the release
  from a fresh install, one dropout in two runs while the first shader builds ran - the thread wasn't run for ~50 ms,
  with no wait on a lock and 0.2 ms of rendering (the dropout line now shows the kernel lock and queue wait too).
- **`make dist`'s check for this machine's paths:** make's shell has no `USERPROFILE` / `USERNAME`, so the profile
  pattern had been just `C:/Users/` (it matched text in Python's DLL). The user name now comes from `HOME` when
  needed, the check stops if it has none, and the MSYS2 home folder is checked too.

## Stutter in play and in cutscenes; shaders prepared for a new install (2026-11-14)

### Measured first: what reaches the screen (`runtime/nvn/vulkan/present_timing.c`)

- Every present now carries an id (`VK_KHR_present_id` / `VK_KHR_present_wait`, where the driver has them) and a
  thread notes when the compositor takes each frame; with the display's vblank (`plat_display_vblank`: the desktop
  compositor's timing) that says which refresh it is shown at. A `[present]` line every ~10 s: frames held 1 / 2 / 3+
  refreshes, and where in a refresh they were taken.
- **Found** (RTX 3060, a 60.00 Hz display, 30 fps, the 5.5-minute route from the title through the opening movie to
  "Follow the Scanner"): the average was 30.0 fps throughout, but 107 frames were held one refresh or three instead of
  two - 41 of them in the opening movie. The display pacing (`present_pace`) ran on its own 60 Hz clock, which drifts
  against the display's, and where the frames' hand-over came close to a vblank, a frame a little late waited a whole
  refresh more and the next one a refresh less. At 59.94 Hz that happens every 17 s, at 144 Hz all the time.

### Frames locked to the display (`runtime/nvn/objects.c`, `runtime/kernel.c`, `vulkan/record.c`, `vulkan/device.c`)

- When the setting's frame period is a whole number of the display's refreshes (or at most 2% slower: 29.97 fps at
  59.94 Hz), frames are paced that many refreshes apart, on the display's own clock. The game's frame limiter follows
  the same clock, so the two can't drift apart. Never faster than the setting (no speed-up).
- Each frame gets a slot 2 ms into a refresh; the recorder holds the present until then. Every frame reaches the
  compositor at the same point of a refresh, and one whose work runs long still makes its vblank with most of a refresh
  to spare. The time a frame needs before its slot (the lead) follows the GPU's measured time for the slowest of the
  last 30 frames: the opening movie runs over a 3D scene (13 ms of GPU a frame here, against ~5 ms in play), and
  handed over as soon as they were done, its heavier frames missed their vblank.
- Not locked: another display than the primary one (the compositor's timing is the primary's), frame rates the display
  can't divide (30 at 144 Hz), above the display's rate (120 at 60 Hz) - there, the pacing as before. A safety net
  turns the lock off for the session if, locked, more than a tenth of the frames are off the cadence for 20 s.
- **Checked:** the same route at 30 fps: 0 frames off the cadence (107 before), the opening movie included; at 60 fps:
  0; game time at real time at 30, 60 and 120 (`LARECOMP_LOG_GAMETIME`).

### A shader cache for a new install (`runtime/nvn/shader_variants.txt`, `shader_code_locs.txt`, `tools/shader_locs.py`)

- The shipped shader list (the pipeline variants prepared at startup) was DOAX3's empty one: a new install built every
  shader the first time it was drawn - 253 first-use builds from the title into the first level, each object missing
  for a frame or two while its pipeline compiled, and the compiles competing with the game for the CPU.
- Now 198 variants ship: the title, menus, the opening movie and the first level. Only hashes and pipeline state, as
  before; the code is found in the player's own game files. MUA3 keeps its shaders in its shader packages
  (`SHADER/**/*.spkg`, `*.kps`), which are read in place from the NSPs, so `shader_code_locs.txt` says where each
  shader is: file, offset, size (`tools/shader_locs.py` writes it from a play's `cache/shader_code` and the extracted
  files; 165 of 173 found, 14 of them in the main module; the other 8 the game compiles itself at run time). The bytes
  are read from the player's copy and checked against the hash before anything is built from them.
- **Checked** (no `cache/` folder, a new driver shader cache): 182 variants prepared at the first start, 18 first-use
  builds in the same route (253 before).
- The build workers now take a draw's first-use builds before preparation's, and run preparation's below the normal
  priority (`plat_thread_background`): on a new install the background builds no longer hold up a new area's
  pipelines or the game's own threads.

## Textures streamed in: less of the render thread (2026-11-14)

- **Measured** (`LARECOMP_PRESENT_TIMING=2`: each frame's render-thread time and the part of it spent on textures):
  textures arrive in bursts - a hundred or more at once while a level loads, two of 8 MB at a time, 25-48 MB in a
  frame while the opening movie plays over the first level loading - and the render thread hashed each new
  texture's contents, copied it out of the GPU's tiled layout into the upload ring, and recorded the copy: up to
  11 ms in one frame here (8 cores), 17 frames over 4 ms in the route from the title through the opening movie. On a
  slower CPU, the same bursts are missed frames.
- **The upload ring in system memory** (`vulkan/exec.c` `vk_transient_upload`): texture contents and movie frames
  went into the device-local window, which the CPU writes across PCIe (~7 GB/s here); now a ring of their own in
  system memory, which it writes at memory speed, and the GPU's copy reads across the bus instead. The main ring
  (uniforms, vertices: read by shaders) stays where it was. The copying went from 102 to 55 ms over the route.
- **The tiled layout's copy** (`runtime/nvn/tegra.c` `deswizzle_rows`): one offset per row and four fixed 16-byte
  copies per 64 bytes, where the offset was worked out for every 16 bytes: three times as fast on its own (24 GB/s
  against 8), byte for byte the same over 3000 layouts.
- **Checked:** the route's texture time 288 -> 240 ms, frames over 4 ms 17 -> 9, over 6 ms 5 -> 3; the movie and the
  level look as before. Splitting a texture's hash and copy over helper threads gained nothing here (the copy waits
  on memory, not the CPU) and isn't kept. The hash, about a third of what's left, would go with a single pass that
  hashes while it copies.

## The opening movie's starts and stops (2026-11-14)

- **Found** (each game thread's waits, sleeps and signals around the late frames, with timestamps): the presenting
  thread waits up to a frame for the game's main thread to hand it the next one, and the late frames were the main
  thread's long ones - at the movie's start, in the middle (the level streamed in), at its end and as the level
  begins.
- **The movie's start** (`runtime/dispatch.c` `hle_alloc`): opening the decoder took the main thread 13 ms, nearly all
  of it clearing 34 MB of new buffers that were zero already (committed memory is) - every page faulted in on that
  thread. Only memory handed out before (a reboot starts the native heap over) is cleared now; the decoder's buffers
  are faulted in by the thread that fills them. Opening the movie's file (5.6 ms) remains, within the frame.
- **Short sleeps** (`runtime/kernel.c` SleepThread, `plat_sleep_short_ns`): the game polls with 100 us sleeps - eight
  in a row while the level streams in during the movie - and each took 1-2.5 ms (Windows' timer, even at 1 ms
  resolution). Under 2 ms they now wait on the high-resolution timer, ~0.5 ms, without spinning a core.
- **The audio thread and the kernel lock** (`runtime/services/audio.c`): once in ~20 runs the sound dropped out for a
  moment as the game loaded its heroes after the title - the audio thread had waited 44 ms for the kernel lock, to
  signal the game's audio event (no code path held it that long: a game thread holding it, descheduled while every
  core was loading). The signal now waits for a free lock only after ~20 ms of trying; until then it goes with the
  next block (the event stays set, the game misses no update) and the output keeps flowing.
- **Checked:** in seven runs of the opening movie, its 10 s window showed 2-6 frames off the cadence (one to three
  late frames, at the level's own loading as the movie ends), where it had shown up to 24. The full route at 30 and
  60 fps: 10 and 12 frames off the cadence in 5.5 minutes (25 and 22 the round before), game time 4.00 s in 4.00 s.

## Status (MUA3, 2026-11-01)

- **Runs at 29-30 fps at 1920x1080** (i9-10900, RTX 3080 Ti) from the title through save creation, the opening movie
  and the first level (the Kree ship), with saving working. No crash in runs of up to 8.5 minutes, six of them
  playing (`tools/route.sh play`: moving, attacking, jumping, switching heroes).
- **Checked correct:** title, update history, save slot list and dialog, difficulty, save creation, the opening movie,
  loading tips, the first level's rendering (characters, surfaces, lighting, spotlight, effects), HUD and dialogue.

## Known gaps and next steps

- **Culling:** the geometry shader's triangle culling isn't done (every triangle is drawn). A compute version of it
  would cut the G-buffer pass's work if the GPU ever becomes the limit.
- **Frame rate:** the title and menus hold 29-30 fps when nothing else uses the GPU. The 3-15 fps stretches seen in
  testing lined up with other recomps running on the same GPU (~60 ms GPU time slices in `LARECOMP_GPUPROF_SLOW`
  profiles), so check for them before trusting a measurement.
- **Compute:** all three title-screen compute shaders now translate (ATOMS, ATOM, imageSize through TXQ; lane id and
  lane masks were 0). The tiled light culling shader (`cs 72b4b35500`) takes 1.5-10 ms; its light lists are read by
  the lighting pass from the guest heap in place, which is worth moving to device-local memory. The lane masks
  assume 32-lane subgroups (NVIDIA); AMD's wave64 would need subgroup size control.
- **Frame rate:** 30 / 60 / 120 are a setting (above). At 120 the RTX 3080 Ti shows ~70-100 fps in the first level
  while other GPU work runs alongside: the GPU (~8-11 ms a frame, the tiled light culling 3.3 ms of it: its 8.4 MB
  light lists go through the dispatch's device-local copy and back every frame) is the limit now.
- **Build time:** glslc is now built without optimization (below, "Setup time"). The rest is zig's clang, about twice
  as slow as MSYS2's on this code, and a CPU that gains little past 8 compilers at once.
- From DOAX3, still open: `game_in_play` and level hooks (slow-frame classification treats every frame as a load),
  stencil sampling in graphics stages, 8- and 16-bit integer texture formats, swkbd offsets.
