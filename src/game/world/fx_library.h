#pragma once
// Particle presets from data/fx.json.
#include <map>
#include <string>

#include "engine/particles/particles.h"

namespace game {

class FxLibrary {
public:
    void load();  // throws on bad data
    const eng::EmitterDef* find(const std::string& id) const;
    size_t size() const { return emitters_.size(); }

private:
    std::map<std::string, eng::EmitterDef> emitters_;
};

}  // namespace game
