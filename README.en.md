# Ren'Py Patcher

> 🌐 中文文档：[README.md](README.md)
> 📄 **License**: This project is released under a **Non-Commercial License** (see [`LICENSE`](LICENSE)). Personal study, research, and non-commercial use are allowed; **commercial use is prohibited** and requires a separate written license.
> ⚠️ **Beta (test-v0.1)**: The core engine (unpack / repack / direct injection) is complete and self-tested. Hot injection and the GUI still need real-world testing; the API and file layout may change — do not use in production.

A **translation / mod patching tool** for Ren'Py games (Galgames / visual novels).

A pure-C core engine parses and rewrites the resource archives (RPA), and — together with a unified module contract — can be extended to support multiple injection methods, translation pipelines, and compressed export. The full chain "unpack → repack → direct injection → hot injection (Windows)" is already in place.

---

## Table of Contents
- [Why this exists](#why-this-exists)
- [Features](#features)
- [How it works](#how-it-works)
  - [RPA-3.0 format internals](#rpa-30-format-internals)
  - [Direct injection](#direct-injection)
  - [Hot injection (Windows)](#hot-injection-windows)
- [Tech Stack](#tech-stack)
- [Project Layout](#project-layout)
- [Architecture (Unified Module Contract)](#architecture-unified-module-contract)
- [Build](#build)
  - [Prerequisites](#prerequisites)
  - [Linux / macOS](#linux--macos)
  - [Windows (vcpkg + MSVC)](#windows-vcpkg--msvc)
  - [Single-file gcc build](#single-file-gcc-build)
- [Usage](#usage)
  - [unpack](#unpack)
  - [repack](#repack)
  - [build (direct injection)](#build-direct-injection)
  - [hotinject](#hotinject)
- [Authoring a mod](#authoring-a-mod)
- [Testing](#testing)
- [CI / Release](#ci--release)
- [Progress / Roadmap](#progress--roadmap)
- [Contributing (extending the engine)](#contributing-extending-the-engine)
- [Troubleshooting](#troubleshooting)
- [FAQ](#faq)
- [License](#license)

---

## Why this exists
Ren'Py ships game assets inside `.rpa` archives. To translate or mod a game you normally need to unpack, edit, and repack those archives — and many existing tools are Python-only, slow on large games, or hard to embed. Ren'Py Patcher is a **small, dependency-light C engine** that does the RPA surgery correctly (byte-for-byte compatible with the Ren'Py loader) and exposes a clean module contract so new capabilities plug in without forking the core.

## Features
- **RPA-3.0 round-trip**: unpacking and repacking are byte-for-byte compatible with the Ren'Py engine (the index is stored *after* the data, and the header records the index offset).
- **Direct injection**: merge translation/mod resources into the game archive and repack, producing a patch that a real game can read directly.
- **Hot injection (Windows)**: inject a DLL into the running game process at runtime, locate CPython, and activate the game's in-process Python to load `mod_main.py` — zero file modification.
- **Extensible architecture**: a unified module registry (`core/plugin_loader`) lets new capabilities (compressed export / translation backends / external plugins) be plugged in with no core changes.

## How it works

### RPA-3.0 format internals
A Ren'Py 3.0 archive looks like:

```
+---------------------------------------+
| "RPA-3.0" <space>                     |  header magic
| <8 hex chars = index offset (base 16)>|  where the pickle index starts
| <4 hex chars = XOR prefix>            |  per-archive XOR key (lower 32 bits)
| newline                               |
+---------------------------------------+
| raw file data ...                     |  concatenated, possibly zlib-compressed
+---------------------------------------+
| pickle index (zlib-compressed)       |  starts at the offset above
+---------------------------------------+
```

- Each index entry maps a logical path (`"script.rpy"`, `"images/logo.png"`) to one or more `(offset, length, prefix)` blocks.
- Data is **XOR-obfuscated** with the archive prefix (`offset ^ (prefix << 32)`, `length ^ prefix`), then optionally zlib-compressed per block.
- The engine's `util/pickle_min` parses the CPython `pickle` payload (protocol 2/3/4, memo-aware) and `util/pickle_write` re-serializes it — so the rewritten index is identical in meaning to what Ren'Py wrote.

### Direct injection
1. `unpack` the original archive into a directory tree.
2. Merge a `mod/` directory over it (new files added, existing files overwritten).
3. `repack` the merged tree into a new `.rpa`, recomputing offsets and re-writing the pickle index.
4. The resulting archive is byte-valid and loadable by the real game.

### Hot injection (Windows)
Instead of touching files on disk, `hotinject`:
1. Opens the target game process and injects `agent.dll` via `CreateRemoteThread` + `LoadLibraryW`.
2. `agent.dll` walks the host process for the embedded CPython interpreter (`py_bridge.c`), grabs `PyImport_*` / `PyRun_*` function pointers, and executes the mod's `mod_main.py` inside the live interpreter.
3. The mod runs in-process, with full access to the game's own modules and state — no repack needed.

> Status: the hot-injection code is written and compiles, but **has not been tested against a live Ren'Py build on Windows yet**. Treat M4 as "code complete, unverified."

## Tech Stack
| Layer | Choice |
| --- | --- |
| Core language | **C11** (pure C, no C++ / no external runtime dependency) |
| Compression | **zlib** (RPA-embedded deflate; in-house `zlib_wrap` wraps inflate/deflate) |
| Serialization | in-house **pickle_min**: parses CPython protocol 2/3/4 `pickle`, used to read & rewrite the Ren'Py index |
| Archive format | **RPA-3.0**: header `RPA-3.0` + 8-byte index offset + 4-byte XOR prefix; the index (pickle) lives after the data segment |
| Build system | **CMake ≥ 3.16** (core static lib + `patcher` executable + `agent` DLL); Windows uses **vcpkg** to provide zlib |
| CI | **GitHub Actions**: Ubuntu (gcc) / Windows (MSVC) dual-matrix build; pushing `v*` tags auto-publishes a pre-release |
| Hot injection (Windows) | DLL injection (`CreateRemoteThread` + `LoadLibrary`) + **CPython C-API** bridge (`py_bridge.c` locates the host interpreter and runs `mod_main.py`) |
| Extension mechanism | unified module contract (`core/plugin.h`) + central registry (`core/plugin_loader`) |

## Project Layout
```
.
├── CMakeLists.txt            # Build: core static lib + patcher exe + agent DLL
├── vcpkg.json               # Windows dependency manifest (zlib, auto-fetched in manifest mode)
├── LICENSE                  # Non-Commercial License (commercial use prohibited)
├── DESIGN.md                # Architecture design & milestone notes
├── .github/workflows/
│   └── build.yml            # CI: Linux/Windows build + tag-triggered Release
├── core/                    # Unified module contract & registry
│   ├── plugin.h             #   module interface / struct definitions
│   └── plugin_loader.c/.h   #   central registry (lookup / register / extension entry)
├── util/                    # Common utilities
│   ├── io_buf.c/.h          #   byte buffer (read / write / append)
│   ├── pickle_min.c/.h      #   CPython pickle parser (protocol 2/3/4, memo-aware)
│   ├── pickle_write.c/.h    #   pickle serializer (writes RPA index, protocol 2)
│   └── dir_walk.c/.h        #   directory tree walk (repack / mod scan)
├── compress/
│   └── zlib_wrap.c/.h       # zlib inflate/deflate wrapper (with safe loop guard)
├── rpa/                     # RPA-3.0 unpack & repack
│   ├── rpa_format.h         #   header / block struct definitions
│   ├── rpa_v3.c/.h          #   unpack + index parse
│   └── rpa_v3_repack.c      #   repack (data + index assembly)
├── mod/                     # Unified mod module
│   ├── mod_loader.c         #   directory merge (override / add files)
│   └── mod_manifest.h       #   mod metadata / contract
├── inject/
│   ├── direct/
│   │   └── direct_inject.c/.h  # Direct injection: unpack → merge → repack
│   └── hot/                 # Hot injection (runtime process injection, Windows)
│       ├── agent.c/.h       #   injected DLL payload
│       ├── inject_api.c     #   hot-inject entry (stub on non-Windows)
│       └── py_bridge.c/.h   #   locate host CPython & execute mod_main.py
├── cli/
│   └── main.c               # CLI entry (subcommand dispatch)
└── tests/                   # Demo game + translation mod + verification scripts
    ├── README_test.md       #   test notes (incl. license constraints)
    ├── gen_demo_assets.py   #   generate demo game assets
    ├── make_real_rpa.py     #   pack using the real RPA format
    ├── verify_rpa.py        #   verify output via Ren'Py loader logic
    ├── mod_hanzi/           #   sample translation mod (Chinese rpy + logo swap)
    └── sample_game/         #   sample game (game/ directory)
```

## Architecture (Unified Module Contract)
- Every capability implements the unified interface defined in `core/plugin.h` and registers itself with the central registry `core/plugin_loader` at startup.
- The CLI (`cli/main.c`) parses the subcommand, then looks up the matching module in the registry and invokes it.
- New capabilities (compressed export / translation backend / external plugin DLL) only need to implement the contract and register — no core code changes required.

This keeps the core tiny and stable while letting the surface area grow: a future GUI (C# shell) just links `core` and drives it through the same contract.

## Build

### Prerequisites
- A C11 compiler: **gcc/clang** (Linux/macOS) or **MSVC** (Windows, via Visual Studio Build Tools).
- **CMake ≥ 3.16**.
- **zlib**:
  - Linux/macOS: system package (`zlib1g-dev` / `zlib`).
  - Windows: provided automatically by **vcpkg** (see below) — you do not install it by hand.

> **Versioning**: the version lives in the root `VERSION` file (semver, e.g. `0.1.0`). CMake reads it, captures the git short commit, and generates `core/version.h` (included from `core/plugin.h`). `patcher --version` prints it. When cutting a release, update `VERSION` and push a matching `vX.Y.Z` git tag.

### Linux / macOS
```bash
cmake -B build -S .
cmake --build build
# produces: build/patcher
```

### Windows (vcpkg + MSVC)
The repo ships `vcpkg.json` declaring `zlib`, so CMake's manifest mode fetches it:
```powershell
# run in "Developer Command Prompt for VS" or let setup-msbuild handle the toolchain
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release `
  "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
# produces: build/Release/patcher.exe  and  build/Release/agent.dll
```
The Windows job in `.github/workflows/build.yml` does exactly this on `windows-latest`.

### Single-file gcc build
If you prefer not to use CMake:
```bash
gcc -std=c11 -O2 -I. core/plugin_loader.c util/*.c compress/*.c rpa/*.c mod/*.c \
    inject/direct/*.c inject/hot/inject_api.c cli/main.c -lz -o build/patcher
```
(On non-Windows, `inject/hot/inject_api.c` compiles to a no-op stub.)

## Usage

### global options & game management
```bash
patcher [--log f.log | --no-log | --debug | --log-level N] <subcommand> ...

# register games as aliases, then use the alias instead of the path
patcher game add    <alias> <game.rpa path> [note]   # register
patcher game list                                    # list registered
patcher game show   <alias>                          # details (incl. file count)
patcher game remove <alias>                          # unregister
patcher game path   <alias>                          # print path only (for scripts)
``` into `out_dir`, preserving the original directory structure (`images/logo.png` → `out_dir/images/logo.png`).

### repack
```bash
patcher repack dir -o out.rpa
```
Re-packs a directory tree back into a valid `.rpa`. Used to verify the round-trip is lossless.

### build (direct injection)
```bash
patcher build game.rpa -m mod_dir -o patched.rpa
```
Unpacks `game.rpa`, merges `mod_dir` over it (new files added, same-named files overwritten), then repacks into `patched.rpa`. The output is directly loadable by the game.

### hotinject
```bash
patcher hotinject --pid <pid> --mod mod_dir
```
(Windows only.) Injects `agent.dll` into the process `<pid>` and runs `mod_dir/mod_main.py` inside the game's Python interpreter. No files are modified on disk.

## Authoring a mod
A mod is just a directory mirroring the game's `game/` layout:
```
my_mod/
├── zz_hanzi.rpy        # your translated / added script (UTF-8)
├── images/
│   └── logo.png        # a replacement asset (same path = overwrite)
└── mod_main.py         # (hot-inject only) entry executed in-process
```
- Paths are matched verbatim against the archive's logical paths.
- A file with the same path as an existing archived file **overwrites** it.
- New paths are **added** to the index.
- For hot injection, `mod_main.py` is the in-process entry point (not repacked into the archive).

See [`tests/mod_hanzi`](tests/mod_hanzi) for a working sample (Chinese script + logo swap).

## Testing
See [`tests/README_test.md`](tests/README_test.md). The `tests/` directory ships:
- `gen_demo_assets.py` — generates a small demo game (`options.rpy`, `script.rpy`, `images/logo.png`).
- `make_real_rpa.py` — packs it with the **real RPA-3.0 format** (random XOR prefix).
- `verify_rpa.py` — validates the patched output using Ren'Py loader logic (XOR decode + zlib inflate + pickle parse), and asserts byte-equality against the expected file set.

You can run the demo end-to-end, then drop the produced `patched.rpa` into a real Ren'Py SDK game to confirm the Chinese text shows up.

## Version Management

- **Single source of truth**: the root `VERSION` file (semver, e.g. `0.1.0`).
- **Auto-generated**: CMake parses `VERSION`, captures the git short commit, and writes `core/version.h` (pulled in by `core/plugin.h`) — no need to edit version macros by hand.
- **Check version**: `patcher --version` (or `-v`) prints the version, the build commit, and the license.
- **Release convention**: bump `VERSION`, then push a matching `vX.Y.Z` git tag; CI auto-publishes a pre-release on `v*` tags.
- `core/version.h` is a generated file and is `.gitignore`d — do not commit it by hand.


## Export (Distribute Patch)

Pack an unpacked directory, a mod folder, etc. into a **standard `.zip`** so players can extract it without any RPA tool:

```bash
patcher export <dir> -o patch.zip             # deflate compression by default
patcher export <dir> -o patch.zip -store       # store only, no compression
patcher export <dir> -o patch.zip -split 100M  # volumes: .z01 .z02 ... .zip
```

Notes:
- Output is a standard ZIP readable by OS extractors / Python `zipfile`.
- `-split <size>` splits into fixed-size volumes (K/M/G); concatenate in order (`cat *.z* out.zip`) to restore.
- Backed by in-house `util/zip` (zlib only, no third-party lib), consistent with the project's zero-runtime-dependency philosophy.


## GUI Version (Windows / .NET WinForms)

Besides the CLI, there is a **GUI version** (Windows only, C++/CLI + .NET WinForms) with **feature parity** to the CLI, sharing the exact same core C implementation:

- Game management: add / remove / list aliases
- Unpack / Inject (build, auto credit) / Credit / Export (with split)

Source lives on the **`gui`** branch; project files `gui/patcher_gui.sln` + `gui/patcher_gui.vcxproj`. The GUI compiles the core `.c` files into the same executable, so CLI and GUI stay in sync.

### Build GUI (Windows)
1. Install Visual Studio 2019/2022 ("Desktop development with C++" + "C++/CLI support").
2. Install zlib via vcpkg: `vcpkg install zlib:x64-windows` then `vcpkg integrate install`.
3. Open `gui/patcher_gui.sln` or run `gui/build_gui.bat`; output `gui/x64/Release/patcher_gui.exe`.

> Note: the GUI excludes the hot-injection module (it depends on libpython); `RP_NO_HOT_INJECT` is defined so it is not compiled in. All other features match the CLI.


## Source Credit

After you inject/translate a game, you can bake a "injected/translated by Ren'Py Patcher (open-source)" notice into the archive so players can trace the patch source:

- **Automatic**: `patcher build` writes `ZZ_RENPY_PATCHER_CREDIT.txt` into the output rpa automatically.
- **Manual**: for an existing/already-injected `.rpa`, `patcher credit <rpa> -o out.rpa` appends the notice (unpack -> add file -> repack).
- **Text only**: `patcher credit --text [--game <name>]` prints the notice to stdout for copying elsewhere.

The notice includes the project name, repo URL, version, the Non-Commercial License, and a timestamp (bilingual).


## Logging

By default each run appends a log to **`patcher.log`** (with timestamp, level, and module tag), and also prints to stderr. Global options (placed before the subcommand) control this:

| Global option | Effect |
| --- | --- |
| `--no-log` | do not write a log file, stderr only |
| `--log <file>` | use a custom log file |
| `--debug` | enable DEBUG level (most verbose) |
| `--log-level <0-3>` | min level: 0=DEBUG 1=INFO 2=WARN 3=ERROR |

Higher level = less output; `INFO` and above is the default. Logs include handler selection, unpack/inject progress, and error codes — handy for debugging.

## Game Management

Register a `.rpa` path under an **alias**; afterwards `unpack` / `build` accept either the path or the alias:

```bash
patcher game add   mygame /path/to/game.rpa "note"
patcher unpack mygame -o out        # == unpack /path/to/game.rpa
patcher build  mygame -m mod/ -o p.rpa       # direct inject (auto-adds credit)

# source credit: mark an (already) injected/translated rpa as patched by Ren'Py Patcher
patcher credit <game.rpa|alias> -o out.rpa            # add credit to an existing rpa
patcher credit --text [--game <name>]                 # print credit text only

# export: pack a directory (unpacked result / mod) into a standard zip for distribution
patcher export <dir> -o patch.zip [-split 100M] [-store|-deflate]
```

The registry is a plain-text file on disk (`$RENPY_PATCHER_HOME` or `~/.renpy_patcher/games.txt`; on Windows `%APPDATA%/renpy_patcher/games.txt`) — no database required.


## CI / Release
`.github/workflows/build.yml`:
- Triggers on push to `main`, opening a PR against `main`, and pushing any `v*` tag.
- Builds on **Ubuntu (gcc)** and **Windows (MSVC)**; packages each as a `.zip` artifact.
- On a `v*` tag, auto-creates a **pre-release** (`prerelease: true`, because this is still beta) and attaches both platform zips.

To cut a release:
```bash
git tag v0.1 && git push origin v0.1
```

## Progress / Roadmap
| Milestone | Scope | Status |
| --- | --- | --- |
| M1 | RPA-3.0 unpack | ✅ Done |
| M2 | As-is repack round-trip | ✅ Done |
| M3 | Direct injection (merge mod → repack) | ✅ Done |
| M4 | Hot injection (Windows process injection + CPython bridge) | 🔲 Needs Windows test |
| M5 | Export / split (standard zip, in-house util/zip) | ✅ Done |
| GUI | Windows GUI (C++/CLI WinForms, on par with CLI) | ✅ Done (gui branch) |
| M6 | GUI (C# shell referencing the core static lib) | 🔲 Not implemented |
| M7 | External plugin DLL auto-load (registry entry reserved) | 🔲 Not implemented |
| M8 | Translation pipeline | 🔲 Not implemented |
| M9 | APK support | 🔲 Deferred |

## Contributing (extending the engine)
1. Implement the module interface from `core/plugin.h`.
2. Register it in `core/plugin_loader` (add a lookup entry and call it from `cli/main.c`).
3. Build and add tests under `tests/`.
4. Keep changes non-commercial-friendly (see License); do not introduce commercial-only code into this repo.

## Troubleshooting
- **`unpack` produces nothing / hangs** — older versions had a pickle-parser stack bug and an unbounded inflate loop; both are fixed. If you still see a hang, rebuild from current `main` (the loop now has a guard).
- **Windows build fails on zlib** — make sure CMake is using the vcpkg toolchain (`vcpkg.json` must be at the repo root so manifest mode engages).
- **Repacked archive not recognized by the game** — confirm you did not change the logical paths; the index is path-sensitive and case-sensitive.

## FAQ
- **Is this a Ren'Py replacement?** No. It only reads/writes `.rpa` archives and (on Windows) injects into a running game. The game itself still runs on Ren'Py.
- **Does it modify Ren'Py?** Direct injection modifies the *archive*, not Ren'Py. Hot injection runs code inside the game's own Python — no Ren'Py files are touched.
- **Can I sell my mod?** No. The Non-Commercial License prohibits commercial use/distribution. Contact the author for a commercial license.
- **Why custom license instead of MIT?** MIT and other OSI licenses permit commercial use. This project explicitly forbids it, so a custom NC license is used (GitHub labels it `other`).

## License
This project is released under a **Non-Commercial License** (see [`LICENSE`](LICENSE)). Personal study, research, and non-commercial use are allowed; **commercial use is prohibited** (sale, embedding in commercial products, paid distribution, etc.) and requires a separate written license. The demo game and translation mod under `tests/` are subject to the same constraint.

> Note: this is a custom license, not an OSI-approved one. If you need a recognizable standard, consider CC BY-NC 4.0 for the content — but the code remains under this project's Non-Commercial License unless the author says otherwise.
