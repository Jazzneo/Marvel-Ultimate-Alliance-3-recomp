# MUA3 Recomp

<a href="https://recomplauncher.com"><picture><source media="(prefers-color-scheme: dark)" srcset="https://recomplauncher.com/assets/press/launcher/badge-light.svg"><img alt="Get it on RecompLauncher" src="https://recomplauncher.com/assets/press/launcher/badge-dark.svg" height="48"></picture></a>

A work-in-progress static recompilation of *MARVEL ULTIMATE ALLIANCE 3: The Black Order* (Nintendo Switch, 4.0.1:
the base game with its last update) into a native Windows x86-64 program, built on
[LAS Recomp](https://github.com/peiikai/LASRecomp)'s recompiler and Switch runtime (by way of DOAX3 Recomp, which
shares the game's Koei Tecmo engine).

**Status: early, work in progress.** The game boots and runs at its own 30 fps (60 and 120 as options) at 1920x1080
through the title, the update history, save slot and difficulty selection, the opening movie and into the first level
(the Kree ship), with the update's content and the Expansion Pass recognised. What has been changed, the debugging
switches and the open work: [docs/CHANGES.md](docs/CHANGES.md).

No game files are included or ever committed, and the releases hold no game code: the game code is recompiled from
your own copy on your own PC. You need your own legally obtained copy of the game, its 4.0.1 update and the keys from
your own console.

## Play with RecompLauncher

MUA3 Recomp works with [RecompLauncher](https://recomplauncher.com): install it from the launcher, then give Easy Setup
your game (NSP or XCI), the 4.0.1 update NSP, the Expansion Pass NSP if you have it, and your `prod.keys`. The setup
step recompiles the game for your PC (30 to 60 minutes, depending on the CPU; nothing else to install), then Play
starts it. Video, sound, controller and keyboard settings are on the game's page. Keep your NSP / XCI files where they
are: the game reads its data from them.

## Play with the release zip

1. Download `MUA3-Recomp-v<version>-Windows-x64.zip` from
   [Releases](https://github.com/VirtuaQuest/Marvel-Ultimate-Alliance-3-recomp/releases) and unzip it somewhere you can
   write to (Documents or Desktop, not Program Files).
2. Run `mua3launcher.exe`. On **GAME FILES**, choose your base game (NSP or XCI), the 4.0.1 update (NSP), the Expansion
   Pass (optional) and your `prod.keys`, then **INSTALL**. A wrong file is named in one line (another game, another
   version, keys that can't open it).
3. **PLAY**.

Needs Windows 10 or 11 (64-bit), a CPU with AVX2 (Intel 4th gen / AMD Ryzen or newer), a graphics driver with Vulkan
1.3, 8 GB of RAM (16 GB or more makes the setup faster: it compiles more files at once) and about 2 GB free for the
setup (about 0.5 GB stays). The setup took 40 minutes on an 8-core Intel i7-11700F.

## The launcher

`mua3launcher.exe` (`launcher/launcher.cpp`) has five pages:

- **PLAY**: checks the game files are set up, your NSPs are still where they were, and the PC version is built; PLAY
  saves the settings and starts the game.
- **GAME FILES**: picks your files and INSTALLs (extract, recompile, build) with a live log; REBUILD recompiles from
  the files already set up.
- **DISPLAY**: window mode and size, render resolution, upscaler, anti-aliasing, aspect ratio, frame rate 30 / 60 / 120,
  the FPS counter, volume and movie sound.
- **CONTROLS**: remap every Switch button and stick direction on the keyboard, and every button on a controller: click
  the binding, then press the key or button (Esc cancels; a key already in use swaps places). Rumble, its strength and
  background input are there too. The game's own menu (F1 > Controls) edits the same bindings.
- **ABOUT**: credits and the data folder.

Everything is saved in `larecomp.ini` next to the game, and the launcher only changes its own keys in it.

Default controls - keyboard: WASD left stick, numpad 8 / 5 / 4 / 6 right stick, K / J / L / I the Switch's A / B / X / Y,
Q / E L / R, 1 / 3 ZL / ZR, Enter Plus, Backspace Minus, arrows the d-pad, F / numpad 0 the stick presses. Controllers
(Xbox, PlayStation, Switch Pro and more, through SDL 3) map by position, as on the Switch. F1 or Esc (or Home) opens the
recomp's menu; F11 or Alt+Enter switches fullscreen.

## How it works

The game's ARM64 code (`exefs`) is translated to C by `tools/gen.py` and compiled into `build/mua3game.dll`. The
runtime (`runtime/`) provides what the Switch would: a small Horizon kernel (threads, memory, sync), the OS services
(file system, HID, audio, time, ...), and NVN (the Switch's graphics API) on Vulkan, with a Maxwell shader translator.

| exefs module | What it is | Here |
|---|---|---|
| `main` | the game (Koei Tecmo engine, NintendoSDK 6.5.0) | recompiled: `generated/game` |
| `sdk` | NintendoSDK | recompiled: `generated/sdk`; its system calls and services are host code |
| `subsdk0` | Nintendo multimedia (movie playback) | not loaded: `movie::` is host code on FFmpeg (`runtime/hle/movie.c`) |
| `subsdk1` | NVIDIA glslc (runtime GLSL compiler) | recompiled: `generated/glslc` |

The game data (18.4 GB with the update) is not copied out: `tools/nsp_extract.py` writes `extracted/romfs.src`, and
the runtime reads the data in place from your NSPs (`runtime/romfs_nca.c`).

## Build from source (Windows)

1. MSYS2 with UCRT64 clang, make, SDL3, FFmpeg, Vulkan, shaderc (see LAS Recomp's `tools/setup_windows.ps1`), and
   Python 3.12 with a venv: `py -V:3.12 -m venv .venv` and
   `.venv/Scripts/python -m pip install unicorn==2.1.4 capstone==5.0.9 lz4==4.4.5 pycryptodome==3.23.0 pillow==13.0.0`
   (Pillow: the window and executable icon, made from the game's own icon in your dump at build time).
2. Extract your game (in Git Bash, `export MSYS_NO_PATHCONV=1` first):
   `.venv/Scripts/python -I tools/nsp_extract.py "<base>.nsp" "<your prod.keys>" extracted --update "<update>.nsp"
   --dlc "<expansion pass>.nsp"`. Keep the NSPs where they are: the game data is read from them.
3. In Git Bash with `export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"`:
   `make gen` (recompile the game code into `generated/`, a few minutes), then `make -j16` (about 25 to 40 minutes
   the first time) and `make dlls` (the runtime DLLs next to the programs, so they start from Explorer).
4. Run: `build/mua3launcher.exe` (or `tools/run.sh`; log: `mua3.log`). Steps 2 and 3 can also be run from the
   launcher's GAME FILES page. The settings are in `larecomp.ini` next to `extracted/`; the in-game menu (F1 or Esc)
   changes most of them too.

## Making a release

From the MSYS2 UCRT64 shell (with `nasm` and `zip` installed as well): `make deps-windows` (an LGPL-only FFmpeg),
`make gen-helper` (the extractor and recompiler frozen with PyInstaller), `tools/fetch_zig.sh` (the pinned compiler),
then `make dist`: `build/dist/MUA3-Recomp-v<VERSION>-Windows-x64.zip`. It checks that no game code, game files, keys,
GPL codec libraries or paths of the build machine are inside. Attach it to a GitHub release whose tag is `v<VERSION>`;
`recomplauncher.json` inside tells RecompLauncher how to set the game up (`mua3launcher --prepare`) and start it
(`mua3launcher --play`).

## Credits

- **LAS Recomp** by Peiikai and contributors (MIT License, `LICENSE.LASRecomp`): the recompiler (`tools/gen.py`,
  `lifter.py`, `nso.py`, `simd_c.py`, ...) and the runtime this project started from.
- Third-party components keep their own licenses (`third_party/`): Dear ImGui (MIT), SMAA (MIT), the ported audio
  renderer and shader decoder references (MIT, `third_party/ryujinx/LICENSE.txt`), fonts (SIL OFL 1.1 / DejaVu). The
  release's bundled components, their licenses and their sources: [docs/THIRD_PARTY.md](docs/THIRD_PARTY.md).
- Libraries: SDL 3, FFmpeg (LGPL, dynamically linked), Vulkan Memory Allocator, shaderc; the generator uses Capstone,
  Unicorn and python-lz4.
- [Switchbrew](https://switchbrew.org) documentation (NCA/PFS0/RomFS formats for `tools/nsp_extract.py`).

*MARVEL ULTIMATE ALLIANCE 3* is a trademark of Marvel and Nintendo; the game was developed by Koei Tecmo. This project
is not affiliated with Marvel, Koei Tecmo or Nintendo. The release's icon is original (`tools/make_original_icon.py`).
