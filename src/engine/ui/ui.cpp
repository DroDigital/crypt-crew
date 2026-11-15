#include "engine/ui/ui.h"

#include <algorithm>
#include <cmath>

#include "engine/core/log.h"
#include "engine/core/paths.h"

namespace eng {
namespace {

Rectangle sr(const Rectangle& r, float s) { return Rectangle{r.x * s, r.y * s, r.width * s, r.height * s}; }

Texture2D makePanelTexture() {
    // 48x48 rounded panel with a light rim, for nine-slice drawing (16 px borders).
    const int n = 48;
    Image img = GenImageColor(n, n, BLANK);
    auto* px = static_cast<Color*>(img.data);
    const float r = 14.f;
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
            const float cx = std::clamp(static_cast<float>(x) + 0.5f, r, n - r), cy = std::clamp(static_cast<float>(y) + 0.5f, r, n - r);
            const float d = std::sqrt((x + 0.5f - cx) * (x + 0.5f - cx) + (y + 0.5f - cy) * (y + 0.5f - cy));
            if (d > r) continue;
            const float edge = std::clamp(r - d, 0.f, 1.f);
            const bool rim = d > r - 3.f;
            const float shade = 1.f - 0.25f * static_cast<float>(y) / n;
            Color c = rim ? Color{200, 190, 170, 255}
                          : Color{static_cast<unsigned char>(40 * shade), static_cast<unsigned char>(36 * shade),
                                  static_cast<unsigned char>(48 * shade), 235};
            c.a = static_cast<unsigned char>(c.a * edge);
            px[y * n + x] = c;
        }
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    return t;
}

}  // namespace

bool Ui::init(const std::string& fontPath) {
    font_ = LoadFontEx(resPath(fontPath).c_str(), 64, nullptr, 0);
    if (font_.texture.id == 0 || font_.glyphCount == 0) {
        log::error("ui: missing font %s", fontPath.c_str());
        font_ = GetFontDefault();
        ownFont_ = false;
    } else {
        ownFont_ = true;
        GenTextureMipmaps(&font_.texture);
        SetTextureFilter(font_.texture, TEXTURE_FILTER_TRILINEAR);
    }
    panelTex_ = makePanelTexture();
    return ownFont_;
}

bool Ui::loadImage(const std::string& key, const std::string& path) {
    const std::string full = resPath(path);
    Texture2D t = LoadTexture(full.c_str());
    if (t.id == 0) {
        log::error("ui: missing image %s", path.c_str());
        return false;
    }
    GenTextureMipmaps(&t);
    SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    if (auto it = images_.find(key); it != images_.end()) UnloadTexture(it->second);
    images_[key] = t;
    return true;
}

void Ui::image(const std::string& key, Rectangle dst, Color tint) const {
    auto it = images_.find(key);
    if (it == images_.end()) return;
    const Texture2D& t = it->second;
    const float k = std::min(dst.width / static_cast<float>(t.width), dst.height / static_cast<float>(t.height));
    const float w = static_cast<float>(t.width) * k, h = static_cast<float>(t.height) * k;
    const Rectangle r{dst.x + (dst.width - w) * 0.5f, dst.y + (dst.height - h) * 0.5f, w, h};
    DrawTexturePro(t, Rectangle{0, 0, static_cast<float>(t.width), static_cast<float>(t.height)}, sr(r, scale_),
                   Vector2{0, 0}, 0.f, tint);
}

void Ui::shutdown() {
    for (auto& [k, t] : images_) UnloadTexture(t);
    images_.clear();
    if (ownFont_) UnloadFont(font_);
    if (panelTex_.id) UnloadTexture(panelTex_);
    ownFont_ = false;
    panelTex_ = Texture2D{};
}

void Ui::beginFrame() {
    const float sw = static_cast<float>(GetScreenWidth()), sh = static_cast<float>(GetScreenHeight());
    scale_ = std::max(0.1f, std::min(sw / 1920.f, sh / 1080.f));
    vw_ = sw / scale_;
    vh_ = sh / scale_;
}

Vector2 Ui::measure(const char* t, float size) const {
    const Vector2 m = MeasureTextEx(font_, t, size, size * 0.04f);
    return m;
}

