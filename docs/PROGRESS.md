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
- The M0 smoke test runs the gallery; it becomes a 4-player run in M2.

### Next
- M1: one hero in one room — movement, dodge, combo, special, one enemy, hitstop and flash.
