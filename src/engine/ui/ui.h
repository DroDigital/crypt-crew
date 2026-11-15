#pragma once
// Small immediate-mode UI on raylib drawing. Layout happens in virtual units where the
// shorter screen axis maps to 1080 (so 1920x1080 at 16:9); corners follow the real screen.
#include <raylib.h>

#include <cstdint>
#include <map>
#include <string>

namespace eng {

enum class Align { Left, Center, Right };

class Ui {
public:
    bool init(const std::string& fontPath);
    void shutdown();
    void beginFrame();

    float width() const { return vw_; }   // virtual screen size
    float height() const { return vh_; }
    float scale() const { return scale_; }
    Vector2 toScreen(Vector2 v) const { return Vector2{v.x * scale_, v.y * scale_}; }
    Vector2 fromScreen(Vector2 s) const { return Vector2{s.x / scale_, s.y / scale_}; }

    void text(const char* t, Vector2 pos, float size, Color c, Align a = Align::Left, bool shadow = true) const;
    Vector2 measure(const char* t, float size) const;
    // Like text(), but shrinks the size so the string fits in maxWidth.
    void textFit(const char* t, Vector2 pos, float size, float maxWidth, Color c, Align a = Align::Left) const;
    void panel(Rectangle r, Color tint) const;
    void rect(Rectangle r, Color c) const;
    void roundRect(Rectangle r, float roundness, Color c) const;
    void bar(Rectangle r, float frac, Color fg, Color bg, Color flash = BLANK, float flashFrac = 0.f) const;
    void icon(Vector2 center, float radius, Color fill, const char* label, Color labelColor = WHITE) const;
    void ring(Vector2 center, float radius, float frac, Color c, float thickness = 6.f) const;
    // A focusable button; returns true if `activate` is set while focused.
    bool button(const char* label, Rectangle r, bool focused, bool activate, bool enabled = true) const;
    Font font() const { return font_; }
    // Named images (e.g. button prompts). Returns false if the file is missing.
    bool loadImage(const std::string& key, const std::string& path);
    bool hasImage(const std::string& key) const { return images_.count(key) > 0; }
    // Draws the image fitted into `dst` (virtual units), keeping its aspect ratio.
    void image(const std::string& key, Rectangle dst, Color tint = WHITE) const;

private:
    Font font_{};
    Texture2D panelTex_{};
    std::map<std::string, Texture2D> images_;
    bool ownFont_ = false;
    float scale_ = 1.f, vw_ = 1920.f, vh_ = 1080.f;
};

}  // namespace eng
