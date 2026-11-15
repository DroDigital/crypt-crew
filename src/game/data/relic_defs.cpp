#include "game/data/relic_defs.h"

#include <algorithm>
#include <array>

#include "game/data/game_data.h"

namespace game {
namespace {

const char* kStatKeys[] = {"move_mult",  "damage_mult",  "lifesteal", "special_cd_mult", "gold_mult",
                           "max_hp_bonus", "revive_speed_mult", "thorns", "aoe_mult", "kill_heal", "extra_dodges"};

}  // namespace

bool isStatKey(const std::string& key) {
    return std::any_of(std::begin(kStatKeys), std::end(kStatKeys), [&](const char* k) { return key == k; });
}

RelicDef parseRelic(const eng::json& j, const std::string& ctx) {
    RelicDef r;
    r.id = eng::req<std::string>(j, "id", ctx);
    r.name = eng::req<std::string>(j, "name", ctx);
    r.description = eng::req<std::string>(j, "description", ctx);
    r.glyph = eng::req<std::string>(j, "glyph", ctx);
    const auto c = eng::req<std::array<int, 3>>(j, "color", ctx);
    r.color = Color{static_cast<unsigned char>(c[0]), static_cast<unsigned char>(c[1]), static_cast<unsigned char>(c[2]), 255};
    const eng::json stats = eng::opt(j, "stats", eng::json::object());
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        if (!isStatKey(it.key())) throw std::runtime_error(ctx + ": unknown stat '" + it.key() + "'");
        r.add[it.key()] = it->get<float>();
    }
    r.hook = eng::opt<std::string>(j, "hook", "");
    r.classOnly = eng::opt<std::string>(j, "class", "");
    r.stackable = eng::opt(j, "stackable", true);
    r.price = eng::opt(j, "price", 60);
    return r;
}

const RelicDef* findRelic(const GameData& data, const std::string& id) {
    auto it = data.relics.find(id);
    return it == data.relics.end() ? nullptr : &it->second;
}

PlayerStats computeStats(const GameData& data, const std::vector<std::string>& relics) {
    PlayerStats s;
    for (const auto& id : relics) {
        const RelicDef* r = findRelic(data, id);
        if (!r) continue;
        for (const auto& [k, v] : r->add) {
            if (k == "move_mult") s.moveMult += v;
            else if (k == "damage_mult") s.damageMult += v;
            else if (k == "lifesteal") s.lifesteal += v;
            else if (k == "special_cd_mult") s.specialCdMult = std::max(0.2f, s.specialCdMult + v);
            else if (k == "gold_mult") s.goldMult += v;
            else if (k == "max_hp_bonus") s.maxHpBonus += v;
            else if (k == "revive_speed_mult") s.reviveSpeedMult += v;
            else if (k == "thorns") s.thorns += v;
            else if (k == "aoe_mult") s.aoeMult += v;
            else if (k == "kill_heal") s.killHeal += v;
            else if (k == "extra_dodges") s.extraDodges += static_cast<int>(v);
        }
    }
    return s;
}

std::vector<std::string> rollRelicOffer(const GameData& data, const std::string& classId,
                                        const std::vector<std::string>& owned, eng::Rng& rng, int count) {
    std::vector<std::string> pool;
    for (const auto& [id, r] : data.relics) {
        if (!r.classOnly.empty() && r.classOnly != classId) continue;
        if (!r.stackable && std::find(owned.begin(), owned.end(), id) != owned.end()) continue;
        pool.push_back(id);
    }
    std::vector<std::string> out;
    while (static_cast<int>(out.size()) < count && !pool.empty()) {
        const size_t i = static_cast<size_t>(rng.range(static_cast<int>(pool.size())));
        out.push_back(pool[i]);
        pool.erase(pool.begin() + static_cast<long>(i));
    }
    return out;
}

std::vector<std::string> hooksOf(const GameData& data, const std::vector<std::string>& relics) {
    std::vector<std::string> out;
    for (const auto& id : relics)
        if (const RelicDef* r = findRelic(data, id); r && !r->hook.empty()) out.push_back(r->hook);
    return out;
}

}  // namespace game