void Ui::text(const char* t, Vector2 pos, float size, Color c, Align a, bool shadow) const {
    const Vector2 m = measure(t, size);
    if (a == Align::Center) pos.x -= m.x * 0.5f;
    else if (a == Align::Right) pos.x -= m.x;
    const float s = scale_;
    if (shadow)
        DrawTextEx(font_, t, Vector2{(pos.x + size * 0.06f) * s, (pos.y + size * 0.06f) * s}, size * s, size * 0.04f * s,
                   Color{0, 0, 0, static_cast<unsigned char>(c.a * 0.7f)});
    DrawTextEx(font_, t, Vector2{pos.x * s, pos.y * s}, size * s, size * 0.04f * s, c);
}

void Ui::textFit(const char* t, Vector2 pos, float size, float maxWidth, Color c, Align a) const {
    const float w = measure(t, size).x;
    const float s = (w > maxWidth && w > 0.f) ? size * maxWidth / w : size;
    text(t, Vector2{pos.x, pos.y + (size - s) * 0.5f}, s, c, a);
}

void Ui::panel(Rectangle r, Color tint) const {
    NPatchInfo n{Rectangle{0, 0, 48, 48}, 16, 16, 16, 16, NPATCH_NINE_PATCH};
    DrawTextureNPatch(panelTex_, n, sr(r, scale_), Vector2{0, 0}, 0.f, tint);
}

void Ui::rect(Rectangle r, Color c) const { DrawRectangleRec(sr(r, scale_), c); }

void Ui::roundRect(Rectangle r, float roundness, Color c) const { DrawRectangleRounded(sr(r, scale_), roundness, 6, c); }

void Ui::bar(Rectangle r, float frac, Color fg, Color bg, Color flash, float flashFrac) const {
    frac = std::clamp(frac, 0.f, 1.f);
    roundRect(r, 0.4f, bg);
    if (flashFrac > frac && flash.a > 0)
        roundRect(Rectangle{r.x, r.y, r.width * std::clamp(flashFrac, 0.f, 1.f), r.height}, 0.4f, flash);
    if (frac > 0.001f) roundRect(Rectangle{r.x, r.y, r.width * frac, r.height}, 0.4f, fg);
    DrawRectangleRoundedLinesEx(sr(r, scale_), 0.4f, 6, 1.5f * scale_, Color{0, 0, 0, 160});
}

void Ui::icon(Vector2 c, float radius, Color fill, const char* label, Color labelColor) const {
    DrawCircleV(toScreen(c), radius * scale_, fill);
    DrawCircleLinesV(toScreen(c), radius * scale_, Color{0, 0, 0, 180});
    if (label && *label) {
        const float size = radius * 1.15f;
        const Vector2 m = measure(label, size);
        text(label, Vector2{c.x - m.x * 0.5f, c.y - m.y * 0.5f}, size, labelColor, Align::Left, false);
    }
}

void Ui::ring(Vector2 c, float radius, float frac, Color col, float thickness) const {
    frac = std::clamp(frac, 0.f, 1.f);
    if (frac <= 0.f) return;
    DrawRing(toScreen(c), (radius - thickness) * scale_, radius * scale_, -90.f, -90.f + 360.f * frac, 36, col);
}

bool Ui::button(const char* label, Rectangle r, bool focused, bool activate, bool enabled) const {
    Color tint = focused ? Color{255, 220, 140, 255} : Color{170, 170, 190, 255};
    if (!enabled) tint = Color{90, 90, 100, 255};
    panel(r, tint);
    const float size = r.height * 0.5f;
    text(label, Vector2{r.x + r.width * 0.5f, r.y + (r.height - size) * 0.5f}, size,
         enabled ? (focused ? Color{255, 240, 200, 255} : RAYWHITE) : GRAY, Align::Center);
    if (focused) {
        const float t = static_cast<float>(GetTime());
        const float a = 0.5f + 0.5f * std::sin(t * 6.f);
        DrawRectangleRoundedLinesEx(sr(Rectangle{r.x - 4, r.y - 4, r.width + 8, r.height + 8}, scale_), 0.3f, 6, 2.f * scale_,
                                    Color{255, 210, 120, static_cast<unsigned char>(120 + 120 * a)});
    }
    return focused && activate && enabled;
}

}  // namespace eng
