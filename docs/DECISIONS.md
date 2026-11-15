# Decisions

Ambiguities resolved during development, newest at the bottom. Each entry: what, why.

## M0
- **raylib 6.2, Jolt 5.6.0, nlohmann/json 3.12.0, doctest 2.5.3** are the newest release tags at the time of setup; all fetched with FetchContent and `GIT_SHALLOW`.
- **Animation is sampled by our own code** (cgltf data, `src/engine/anim`). raylib 6.2 loads the clips correctly, but its API only blends two clips through a per-model pose and gives no events or per-joint access, so the Animator reads the clip tracks itself. raylib still loads meshes, materials and skin weights (non-skinned meshes parented to a joint, e.g. weapons in `handslot.r`, are bound to that joint by raylib's loader).
- **Each character pack has its own texture** (knight_texture.png, skeleton_texture.png, ...), embedded in the GLBs; only the 200 dungeon pieces share one atlas. Every dungeon piece embeds its own copy of that atlas, so the asset cache swaps them all to a single shared GPU texture.
- **Character GLBs contain every weapon variant.** Each class lists the meshes to hide in `data/classes/*.json` (`hide_meshes`). Skeleton GLBs contain no weapons; enemies attach the separate weapon glTFs to `handslot.l/r` (`attachments`).
- **Hit frames come from data, not guesses**: `tools/asset_report` samples every attack-like clip and reports when `handslot.r/l` moves fastest. `data/anims/*.json` use those times; deviations are noted in each file.
- **MSAA with post-processing**: the scene renders into a 4x multisampled RGBA16F framebuffer (GL entry points fetched via `rlGetProcAddress`), resolved into an HDR texture for bloom/tonemap/vignette. FLAG_MSAA_4X_HINT is still set for the backbuffer (UI).
- **Tonemap** is linear up to 0.75 with a soft shoulder rather than ACES, so the flat KayKit palette keeps its colors while torch highlights compress.
- **Kenney assets and music could not be downloaded** (403 for scripted requests). Placeholders are listed in TODO_ASSETS.md. The UI font placeholder is Russo One (OFL) from the google/fonts GitHub repo, the closest freely downloadable match to Kenney Future.
- **`--smoke-test` exits 77 when no OpenGL 3.3 context can be created**; CI then runs `--validate` instead and says so in the log.
- **Version** comes from `git describe --tags --always --dirty` at configure time (leading `v` stripped).
- **macOS** builds one universal binary (`CMAKE_OSX_ARCHITECTURES=arm64;x86_64`). Jolt detects SIMD per slice from compiler macros, so no lipo step is needed.
- **x86-64 baseline**: Jolt AVX/AVX2/AVX-512/LZCNT/TZCNT/F16C/FMADD are off; SSE4.2 stays on (x86-64-v2, every x86-64 CPU since ~2009).

## M1
- **One room at a time.** The floor is a grid of rooms, but only the current room is instantiated (meshes, Jolt statics, nav grid). Walking through an open door fades out and builds the neighbor. Simpler physics/nav and a camera that always frames one room.
- **Walls come from tile edges**, not wall tiles: every edge between a floor tile and void/outside becomes a 4 m `wall` (with a few cracked/broken variants, chosen by hash). South-facing edges use the low `barrier` piece and south doors are low too, so the 55-degree camera always sees the players.
- **Doors**: connected sockets get `wall_doorway_sides` + a `wall_gated` gate that sinks into the floor when the room is cleared; unused sockets are plain walls.
- **Nav grid** is 1 m cells over the room, blocked by void, wall edges and colliding props (inflated by 0.45 m).
- **Enemy melee reach**: enemies start a swing only when the target is within `trigger_range` (center distance) and the swing reaches `range` (+ target radius), stepping in during the wind-up.
- **Attack tokens**: an enemy needs a token on its target to start an attack (max 3 per player, data/config.json); target choice adds a penalty for already-swarmed players so groups spread.
- **Hitstop** freezes the simulation clock for 60/100 ms (`data/config.json`), never the frame rate.
- **Input mapping**: Attack = X/Square (left face) or RT, Special = Y/Triangle or LT, Dodge = A/Cross or RB, Interact = B/Circle or LB, Pause = Start. Keyboard: LMB/J, RMB/K, Space/Shift, E, Esc/P. Positional SDL mapping makes Switch Pro match (glyphs show Switch labels).
- **Damage numbers, telegraph glow and slot rings** are drawn by the run scene; telegraphs = wind-up clip (>= 0.4 s before the hit frame) + emissive red flash, plus a red lane decal for the rogue lunge.
- **Placeholder SFX/music** are synthesized by `tools/gen_audio.py` (deterministic, stdlib only).
- **Window creation failure**: raylib cannot clear FLAG_MSAA_4X_HINT once set and crashes if CloseWindow follows a failed InitWindow, so there is no in-process retry. If the driver rejects the multisampled backbuffer, the game turns `msaa_backbuffer` off in the user's settings.json and exits (code 77); the next launch works. The 3D scene keeps its own MSAA framebuffer either way.

## M3
- **Every room template has all four door sockets.** The generator can then connect any side; unused sockets render as plain walls.
- **Floor generator**: main chain of (total - 1) rooms by a seeded random walk going north/east/west (never south, so the chain reads bottom-to-top), plus exactly one treasure room as a dead-end branch off a middle room. One trap room sits in the middle of the chain; the last room is the shrine (floors 1-2) or the boss (floor 3). Templates are not repeated within a floor while unused ones remain.
- **Spawn markers per floor**: `e` (random) and markers for enemies not in the floor's pool (`data/floors.json`) become a random non-minion from that pool, so floor 1 has no mages. Minion markers are groups of 4-8, +30% per extra player.
- **Chests**: after every combat/trap room a chest pops up and opens by itself (gold + relic picks); treasure rooms have a chest that opens when a player walks up. KayKit has no open-lid chest, so opening is a hop + light + gold burst.
- **Relic picks are simultaneous and per player**: each player gets their own panel near their HUD corner and their input goes to it until they pick; bots pick at random after ~1 s. Offers skip relics of other classes and non-stackable relics already owned.
- **Relic stats stack additively** (Cracked Hourglass is clamped at -80% cooldown). Grand Sigil also widens melee reach by half its bonus so it helps every class.
- **Traps**: grate tiles (`T`) erupt on a phase-shifted 3.4 s cycle: 0.9 s red warning decal, then 1.3 s of fire that hurts players and enemies.
- **Bot navigation without a human**: bots walk to unopened chests, then to the next unvisited door (branch first), so the smoke test travels through rooms.

## M4
- **Shrine**: the statue (`pillar_decorated` with a glow) fully heals the team once per floor and stands downed teammates up. The merchant is the KayKit `Rogue_Hooded` model (same pack, imported for this) behind a table; he sells a Healing Draught (60% HP, 30 g) and 3 relics (relic price +25% per floor). Each player shops on their own panel, like relic picks.
- **Drop-in** happens in the shrine room: any unassigned device pressing Start/A/Enter joins with the first unused class. The newcomer starts with no relics.
- **Floor exits**: the last room of a floor gets one extra open door (preferring north) that leads down to the next floor; on the boss floor the run ends when the Bone King dies.
- **The Bone King** is the Skeleton Warrior scaled x2.2 with a dark tint; tunables in `data/enemies/bone_king.json`. Moves are picked on a cooldown by distance: 3-swing combo (close), Jump_Chop slam with a red disc + expanding warning ring then a shockwave ring that is low enough to dodge-roll through (i-frames), and a charge with a red lane telegraph that stops at walls. At 50% HP: one summon of 4 minions and 1.35x combo speed. His death triggers 1.6 s of 0.3x slow motion (simulation clock) and the victory sequence.
- **Summons and other mid-update spawns are queued** and applied after the enemy update so the enemy vector never reallocates under a loop.
- **Nav build is two-pass** (all floor first, then blockers) and obstacles are inflated by 0.7 m (largest hero radius + margin); connected doorways stay open in the grid.
- **Long smoke runs** (`--steps N > 3600`) must reach the run summary or fail; CI runs one full 4-bot run on Linux.

## M5
- **Balance** was tuned with 4 AI bots per player count (1-4) over many seeds (`--smoke-test --steps 600000 --players N --seed S`). Floors have 7-8 rooms and 2/3/3 combat waves, deeper floors scale enemy HP (x1/1.25/1.5) and damage (x1/1.2/1.4) on top of the player-count HP/spawn scaling from the design. Solo play also takes 20% less enemy damage (`enemy_damage_mult` in `data/scaling/players.json`), since a single bot otherwise lost 7 of 8 runs. Results after the pass: solo bots win about 1 in 3 runs, 2-4 bot crews win most runs with 0-4 revives.
- **Run length**: bots finish a run in 7-10 simulated minutes because they never stop to read, shop or explore. The 25-35 minute target for people is an estimate from that; it has not been measured with players.
- **Warrior shields**: blocking never cancels the warrior's own wind-up (otherwise spamming attacks into a shield stalled him), and a heavy blow breaks the guard for 0.9 s (`guard_break_time`).
- **Door triggers** need the player to actually step into the doorway (0.3 m past the line) so someone pushed against a closed door isn't whisked away the moment it opens.
- **Title -> lobby -> run -> summary -> lobby**; Back on the empty lobby returns to the title. Settings live in one menu shared by the title and the pause screen and save on close.
- **Bots are a test harness**, not shipped AI: they follow paths, flank shields, revive, pick relics at random and walk to the next door. A few seeds still leave them wedged; the CI full run uses a seed that completes.
