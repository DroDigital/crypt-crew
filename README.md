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

No installer, engine or runtime needed. Keep the executable next to its `assets/` and `data/`
folders (on macOS they live inside the app).

## How to play
Title -> **Play** -> lobby. Each player presses **Start/A** on a gamepad (or **Enter** on the
keyboard) to join, picks a class with left/right, and readies up. When everyone who joined is
ready, the crew descends: three floors of 7-8 rooms each, a shrine between floors, and the Bone
King at the bottom. A friend can still drop in at a shrine.

| Action | Gamepad (Xbox / PlayStation / Switch Pro) | Keyboard + mouse |
|---|---|---|
| Move | Left stick | WASD |
| Aim | Right stick (idle = aim assist) | Mouse |
| Attack | X / Square / Y, or RT | Left mouse or J |
| Special | Y / Triangle / X, or LT | Right mouse or K |
| Dodge roll | A / Cross / B, or RB | Space or Shift |
| Interact / revive (hold) | B / Circle / A, or LB | E |
| Pause | Start / Options / + | Esc or P |

Reviving a downed friend costs gold from the shared purse (25, doubling per revive on the same
floor). Settings (volumes, screen shake, fullscreen, resolution, vsync) are in the title and
pause menus and are saved to `%APPDATA%\CryptCrew`, `~/Library/Application Support/CryptCrew`
or `~/.local/share/crypt-crew`.

## Build from source
Requirements: CMake 3.25+, Ninja, a C++20 compiler (MSVC 2022, Apple Clang 14+, GCC 11+).
Linux also needs X11/GL headers: `sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`.

```
cmake --preset release
cmake --build --preset release
ctest --preset release
./build/release/CryptCrew            # macOS: open "build/release/Crypt Crew.app"
```

Useful flags:
- `--smoke-test [--steps N --players P --seed S]` — headless-ish run played by AI bots; with `--steps` > 3600 it must reach the run summary.
- `--validate` — no window: parses every glTF and data file and cross-checks references.
- `--capture <scene> --frames N [--every K] --out shots/` — scenes: `gallery`, `title`, `lobby`, `hud`, `relics`, `revive`, `summary`, `room:<template id>`, `icon`, `pieces:<model,...>`.
- `--dummies N` (AI teammates in the lobby), `--room <id> [--gold N]` (start in one room), `--seed S`, `--size WxH`, `--verbose`.

F1 toggles the debug overlay (FPS, seed, room, player states); F2 in the lobby adds an AI
teammate; in debug builds F5 hot-reloads `data/`.

## Layout
- `src/engine/` — game-agnostic systems: assets, animator, renderer, particles, physics (Jolt), navigation, input, audio, UI, debug.
- `src/game/` — the game: players, enemies, rooms, boss, UI, scenes.
- `data/` — every tunable number (JSON). `assets/` — CC0 models, textures, fonts, audio.
- `tools/asset_report` — dumps `docs/asset_report.md` from the glTF files.
- `docs/` — PROGRESS, DECISIONS, asset report.
