#pragma once
// Relics: data files (data/relics/*.json) with stat modifiers and an optional hook id.
#include <raylib.h>

#include <map>
#include <string>
#include <vector>

#include "engine/core/json_util.h"
#include "game/data/game_data.h"
#include "game/world/entities.h"

namespace game {





RelicDef parseRelic(const eng::json& j, const std::string& ctx);
const RelicDef* findRelic(const GameData& data, const std::string& id);
// Stats from a list of relic ids (duplicates stack additively).
PlayerStats computeStats(const GameData& data, const std::vector<std::string>& relics);
std::vector<std::string> hooksOf(const GameData& data, const std::vector<std::string>& relics);
// Known stat keys (validation).
bool isStatKey(const std::string& key);

}  // namespace game
