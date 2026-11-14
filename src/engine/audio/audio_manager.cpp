#include "engine/audio/audio_manager.h"

#include <algorithm>
#include <filesystem>

#include "engine/core/log.h"
#include "engine/core/paths.h"

namespace eng {

bool AudioManager::init() {
    InitAudioDevice();
    ready_ = IsAudioDeviceReady();
    if (!ready_) log::warn("audio: no audio device, running silent");
    return ready_;
}

void AudioManager::shutdown() {
    if (!ready_) return;
    for (auto& [id, s] : sfx_map_) {
        for (Sound& v : s.voices) UnloadSoundAlias(v);
        for (Sound& v : s.sources) UnloadSound(v);
    }
    sfx_map_.clear();
    for (auto& [id, t] : tracks_) UnloadMusicStream(t.music);
    tracks_.clear();
    CloseAudioDevice();
    ready_ = false;
}

bool AudioManager::loadSfx(const std::string& id, const std::vector<std::string>& files, int voices, float volume,
                           float pitchVar) {
    bool ok = true;
    for (const auto& f : files)
        if (!std::filesystem::exists(u8path(resPath(f)))) {
            log::error("audio: missing %s", f.c_str());
            ok = false;
        }
    if (!ready_ || !ok) return ok;
    Sfx s;
    s.volume = volume;
    s.pitchVar = pitchVar;
    for (const auto& f : files) {
        Sound snd = LoadSound(resPath(f).c_str());
        if (snd.frameCount == 0) {
            log::error("audio: failed to load %s", f.c_str());
            ok = false;
            continue;
        }
        s.sources.push_back(snd);
    }
    if (s.sources.empty()) return false;
    for (int v = 0; v < std::max(voices, 1); ++v) s.voices.push_back(LoadSoundAlias(s.sources[static_cast<size_t>(v) % s.sources.size()]));
    sfx_map_[id] = std::move(s);
    return ok;
}

bool AudioManager::loadMusic(const std::string& id, const std::string& file) {
    if (!std::filesystem::exists(u8path(resPath(file)))) {
        log::error("audio: missing %s", file.c_str());
        return false;
    }
    if (!ready_) return true;
    Track t;
    t.music = LoadMusicStream(resPath(file).c_str());
    t.music.looping = true;
    tracks_[id] = t;
    return true;
}

void AudioManager::play(const std::string& id, float volume, float pitch) {
    if (!ready_) return;
    auto it = sfx_map_.find(id);
    if (it == sfx_map_.end()) return;
    Sfx& s = it->second;
    Sound& v = s.voices[s.next];
    s.next = (s.next + 1) % s.voices.size();
    SetSoundVolume(v, std::clamp(s.volume * volume * sfx_ * master_, 0.f, 1.f));
    SetSoundPitch(v, pitch * (1.f + rng_.uniform(-s.pitchVar, s.pitchVar)));
    PlaySound(v);
}

void AudioManager::playMusic(const std::string& id, float fade) {
    if (id == musicId_) return;
    for (auto& [k, t] : tracks_) {
        t.target = (k == id) ? 1.f : 0.f;
        t.rate = 1.f / std::max(fade, 0.05f);
        if (k == id && !t.playing) {
            SeekMusicStream(t.music, 0.f);
            PlayMusicStream(t.music);
            t.playing = true;
            t.gain = 0.f;
        }
    }
    musicId_ = id;
}

void AudioManager::stopMusic(float fade) {
    for (auto& [k, t] : tracks_) {
        t.target = 0.f;
        t.rate = 1.f / std::max(fade, 0.05f);
    }
    musicId_.clear();
}

void AudioManager::update(float dt) {
    if (!ready_) return;
    for (auto& [k, t] : tracks_) {
        if (!t.playing) continue;
        if (t.gain < t.target) t.gain = std::min(t.target, t.gain + dt * t.rate);
        else if (t.gain > t.target) t.gain = std::max(t.target, t.gain - dt * t.rate);
        SetMusicVolume(t.music, t.gain * musicBase_ * music_ * master_);
        UpdateMusicStream(t.music);
        if (t.gain <= 0.f && t.target <= 0.f) {
            StopMusicStream(t.music);
            t.playing = false;
        }
    }
}

void AudioManager::setVolumes(float master, float music, float sfx) {
    master_ = std::clamp(master, 0.f, 1.f);
    music_ = std::clamp(music, 0.f, 1.f);
    sfx_ = std::clamp(sfx, 0.f, 1.f);
}

}  // namespace eng
