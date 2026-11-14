#pragma once
// raylib audio: sound pools (aliases) so the same SFX can overlap, slight pitch
// randomization, music streams with crossfade, Master/Music/SFX buses.
#include <raylib.h>

#include <map>
#include <string>
#include <vector>

#include "engine/core/rng.h"

namespace eng {

class AudioManager {
public:
    bool init();  // false if no audio device (everything becomes a no-op)
    void shutdown();
    bool ready() const { return ready_; }

    // Returns false if a file is missing.
    bool loadSfx(const std::string& id, const std::vector<std::string>& files, int voices, float volume, float pitchVar);
    bool loadMusic(const std::string& id, const std::string& file);
    void play(const std::string& id, float volume = 1.f, float pitch = 1.f);
    void playMusic(const std::string& id, float fadeSeconds = 1.5f);
    void stopMusic(float fadeSeconds = 1.f);
    const std::string& currentMusic() const { return musicId_; }
    void update(float realDt);

    void setVolumes(float master, float music, float sfx);
    float master() const { return master_; }
    float musicVolume() const { return music_; }
    float sfxVolume() const { return sfx_; }
    void setMusicBase(float v) { musicBase_ = v; }

private:
    struct Sfx {
        std::vector<Sound> sources;  // one per file
        std::vector<Sound> voices;   // aliases
        size_t next = 0;
        float volume = 1.f, pitchVar = 0.f;
    };
    struct Track {
        Music music{};
        float gain = 0.f;  // fade gain 0..1
        float target = 0.f;
        float rate = 1.f;
        bool playing = false;
    };
    bool ready_ = false;
    std::map<std::string, Sfx> sfx_map_;
    std::map<std::string, Track> tracks_;
    std::string musicId_;
    float master_ = 1.f, music_ = 0.8f, sfx_ = 1.f, musicBase_ = 0.6f;
    Rng rng_{1234};
};

}  // namespace eng
