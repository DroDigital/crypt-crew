# Assets still to replace

These downloads failed from the build environment (kenney.nl answers non-browser
requests with HTTP 403; itch.io downloads need a browser session). Placeholders are
in the game under clearly marked names. Drop the real files in and update
`data/audio.json` / `data/ui.json`; no code changes are needed.

| Placeholder in repo | Intended asset | Source |
|---|---|---|
| `assets/fonts/RussoOne-Regular.ttf` (OFL font) | Kenney Fonts: "Kenney Future" (CC0) | https://kenney.nl/assets/kenney-fonts |
| Procedurally drawn button glyphs (`src/game/ui/glyphs.cpp`) | Kenney Input Prompts (CC0) | https://kenney.nl/assets/input-prompts |
| `assets/audio/sfx/placeholder_*.wav` (synthesized by `tools/gen_audio.py`) | Kenney RPG Audio, Impact Sounds, Interface Sounds (CC0) | https://kenney.nl/assets/rpg-audio, https://kenney.nl/assets/impact-sounds, https://kenney.nl/assets/interface-sounds |
| `assets/audio/music/placeholder_explore.wav`, `placeholder_boss.wav` (synthesized) | Tallbeard Studios "FREE Music Loop Bundle" (CC0): one explore loop, one boss loop | https://tallbeard.itch.io/music-loop-bundle |
