# Progress

## M0 Setup — done
- CMake project with `debug`/`release` presets; raylib 6.2, Jolt 5.6.0, nlohmann/json 3.12.0, doctest 2.5.3 via FetchContent (pinned tags).
- KayKit Adventurers, Skeletons and Dungeon Remastered imported (glTF/GLB + textures only) into `assets/`.
- `tools/asset_report` writes `docs/asset_report.md` (node tree, meshes, AABBs, joints, clips, durations, hit-frame analysis).
- Engine: asset cache, cgltf clip sampler + Animator (crossfade, events, per-clip speed, joint matrices), lit renderer with GPU skinning, 8 nearest point lights, fog, hit flash/tint, blob shadows, decals, MSAA HDR target + bloom/tonemap/vignette.
- Gallery scene with all 4 heroes and 4 skeletons idling; verified with `--capture gallery`.
- `--validate` (no window), `--smoke-test`, `--capture`; unit tests (doctest).
- CI: Windows/macOS/Linux build, tests, smoke test (or validate), packaging, artifacts; GitHub Release on `v*` tags.

### Known issues
- Kenney font/glyphs/SFX and the music loops are placeholders (TODO_ASSETS.md).
- macOS CI runners cannot create an OpenGL context ("NSGL: Failed to find a suitable pixel format"); CI runs `--validate` there instead of the smoke test.

## M1 One hero, one room — done
- Jolt physics wrapper (CharacterVirtual per character, static boxes per room, removable door colliders, raycasts).
- Rooms from data: ASCII grid + shared legend (`data/rooms/_legend.json`), props, door sockets, spawn markers; meshes, colliders, nav grid and torch lights all built from the same tiles. Floor themes in `data/floors.json`.
- Player: move, aim (mouse ray / right stick / 30-degree auto-aim cone), 3-hit combo with buffered input and lunge, dodge roll (0.35 s, 0.2 s i-frames, 1 s cooldown, charges), four specials (Shield Bash + guard + taunt, Rage, Fire Nova, Shadow Dash + crit window), downed/revive logic.
- Enemies: minion swarm, shielded warrior (frontal block, slow turn), circling rogue with 0.5 s lunge telegraph, kiting mage with homing bolts; attack tokens; A* + separation steering.
- Combat: hitbox/hurtbox overlap tests per step with hit lists, crits, knockback, hit flash, hitstop, screen shake, damage numbers, gold drops with magnet.
- Particles (instanced billboards, pooled, capped) with presets in `data/fx.json`; audio manager (pools, pitch variance, music crossfade, buses) with placeholder sounds.
- HUD: corner panels (HP, special cooldown dial with glyph, relics), shared purse, floor/room counter; procedural button glyphs per pad family.
- Debug AI teammates (`--dummies N`); smoke test now runs a 4-bot run.

### Known issues
- Game feel was tuned with bots and keyboard here; a physical gamepad was not available in the build environment.
- The M1 run loops a single room (`combat_pillars`); floors arrive in M3.

### Next
- M2: lobby with join/leave and class pick, all 4 classes playable by humans, disconnect pause, downed/revive polish.
