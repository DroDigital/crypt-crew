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

## M2 Couch co-op — done
- InputRouter (pure logic, unit-tested): devices to slots, join/leave, preferred-slot join, latched presses for the fixed step, per-frame edges for menus, disconnect detection.
- Lobby: four join slots, class preview rendered into a render texture per slot, join with Start/A/Enter, pick class, ready, Back leaves; all ready -> 2 s countdown -> run. F2 adds an AI teammate.
- All 4 classes playable; shared camera follows the living players, zooms to fit (min/max distance), invisible leash at max zoom, focus clamped to the room.
- Downed (12 s, crawl) and revive: hold Interact 1.5 s next to a teammate, 25 gold doubling per revive on the same floor; "Need X gold" when the purse is short; everyone down = run over.
- Pause menu from any player (Resume / Quit to Lobby); a disconnected pad pauses with "Reconnect P2's controller".
- Verified by driving the real game with synthetic keyboard/mouse input under Xvfb (join, class change, ready, fight, special, pause) and with `--capture revive` / `--capture lobby`.

### Known issues
- Drop-in at the shrine arrives with the shrine room (M4).
- Game over returns to the lobby until the run summary exists (M4).

## M3 Content — done
- 12 room templates (7 combat, 2 treasure, 1 trap, 1 shrine, 1 boss) in `data/rooms/`, verified with `--capture room:<id>`.
- Seeded floor generator: 6-8 rooms, one treasure side branch, trap room mid-chain, shrine/boss at the end; unit-tested over 600 seeds (connected, symmetric doors, no overlaps, one branch).
- Doors lock while enemies live and open on clear; the floor's last room has an exit door.
- 4 enemy types with telegraphs and per-floor enemy pools.
- Chests with gold + simultaneous 1-of-3 relic picks per player; 15 relics as data (stat modifiers + hook ids) with a C++ hook table (on_hit, on_kill, on_dodge, on_revive, on_room_clear).
- Trap grates with warning decals.

## M4 Full run — done
- Lobby -> Floor 1 -> shrine -> Floor 2 -> shrine -> Floor 3 -> The Bone King -> run summary -> lobby.
- Three floors with their own lighting, banners and enemy pools (Crypt blue, Ossuary green, Throne of Bones red).
- Shrine: healing statue (once per floor), merchant shop (heals + relics, per-player panels), drop-in for new players.
- The Bone King: combo, slam + shockwave ring, charge, phase 2 summon + faster combo, boss bar, boss music crossfade, slow-motion death.
- Run summary: victory/defeat, time, floor reached, rooms cleared, gold earned, revives used, per-player kills/damage/downs/revives/relics and who died to what.
- Debug flags: `--room <id>`, `--gold N`, `--steps N`, `--every K` (capture sequences).
- Verified: 16 seeds of a 4-bot full run all reach victory deterministically (`--smoke-test --steps 200000 --seed S`); shop and statue driven with synthetic keyboard input.

### Known issues
- Balance: 4 bots finish a run in ~4 simulated minutes with almost no revives; the M5 balance pass will lengthen and harden rooms toward the 25-35 minute target.
- Drop-in was tested through the same InputRouter path as the lobby; a second physical device was not available here.

### Next
- M5: audio pass, particles, title/menus/settings, balance at 1-4 players, icons, version on title, README, v0.1.0 release.
