#include "game/ui/settings_menu.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <raylib.h>

#include "game/ui/glyphs.h"

namespace game {
namespace {

enum Row { kMaster, kMusic, kSfx, kShake, kDisplay, kResolution, kVsync, kBack, kRows };
const int kResolutions[][2] = {{1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}};
constexpr int kResCount = 4;

int resolutionIndex(const Settings& s) {
    for (int i = 0; i < kResCount; ++i)
        if (kResolutions[i][0] == s.width && kResolutions[i][1] == s.height) return i;
    return 1;
}

}  // namespace

void SettingsMenu::apply(Settings& s, eng::AudioManager& audio) const {
    audio.setVolumes(s.master, s.music, s.sfx);
    if (!IsWindowReady()) return;
    const bool isFull = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
    if (s.fullscreen != isFull) ToggleBorderlessWindowed();
    if (!s.fullscreen && (GetScreenWidth() != s.width || GetScreenHeight() != s.height)) {
        SetWindowSize(s.width, s.height);
        const int m = GetCurrentMonitor();
        SetWindowPosition((GetMonitorWidth(m) - s.width) / 2, (GetMonitorHeight(m) - s.height) / 2);
    }
    if (s.vsync) SetWindowState(FLAG_VSYNC_HINT);
    else ClearWindowState(FLAG_VSYNC_HINT);
}

bool SettingsMenu::update(uint32_t b, Settings& s, eng::AudioManager& audio) {
    if (!open_) return false;
    if (b & eng::kBtnUp) focus_ = (focus_ + kRows - 1) % kRows, audio.play("ui_move");
    if (b & eng::kBtnDown) focus_ = (focus_ + 1) % kRows, audio.play("ui_move");
    const int dir = (b & eng::kBtnRight) ? 1 : (b & eng::kBtnLeft) ? -1 : 0;
    const bool confirm = b & eng::kBtnConfirm;
    bool changed = false;
    auto step = [&](float& v) {
        if (dir == 0) return;
        v = std::clamp(std::round((v + 0.1f * static_cast<float>(dir)) * 10.f) / 10.f, 0.f, 1.f);
        changed = true;
    };
    switch (focus_) {
        case kMaster: step(s.master); break;
        case kMusic: step(s.music); break;
        case kSfx: step(s.sfx); break;
        case kShake:
            if (dir || confirm) s.screenShake = !s.screenShake, changed = true;
            break;
        case kDisplay:
            if (dir || confirm) s.fullscreen = !s.fullscreen, changed = true;
            break;
        case kResolution:
            if (dir || confirm) {
                const int i = (resolutionIndex(s) + (dir ? dir : 1) + kResCount) % kResCount;
                s.width = kResolutions[i][0];
                s.height = kResolutions[i][1];
                changed = true;
            }
            break;
        case kVsync:
            if (dir || confirm) s.vsync = !s.vsync, changed = true;
            break;
        case kBack:
            if (confirm) {
                open_ = false;
                s.save();
                audio.play("ui_back");
                return true;
            }
            break;
        default: break;
    }
    if (b & eng::kBtnBack) {
        open_ = false;
        s.save();
        audio.play("ui_back");
        return true;
    }
    if (changed) {
        apply(s, audio);
        audio.play("ui_move");
    }
    return false;
}

void SettingsMenu::draw(const eng::Ui& ui, const Settings& s, eng::PadFamily fam) const {
    if (!open_) return;
    const float pw = 760, rowH = 62;
    const float ph = 110.f + rowH * static_cast<float>(kRows) + 70.f;
    const Rectangle panel{(ui.width() - pw) * 0.5f, (ui.height() - ph) * 0.5f, pw, ph};
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 140});
    ui.panel(panel, Color{230, 210, 170, 255});
    ui.text("SETTINGS", Vector2{panel.x + pw * 0.5f, panel.y + 24}, 48, Color{240, 220, 180, 255}, eng::Align::Center);
    const char* labels[kRows] = {"Master volume", "Music volume", "SFX volume", "Screen shake", "Display", "Resolution", "VSync", "Back"};
    char buf[64];
    for (int i = 0; i < kRows; ++i) {
        const Rectangle row{panel.x + 30, panel.y + 96 + static_cast<float>(i) * rowH, pw - 60, rowH - 10};
        const bool f = i == focus_;
        ui.roundRect(row, 0.2f, f ? Color{90, 75, 40, 230} : Color{30, 28, 40, 200});
        ui.text(labels[i], Vector2{row.x + 20, row.y + 12}, 26, f ? Color{255, 235, 190, 255} : RAYWHITE);
        const Vector2 right{row.x + row.width - 20, row.y + 12};
        switch (i) {
            case kMaster:
            case kMusic:
            case kSfx: {
                const float v = i == kMaster ? s.master : i == kMusic ? s.music : s.sfx;
                ui.bar(Rectangle{row.x + row.width - 320, row.y + 14, 230, 22}, v, Color{240, 200, 90, 255}, Color{20, 20, 26, 255});
                std::snprintf(buf, sizeof buf, "%d%%", static_cast<int>(std::lround(v * 100)));
                ui.text(buf, right, 24, RAYWHITE, eng::Align::Right);
                break;
            }
            case kShake: ui.text(s.screenShake ? "On" : "Off", right, 26, RAYWHITE, eng::Align::Right); break;
            case kDisplay: ui.text(s.fullscreen ? "Fullscreen" : "Windowed", right, 26, RAYWHITE, eng::Align::Right); break;
            case kResolution:
                std::snprintf(buf, sizeof buf, "%d x %d%s", s.width, s.height, s.fullscreen ? " (windowed)" : "");
                ui.text(buf, right, 26, RAYWHITE, eng::Align::Right);
                break;
            case kVsync: ui.text(s.vsync ? "On" : "Off", right, 26, RAYWHITE, eng::Align::Right); break;
            default: break;
        }
    }
    const float py = panel.y + ph - 56;
    drawPrompt(ui, fam, Prompt::Confirm, "Select", Vector2{panel.x + 40, py}, 34);
    ui.text("< >  Change", Vector2{panel.x + 240, py + 4}, 24, RAYWHITE);
    drawPrompt(ui, fam, Prompt::Back, "Back", Vector2{panel.x + pw - 200, py}, 34);
}

}  // namespace game
