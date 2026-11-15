# Assets still to replace

All UI, SFX and music packs are in. Two sounds have no match in the Kenney packs and
are still synthesized by `tools/gen_audio.py`:

| Placeholder in repo | Wanted | Notes |
|---|---|---|
| `assets/audio/sfx/placeholder_boss_roar.wav` | A deep monster roar for the Bone King | Any CC0 roar works; point `boss_roar` in `data/audio.json` at it |
| `assets/audio/sfx/placeholder_fire.wav` | A fire whoosh for the Mage's Fire Nova | Point `fire` in `data/audio.json` at it |

`--validate` checks that every file in `data/audio.json` and every glyph in
`data/ui.json` exists and decodes.
