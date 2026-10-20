# Ren'Py Patcher

> 🌐 中文文档：[README.md](README.md)
> 📄 **License**: This project is released under a **Non-Commercial License** (see [`LICENSE`](LICENSE)). Personal study, research, and non-commercial use are allowed; **commercial use is prohibited** and requires a separate written license.
> 📌 **Version**: Latest stable / pre-release builds are on [Releases](https://github.com/Zz131417/renpy_patcher/releases); the version number is managed in the root [`VERSION`](VERSION) file. Full history is in [CHANGELOG.md](CHANGELOG.md) (this document does not hard-code a version, so it needs no edits per release).

A **translation / mod patching tool** for Ren'Py games (Galgames / visual novels).

A pure-C core engine parses and rewrites the resource archives (RPA), and — together with a unified module contract — can be extended to support multiple injection methods, translation pipelines, and compressed export. The full chain "unpack → repack → direct injection → hot injection (Windows)" is in place, with a Windows GUI that mirrors the CLI feature-for-feature.

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
- [Usage](#usage)
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
Ren'Py ships game assets inside `.rpa` archives. To translate or mod a game you normally need to unpack, edit, and repack those archives — and many existing tools are Python-only, slow on large games, or hard to embed. Ren'Py Patcher is a **small, dependency-light C engine** that does the RPA surgery correctly (byte-for-byte compatible with the Ren'Py loader) and exposes a clean module contract so new capabilities plug in without forking the core. A dialog-translation pipeline (extract → ai/human → build mod) is built in.

## Features
- **RPA-3.0 round-trip**: unpacking and repacking are byte-for-byte compatible with the Ren'Py engine (the index is stored *after* the data, and the header records the index offset).
- **Direct injection**: merge translation/mod resources into the game archive and repack, producing a patch that a real game can read directly.
- **Hot injection (Windows)**: inject a DLL into the running game process at runtime, locate CPython, and activate the game's in-process Python to load `mod_main.py` — zero file modification. The GUI also ships a standalone **DLL Injection** window (pick a DLL + target process PID, or launch an exe and inject, with an optional delay).
- **Extensible architecture**: a unified module registry (`core/plugin_loader`) lets new capabilities (compressed export / translation backends / external plugins) be plugged in with no core changes.
- **Windows GUI**: C# / WPF (.NET 8) calling the core via `patcher_core.dll`, on par with the CLI; bottom 3-page nav (Game / Home / Settings), validates input, shows "error code + meaning + detail" on failure, runs ops on a background thread.
- **Built-in update check**: the GUI can check for updates from Settings (compared by build number), with an optional pre-release channel toggle.

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

> Status: the hot-injection code is written and compiles, but has **not been verified against a live Ren'Py build on Windows yet**. Treat M4 as "code complete, unverified."

## Tech Stack
| Layer | Choice |
| --- | --- |
| Core language | **C11** (pure C, no C++ / no external runtime dependency) |
| Compression | **zlib** (RPA-embedded deflate; in-house `zlib_wrap` wraps inflate/deflate), vendored as source under `third_party/zlib` |
| Serialization | in-house **pickle_min**: parses CPython protocol 2/3/4 `pickle`, used to read & rewrite the Ren'Py index |
| Archive format | **RPA-3.0**: header `RPA-3.0` + 8-byte index offset + 4-byte XOR prefix; the index (pickle) lives after the data segment |
| Build system | **CMake ≥ 3.17** (core static lib `renpy_core` + CLI `patcher` exe + Windows core DLL `patcher_core.dll`); WPF GUI in `gui_wpf/` (.NET 8, separate `dotnet publish`) |
| CI | **GitHub Actions**: Ubuntu (gcc) / Windows (MSVC) dual-matrix build; release is a manual `workflow_dispatch` one-click build, `v*` tags are for periodic backup only |
| Hot injection (Windows) | DLL injection (`CreateRemoteThread` + `LoadLibrary`) + **CPython C-API** bridge (`py_bridge.c` locates the host interpreter and runs `mod_main.py`) |
| Extension mechanism | unified module contract (`core/plugin.h`) + central registry (`core/plugin_loader`) |

## Project Layout
```
.
├── CMakeLists.txt            # Build: renpy_core static lib + patcher exe + patcher_core.dll (Windows core DLL, for WPF P/Invoke)
├── VERSION                   # Version (semver, single source of truth)
├── CHANGELOG.md              # Changelog
├── LICENSE                  # Non-Commercial License (commercial use prohibited)
├── DESIGN.md                # Architecture design & milestone notes
├── .github/workflows/
│   └── build.yml            # CI: Linux/Windows build; manual workflow_dispatch releases
├── third_party/
│   └── zlib/                # Vendored zlib source (add_subdirectory, no vcpkg / system zlib / network)
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
├── gui_wpf/                 # Windows GUI (C# + WPF / .NET 8, P/Invoke to patcher_core.dll)
│   ├── MainWindow.xaml/cs   #   main window: bottom 3-page nav + pages + log/progress
│   ├── Native.cs           #   P/Invoke bridge to patcher_core.dll
│   └── App.xaml/cs         #   app entry + UI settings
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
- The GUI (`gui/MainForm.cpp`) links the same static library `renpy_core` and drives it through the C interface, sharing all CLI capabilities.
- New capabilities (compressed export / translation backend / external plugin DLL) only need to implement the contract and register — no core code changes required.

## Build

zlib is vendored as source under `third_party/zlib` and built via CMake `add_subdirectory` — **no system zlib / vcpkg / network** required.

The version lives in the root `VERSION` file (semver, e.g. `1.0.0`). CMake reads it, captures the git short commit, and generates `core/version.h` (included from `core/plugin.h`). `patcher --version` prints it.

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
# outputs: build/patcher (Linux), build/Release/patcher.exe + patcher_core.dll (Windows); gui_wpf via dotnet publish -> renpy_patcher_gui.exe
```

> The CLI can also be built with a single-file gcc (still needs `core/version.h` generated by CMake first, and links system `-lz`):
> ```bash
> gcc -std=c11 -O2 -I. core/plugin_loader.c util/*.c compress/*.c rpa/*.c mod/*.c \
>     inject/direct/*.c inject/hot/inject_api.c cli/main.c -lz -o build/patcher
> ```

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
```

### resource operations
```bash
patcher unpack <game.rpa|alias> -o out_dir             # unpack
patcher repack dir -o out.rpa                         # as-is repack
patcher build  <game.rpa|alias> -m mod_dir -o patched.rpa  # direct injection (auto credit)
patcher hotinject --pid <pid> --mod mod_dir          # hot injection (Windows)

# source credit: mark an (already) injected/translated rpa as patched by Ren'Py Patcher
patcher credit <game.rpa|alias> -o out.rpa            # add credit to an existing rpa
patcher credit --text [--game <name>]                 # print credit text only

# export: pack a directory (unpacked result / mod) into a standard zip for distribution
patcher export <dir> -o patch.zip [-split 100M] [-store|-deflate]
```

## Source Credit
After you inject/translate a game, you can bake a "injected/translated by Ren'Py Patcher (open-source)" notice into the archive so players can trace the patch source:
- **Automatic**: `patcher build` writes `ZZ_RENPY_PATCHER_CREDIT.txt` into the output rpa automatically.
- **Manual**: for an existing/already-injected `.rpa`, `patcher credit <rpa> -o out.rpa` appends the notice (unpack -> add file -> repack).
- **Text only**: `patcher credit --text [--game <name>]` prints the notice to stdout for copying elsewhere.

The notice includes the project name, repo URL, version, the Non-Commercial License, and a timestamp (bilingual).

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
- **Unit / integration tests (ctest)**: core API end-to-end (`tests/test_core_api.c`, covering unpack / repack / mod merge / credit / DLL bake / export) + a DLL hot-injection integration test (`tests/test_inject_probe.c`, self-injects into the current process and verifies the DLL was actually loaded).
- **Manual `Test` workflow**: `.github/workflows/test.yml` is triggered manually via `workflow_dispatch` (choose linux / windows); it builds the core lib / kernel (`patcher_core.dll`) and runs the above via ctest, **without publishing artifacts**.
- **Ren'Py SDK demo**: see [`tests/README_test.md`](tests/README_test.md). The `tests/` directory ships:
  - `gen_demo_assets.py` — generates a small demo game (`options.rpy`, `script.rpy`, `images/logo.png`).
  - `make_real_rpa.py` — packs it with the **real RPA-3.0 format** (random XOR prefix).
  - `verify_rpa.py` — validates the patched output using Ren'Py loader logic (XOR decode + zlib inflate + pickle parse), and asserts byte-equality against the expected file set.

You can run the demo end-to-end, then drop the produced `patched.rpa` into a real Ren'Py SDK game to confirm the Chinese text shows up.

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
```
The registry is a plain-text file on disk (`$RENPY_PATCHER_HOME` or `~/.renpy_patcher/games.txt`; on Windows `%APPDATA%/renpy_patcher/games.txt`) — no database required.

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

## GUI Version (Windows / .NET 8 WPF)
Besides the CLI, there is a **GUI version** (Windows only, C# + WPF / .NET 8) with **feature parity** to the CLI: the core C library is built as `patcher_core.dll` and called via P/Invoke. CLI and GUI share the exact same core code.
- Game management: add / remove / list aliases
- Unpack / Inject (build, auto credit) / Credit / Export (with split)
- DLL Injection window: pick a DLL, choose a target (a running process PID, or launch an exe and inject, with a delay), driven by `patcher_core.dll`

The GUI, the core library and the CLI are all built from a single `CMakeLists.txt`. zlib is vendored as source under `third_party/zlib`, so **no vcpkg / system zlib / network** is required.

### Stability design
- **Error display**: on failure the log box shows "error code + meaning + detail", e.g.
  `[Unpack] failed: error code 2 (I/O error) | detail: cannot open file xxx`.
- **Crash-proof**: every button handler is wrapped in `try/catch` (with a low-level fallback); exceptions are logged, never pop a system error dialog.
- **Input validation**: files/directories are checked for existence before running.
- **Log sync**: the log box mirrors `patcher.log` incrementally and loads history on startup.

### Update check
- The GUI auto-checks silently on startup (prompts only when a newer build is found), or you can click **Check for updates** in Settings.
- Comparison: it fetches `version.json` from the GitHub `latest` branch and compares the **build number** with the local one (`latest > current` means an update exists).
- **Receive pre-releases** toggle (default off): when off, only stable releases are offered; when on, pre-releases are offered too. The toggle persists in `%APPDATA%/RenPyPatcher/gui.cfg`.

### Build GUI (Windows)
1. Install the [.NET 8 SDK](https://dotnet.microsoft.com/download) and Visual Studio 2019/2022 with "Desktop development with C++" (needed to compile the core DLL / CLI via MSVC + CMake).
2. Build the core DLL + CLI with CMake (emits `patcher_core.dll` and `patcher.exe`):
   ```bash
   cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```
   Or just open the folder as a CMake project in Visual Studio.
3. Publish the WPF app: `dotnet publish gui_wpf/gui_wpf.csproj -c Release -r win-x64 --self-contained true -o gui_publish`, then copy `patcher_core.dll` next to `renpy_patcher_gui.exe`.

> Note: the GUI reuses the same core library. The hot-injection `agent.dll` is only built when a Python dev package is present; the GUI itself does not depend on Python.

## Version Management
- **Single source of truth**: the root `VERSION` file (semver, e.g. `1.0.0`).
- **Auto-generated**: CMake parses `VERSION`, captures the git short commit, and writes `core/version.h` (pulled in by `core/plugin.h`) — no need to edit version macros by hand.
- **Check version**: `patcher --version` (or `-v`) prints the version, the build commit, and the license.
- **Build number**: injected automatically by CI (GitHub run number); used for the MSI four-segment version (so the installer can compare upgrades/downgrades) and for the GUI update check. Local builds default to 0.
- **Pre-release naming**: auto-releases use `V<major>.<minor>.<build>` (build = GitHub run number), e.g. `V1.4.148`; `version.json` reports `version` as `<major>.<minor>.<build>` with `prerelease=true`. A **stable** release uses `vX.Y.Z` (semver) and is cut only when a `.release_stable` marker is present at the repo root.
- **Release flow**:
  - Pushing to `main` (no `.release_stable`) → CI builds and publishes a **pre-release** (`V<major>.<minor>.<build>`) automatically.
  - Adding a `.release_stable` marker at the root + bumping `VERSION`, then pushing → CI builds and publishes a **stable release** (`vX.Y.Z`, non-prerelease).
  - The `Test` workflow (`.github/workflows/test.yml`) is a separate, manual `workflow_dispatch` that builds and runs ctest only (no publish).
- `core/version.h` is a generated file and is `.gitignore`d — do not commit it by hand.

## CI / Release
`.github/workflows/build.yml`:
- Triggers on push to `main` (auto build + pre-release) and on manual `workflow_dispatch` (build + stable release).
- Builds on **Ubuntu (gcc)** and **Windows (MSVC)**; packages CLI zips plus a Windows MSI installer and a `version.json` (consumed by the GUI update check).

`.github/workflows/test.yml` (separate, manual `workflow_dispatch`, optional linux/windows): builds the core lib / kernel and runs `ctest` (core API + DLL hot-injection integration) without publishing.
- Latest builds: [Releases](https://github.com/Zz131417/renpy_patcher/releases). Update history: [CHANGELOG.md](CHANGELOG.md).

To cut a stable release: open **Actions → Build & Package → Run workflow**, set the tag (e.g. `v1.1.0`) and `prerelease=false`.

## Progress / Roadmap
| Milestone | Scope | Status |
| --- | --- | --- |
| M1 | RPA-3.0 unpack | ✅ Done |
| M2 | As-is repack round-trip | ✅ Done |
| M3 | Direct injection (merge mod → repack) | ✅ Done |
| M4 | Hot injection (Windows process injection + CPython bridge) | ✅ Done (verified by DLL injection integration test) |
| M5 | Export / split (standard zip, in-house util/zip) | ✅ Done |
| GUI | Windows GUI (C# / WPF, on par with CLI, P/Invoke to patcher_core.dll, crash-proof + error display) | ✅ Done (v1.4.0) |
| M7 | External plugin DLL auto-load (registry entry reserved) | 🔲 Not implemented |
| M8 | Translation pipeline (dialog translation; menu/UI text pending) | ✅ Done (v1.1.0) |
| M9 | APK support | 🔲 Deferred |

## Contributing (extending the engine)
1. Implement the module interface from `core/plugin.h`.
2. Register it in `core/plugin_loader` (add a lookup entry and call it from `cli/main.c`).
3. Build and add tests under `tests/`.
4. Keep changes non-commercial-friendly (see License); do not introduce commercial-only code into this repo.

## Troubleshooting
- **`unpack` produces nothing / hangs** — older versions had a pickle-parser stack bug and an unbounded inflate loop; both are fixed. If you still see a hang, rebuild from current `main` (the loop now has a guard).
- **Repacked archive not recognized by the game** — confirm you did not change the logical paths; the index is path-sensitive and case-sensitive.
- **Windows build fails on zlib** — zlib is now vendored under `third_party/zlib` and built by CMake automatically; you no longer need vcpkg. If it fails, ensure the `third_party/zlib` source tree is present and you used the CMake commands above (not a hand-rolled `cl` invocation).

## FAQ
- **Is this a Ren'Py replacement?** No. It only reads/writes `.rpa` archives and (on Windows) injects into a running game. The game itself still runs on Ren'Py.
- **Does it modify Ren'Py?** Direct injection modifies the *archive*, not Ren'Py. Hot injection runs code inside the game's own Python — no Ren'Py files are touched.
- **Can I sell my mod?** No. The Non-Commercial License prohibits commercial use/distribution. Contact the author for a commercial license.
- **Why custom license instead of MIT?** MIT and other OSI licenses permit commercial use. This project explicitly forbids it, so a custom NC license is used (GitHub labels it `other`).

## License
This project is released under a **Non-Commercial License** (see [`LICENSE`](LICENSE)). Personal study, research, and non-commercial use are allowed; **commercial use is prohibited** (sale, embedding in commercial products, paid distribution, etc.) and requires a separate written license. The demo game and translation mod under `tests/` are subject to the same constraint.

> Note: this is a custom license, not an OSI-approved one. If you need a recognizable standard, consider CC BY-NC 4.0 for the content — but the code remains under this project's Non-Commercial License unless the author says otherwise.
