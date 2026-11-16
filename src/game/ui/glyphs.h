#pragma once
// Button prompts per device family. Placeholder drawn with primitives until the Kenney
// Input Prompts pack is added (TODO_ASSETS.md); same API either way.
#include <raylib.h>

#include <string>

#include "engine/input/input_types.h"
#include "engine/ui/ui.h"

namespace game {

enum class Prompt { Attack, Special, Dodge, Interact, Pause, Confirm, Back };

void drawGlyph(const eng::Ui& ui, eng::PadFamily fam, Prompt p, Vector2 center, float size);
// Glyph followed by a label; returns the total width drawn.
float drawPrompt(const eng::Ui& ui, eng::PadFamily fam, Prompt p, const char* label, Vector2 pos, float size,
                 Color textColor = RAYWHITE);
Color slotColor(int slot);
// Image key in the UI registry for a prompt, e.g. "glyph.xbox.attack".
std::string glyphKey(eng::PadFamily fam, Prompt p);

}  // namespace game
