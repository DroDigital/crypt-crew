#include "game/ui/glyphs.h"

#include <cmath>

namespace game {
namespace {

enum class Face { Left, Top, Bottom, Right, Start, Select };

Face faceFor(Prompt p) {
    switch (p) {
        case Prompt::Attack: return Face::Left;
        case Prompt::Special: return Face::Top;
        case Prompt::Dodge: return Face::Bottom;
        case Prompt::Confirm: return Face::Bottom;
        case Prompt::Interact: return Face::Right;
        case Prompt::Back: return Face::Right;
        case Prompt::Pause: return Face::Start;
    }
    return Face::Bottom;
}

const char* keyLabel(Prompt p) {
    switch (p) {
        case Prompt::Attack: return "LMB";
        case Prompt::Special: return "RMB";
        case Prompt::Dodge: return "SPACE";
        case Prompt::Interact: return "E";
        case Prompt::Pause: return "ESC";
        case Prompt::Confirm: return "ENTER";
        case Prompt::Back: return "ESC";
    }
    return "?";
}

}  // namespace

Color slotColor(int slot) {
    static const Color c[4] = {{235, 80, 70, 255}, {70, 150, 255, 255}, {90, 210, 100, 255}, {245, 200, 60, 255}};
    return c[slot & 3];
}

namespace {

const char* familyKey(eng::PadFamily f) {
    switch (f) {
        case eng::PadFamily::Keyboard: return "keyboard";
        case eng::PadFamily::PlayStation: return "playstation";
        case eng::PadFamily::Switch: return "switch";
        default: return "xbox";
    }
}

const char* promptKey(Prompt p) {
    switch (p) {
        case Prompt::Attack: return "attack";
        case Prompt::Special: return "special";
        case Prompt::Dodge: return "dodge";
        case Prompt::Interact: return "interact";
        case Prompt::Pause: return "pause";
        case Prompt::Confirm: return "confirm";
        case Prompt::Back: return "back";
    }
    return "confirm";
}

}  // namespace

std::string glyphKey(eng::PadFamily fam, Prompt p) { return std::string("glyph.") + familyKey(fam) + "." + promptKey(p); }

namespace {
void drawFallbackGlyph(const eng::Ui& ui, eng::PadFamily fam, Prompt p, Vector2 c, float size);
}  // namespace

void drawGlyph(const eng::Ui& ui, eng::PadFamily fam, Prompt p, Vector2 c, float size) {
    // Kenney Input Prompts (data/ui.json); procedural fallback if an image is missing.
    const std::string key = glyphKey(fam, p);
    if (ui.hasImage(key)) {
        const float s = size * 1.25f;  // the prompt art has transparent padding
        ui.image(key, Rectangle{c.x - s * 0.5f, c.y - s * 0.5f, s, s});
        return;
    }
    drawFallbackGlyph(ui, fam, p, c, size);
}

namespace {
void drawFallbackGlyph(const eng::Ui& ui, eng::PadFamily fam, Prompt p, Vector2 c, float size) {
    const float r = size * 0.5f;
    if (fam == eng::PadFamily::Keyboard) {
        const char* k = keyLabel(p);
        const float ts = size * 0.42f;
        const float w = std::max(size, ui.measure(k, ts).x + size * 0.4f);
        const Rectangle rr{c.x - w * 0.5f, c.y - r, w, size};
        ui.roundRect(rr, 0.35f, Color{235, 235, 235, 255});
        ui.roundRect(Rectangle{rr.x + 2, rr.y + 2, rr.width - 4, rr.height - 6}, 0.35f, Color{250, 250, 250, 255});
        ui.text(k, Vector2{c.x, c.y - ts * 0.6f}, ts, Color{30, 30, 30, 255}, eng::Align::Center, false);
        return;
    }
    const Face f = faceFor(p);
    if (f == Face::Start || f == Face::Select) {
        const char* lab = fam == eng::PadFamily::PlayStation ? "OPT" : (fam == eng::PadFamily::Switch ? "+" : "MENU");
        ui.roundRect(Rectangle{c.x - size * 0.7f, c.y - r * 0.7f, size * 1.4f, size * 0.7f}, 0.6f, Color{60, 60, 70, 255});
        ui.text(lab, Vector2{c.x, c.y - size * 0.22f}, size * 0.38f, RAYWHITE, eng::Align::Center, false);
        return;
    }
    if (fam == eng::PadFamily::PlayStation) {
        ui.icon(c, r, Color{40, 40, 48, 255}, "");
        const Vector2 sc = ui.toScreen(c);
        const float s = r * 0.5f * ui.scale();
        const float th = 2.5f * ui.scale();
        switch (f) {
            case Face::Left: DrawRectangleLinesEx(Rectangle{sc.x - s, sc.y - s, 2 * s, 2 * s}, th, Color{240, 130, 200, 255}); break;
            case Face::Top: {
                Vector2 a{sc.x, sc.y - s * 1.1f}, b{sc.x - s * 1.1f, sc.y + s * 0.8f}, d{sc.x + s * 1.1f, sc.y + s * 0.8f};
                DrawLineEx(a, b, th, Color{90, 220, 180, 255});
                DrawLineEx(b, d, th, Color{90, 220, 180, 255});
                DrawLineEx(d, a, th, Color{90, 220, 180, 255});
                break;
            }
            case Face::Bottom:
                DrawLineEx(Vector2{sc.x - s, sc.y - s}, Vector2{sc.x + s, sc.y + s}, th, Color{120, 160, 255, 255});
                DrawLineEx(Vector2{sc.x - s, sc.y + s}, Vector2{sc.x + s, sc.y - s}, th, Color{120, 160, 255, 255});
                break;
            default: DrawRing(sc, s * 0.8f, s * 0.8f + th, 0, 360, 24, Color{250, 90, 90, 255}); break;
        }
        return;
    }
    // Xbox (default) and Switch (labels by position: left Y, top X, bottom B, right A).
    const bool sw = fam == eng::PadFamily::Switch;
    const char* lab = "A";
    Color col{90, 190, 90, 255};
    switch (f) {
        case Face::Left: lab = sw ? "Y" : "X"; col = Color{70, 130, 230, 255}; break;
        case Face::Top: lab = sw ? "X" : "Y"; col = Color{230, 190, 50, 255}; break;
        case Face::Bottom: lab = sw ? "B" : "A"; col = Color{90, 190, 90, 255}; break;
        case Face::Right: lab = sw ? "A" : "B"; col = Color{220, 70, 60, 255}; break;
        default: break;
    }
    if (sw) col = Color{70, 70, 78, 255};
    ui.icon(c, r, col, lab);
}

}  // namespace

float drawPrompt(const eng::Ui& ui, eng::PadFamily fam, Prompt p, const char* label, Vector2 pos, float size, Color textColor) {
    drawGlyph(ui, fam, p, Vector2{pos.x + size * 0.5f, pos.y + size * 0.5f}, size);
    float x = pos.x + size * (fam == eng::PadFamily::Keyboard ? 1.6f : 1.2f);
    if (fam == eng::PadFamily::Keyboard) x += 10.f;
    ui.text(label, Vector2{x, pos.y + size * 0.12f}, size * 0.75f, textColor);
    return x - pos.x + ui.measure(label, size * 0.75f).x;
}

}  // namespace game
