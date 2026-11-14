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
