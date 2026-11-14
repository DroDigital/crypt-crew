# Crypt Crew

A 1–4 player local (couch) co-op 3D dungeon roguelite. Up to four adventurers dive
into a crypt built from hand-made rooms, clear them of skeletons, pick relics, beat
the floor boss and go deeper. Reviving a downed friend costs gold from the shared
purse, so the team keeps deciding: save the friend or save the money.

Native C++20 on raylib with a small hand-written engine layer (no game engine).
Windows, macOS and Linux.

## Download and play
Grab the archive for your OS from the [Releases](../../releases) page:

| OS | File | Launch |
|---|---|---|
| Windows 10+ x64 | `CryptCrew-<version>-windows-x64.zip` | Unzip, run `CryptCrew.exe`. SmartScreen: *More info > Run anyway*. |
| macOS 11+ (Apple Silicon and Intel) | `CryptCrew-<version>-macos-universal.zip` | Unzip, open `Crypt Crew.app`. If it is not notarized: right-click > *Open*, or *System Settings > Privacy & Security > Open Anyway*. |
| Linux x64 | `CryptCrew-<version>-linux-x64.tar.gz` | `tar xzf ...`, run `./CryptCrew`. |

No installer, engine or runtime needed. See `packaging/README.txt` for controls.

## Build from source
Requirements: CMake 3.25+, Ninja, a C++20 compiler (MSVC 2022, Apple Clang 14+, GCC 11+).
Linux also needs X11/GL headers: `sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`.

```
cmake --preset release
cmake --build --preset release
ctest --preset release
./build/release/CryptCrew            # macOS: open "build/release/Crypt Crew.app"
```

Useful flags: `--smoke-test`, `--validate`, `--capture <scene> --frames N --out shots/`,
`--dummies N` (AI teammates that follow player 1), `--seed S`, `--size WxH`.
In debug builds F5 hot-reloads `data/`, F1 toggles the debug overlay.

## Layout
- `src/engine/` — game-agnostic systems: assets, animator, renderer, particles, physics (Jolt), navigation, input, audio, UI, debug.
- `src/game/` — the game: players, enemies, rooms, boss, UI, scenes.
- `data/` — every tunable number (JSON). `assets/` — CC0 models, textures, fonts, audio.
- `tools/asset_report` — dumps `docs/asset_report.md` from the glTF files.
- `docs/` — PROGRESS, DECISIONS, asset report.
