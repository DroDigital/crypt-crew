#include "game/data/game_data.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "engine/core/paths.h"
#include "game/data/relic_defs.h"

namespace game {
namespace {

Color colorFrom(const eng::json& j, Color fallback) {
    if (!j.is_array() || j.size() < 3) return fallback;
    return Color{j[0].get<unsigned char>(), j[1].get<unsigned char>(), j[2].get<unsigned char>(),
                 j.size() > 3 ? j[3].get<unsigned char>() : static_cast<unsigned char>(255)};
}

AttackStep parseStep(const eng::json& j, const std::string& ctx) {
    AttackStep s;
    s.anim = eng::req<std::string>(j, "anim", ctx);
    s.damage = eng::req<float>(j, "damage", ctx);
    s.range = eng::req<float>(j, "range", ctx);
    s.arcDeg = eng::opt(j, "arc_deg", 90.f);
    s.knockback = eng::opt(j, "knockback", 0.f);
    s.lunge = eng::opt(j, "lunge", 0.f);
    s.speed = eng::opt(j, "speed", 0.f);
    s.radius = eng::opt(j, "radius", 0.f);
    s.heavy = eng::opt(j, "heavy", false);
    return s;
}

std::vector<std::string> listJson(const std::string& dirRel) {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(eng::resourceDir() / dirRel, ec))
        if (e.path().extension() == ".json") out.push_back(dirRel + "/" + e.path().filename().string());
    std::sort(out.begin(), out.end());
    return out;
}

ClassDef parseClass(const eng::json& j, const std::string& ctx) {
    ClassDef c;
    c.id = eng::req<std::string>(j, "id", ctx);
    c.name = eng::req<std::string>(j, "name", ctx);
    c.description = eng::req<std::string>(j, "description", ctx);
    c.animMap = eng::req<std::string>(j, "anim_map", ctx);
    c.hideMeshes = eng::opt(j, "hide_meshes", std::vector<std::string>{});
    c.color = colorFrom(eng::opt(j, "color", eng::json()), WHITE);
    c.hp = eng::req<float>(j, "hp", ctx);
    c.moveSpeed = eng::req<float>(j, "move_speed", ctx);
    c.radius = eng::req<float>(j, "radius", ctx);
    c.height = eng::req<float>(j, "height", ctx);
    const auto& b = j.at("basic");
    c.basicType = eng::req<std::string>(b, "type", ctx + ":basic");
    c.comboWindow = eng::req<float>(b, "combo_window", ctx + ":basic");
    c.moveDuring = eng::opt(b, "move_during", 0.3f);
    for (const auto& s : b.at("combo")) c.combo.push_back(parseStep(s, ctx + ":basic.combo"));
    if (c.combo.empty()) throw std::runtime_error(ctx + ": empty combo");
    c.special = j.at("special");
    c.specialType = eng::req<std::string>(c.special, "type", ctx + ":special");
    c.specialCooldown = eng::req<float>(c.special, "cooldown", ctx + ":special");
    return c;
}

EnemyDef parseEnemy(const eng::json& j, const std::string& ctx) {
    EnemyDef e;
    e.raw = j;
    e.id = eng::req<std::string>(j, "id", ctx);
    e.name = eng::req<std::string>(j, "name", ctx);
    e.animMap = eng::req<std::string>(j, "anim_map", ctx);
    e.behavior = eng::req<std::string>(j, "behavior", ctx);
    for (const auto& a : eng::opt(j, "attachments", eng::json::array()))
        e.attachments.push_back(Attachment{eng::req<std::string>(a, "model", ctx), eng::req<std::string>(a, "joint", ctx)});
    e.hp = eng::req<float>(j, "hp", ctx);
    e.moveSpeed = eng::req<float>(j, "move_speed", ctx);
    e.radius = eng::req<float>(j, "radius", ctx);
    e.height = eng::req<float>(j, "height", ctx);
    e.scale = eng::opt(j, "scale", 1.f);
    e.tint = colorFrom(eng::opt(j, "tint", eng::json()), WHITE);
    e.attack = parseStep(j.at("attack"), ctx + ":attack");
    e.attackCooldown = eng::req<float>(j.at("attack"), "cooldown", ctx + ":attack");
    e.triggerRange = eng::req<float>(j.at("attack"), "trigger_range", ctx + ":attack");
    e.gold = eng::req<std::array<int, 2>>(j, "gold", ctx);
    e.group = eng::req<std::array<int, 2>>(j, "group", ctx);
    e.staggerTime = eng::req<float>(j, "stagger_time", ctx);
    e.knockbackResist = eng::req<float>(j, "knockback_resist", ctx);
    return e;
}

Vector3 vec3(const eng::json& j, const char* key, const std::string& ctx) {
    const auto a = eng::req<std::array<float, 3>>(j, key, ctx);
    return Vector3{a[0], a[1], a[2]};
}

FloorTheme parseFloor(const eng::json& j, const std::string& ctx) {
    FloorTheme f;
    f.id = eng::req<std::string>(j, "id", ctx);
    f.name = eng::req<std::string>(j, "name", ctx);
    const std::string c = ctx + ":" + f.id;
    f.lighting.ambient = vec3(j, "ambient", c);
    f.lighting.sunDir = vec3(j, "sun_dir", c);
    f.lighting.sunColor = vec3(j, "sun_color", c);
    f.lighting.fogColor = vec3(j, "fog_color", c);
    f.lighting.fogStart = eng::req<float>(j, "fog_start", c);
    f.lighting.fogEnd = eng::req<float>(j, "fog_end", c);
    f.lighting.exposure = eng::opt(j, "exposure", 1.f);
    f.lighting.bloomStrength = eng::opt(j, "bloom", 0.6f);
    f.torchColor = vec3(j, "torch_color", c);
    f.torchRadius = eng::req<float>(j, "torch_radius", c);
    f.fillColor = vec3(j, "fill_color", c);
    f.banner = eng::opt<std::string>(j, "banner", "");
    f.rooms = eng::req<std::array<int, 2>>(j, "rooms", c);
    f.enemies = eng::req<std::vector<std::string>>(j, "enemies", c);
    f.eliteChance = eng::opt(j, "elite_chance", 0.f);
    f.waves = eng::req<int>(j, "waves", c);
    f.enemyHpMult = eng::req<float>(j, "enemy_hp_mult", c);
    f.enemyDamageMult = eng::req<float>(j, "enemy_damage_mult", c);
    return f;
}

}  // namespace

float ScalingDef::hpMult(int players) const {
    if (enemyHpMult.empty()) return 1.f;
    const int i = std::clamp(players, 1, static_cast<int>(enemyHpMult.size())) - 1;
    return enemyHpMult[static_cast<size_t>(i)];
}

float ScalingDef::damageMult(int players) const {
    if (enemyDamageMult.empty()) return 1.f;
    const int i = std::clamp(players, 1, static_cast<int>(enemyDamageMult.size())) - 1;
    return enemyDamageMult[static_cast<size_t>(i)];
}

int ScalingDef::spawnCount(int base, int players) const {
    const float mult = 1.f + spawnBonusPerPlayer * static_cast<float>(std::max(players, 1) - 1);
    return std::max(1, static_cast<int>(std::lround(static_cast<float>(base) * mult)));
}

void GameData::loadAll() {
    loadedFiles.clear();
    auto load = [&](const std::string& rel) {
        loadedFiles.push_back(rel);
        return eng::loadJsonFile(rel);
    };
    config = load("data/config.json");

    classes.clear();
    classOrder.clear();
    for (const auto& f : listJson("data/classes")) {
        ClassDef c = parseClass(load(f), f);
        classes[c.id] = c;
    }
    for (const char* id : {"knight", "barbarian", "mage", "rogue"}) {
        if (!classes.count(id)) throw std::runtime_error(std::string("data/classes: missing class ") + id);
        classOrder.emplace_back(id);
    }
    enemies.clear();
    for (const auto& f : listJson("data/enemies")) {
        EnemyDef e = parseEnemy(load(f), f);
        enemies[e.id] = e;
    }
    for (const char* id : {"minion", "warrior", "rogue", "mage", "bone_king"})
        if (!enemies.count(id)) throw std::runtime_error(std::string("data/enemies: missing enemy ") + id);
    animMaps.clear();
    for (const auto& f : listJson("data/anims")) animMaps[f] = parseAnimMap(load(f), f);
    for (const auto& [id, c] : classes)
        if (!animMaps.count(c.animMap)) throw std::runtime_error("class " + id + ": unknown anim map " + c.animMap);
    for (const auto& [id, e] : enemies)
        if (!animMaps.count(e.animMap)) throw std::runtime_error("enemy " + id + ": unknown anim map " + e.animMap);

    const eng::json sc = load("data/scaling/players.json");
    scaling.enemyHpMult = eng::req<std::vector<float>>(sc, "enemy_hp_mult", "data/scaling/players.json");
    scaling.spawnBonusPerPlayer = eng::req<float>(sc, "spawn_count_bonus_per_extra_player", "data/scaling/players.json");
    scaling.enemyDamageMult = eng::opt(sc, "enemy_damage_mult", std::vector<float>{1.f, 1.f, 1.f, 1.f});
    if (scaling.enemyHpMult.size() != 4) throw std::runtime_error("data/scaling/players.json: need 4 hp multipliers");

    floors.clear();
    const eng::json fl = load("data/floors.json");
    for (const auto& f : fl.at("floors")) floors.push_back(parseFloor(f, "data/floors.json"));
    if (floors.size() != 3) throw std::runtime_error("data/floors.json: expected 3 floors");
    for (const auto& f : floors)
        for (const auto& e : f.enemies)
            if (!enemies.count(e)) throw std::runtime_error("data/floors.json: unknown enemy " + e);

    rooms.clear();
    const auto legend = parseLegend(load("data/rooms/_legend.json").at("legend"), "data/rooms/_legend.json");
    for (const auto& f : listJson("data/rooms")) {
        if (f.find("/_") != std::string::npos) continue;
        RoomTemplate r = parseRoomTemplate(load(f), legend, f);
        for (const auto& s : r.spawns)
            if (s.kind != "random" && !enemies.count(s.kind)) throw std::runtime_error(f + ": unknown enemy spawn " + s.kind);
        rooms[r.id] = r;
    }

    relics.clear();
    for (const auto& f : listJson("data/relics")) {
        RelicDef r = parseRelic(load(f), f);
        relics[r.id] = r;
    }
}

bool GameData::reload(std::string& err) {
    GameData fresh;
    try {
        fresh.loadAll();
    } catch (const std::exception& e) {
        err = e.what();
        return false;
    }
    config = fresh.config;
    for (auto& [k, v] : fresh.classes) classes[k] = v;
    for (auto& [k, v] : fresh.enemies) enemies[k] = v;
    for (auto& [k, v] : fresh.animMaps) animMaps[k] = v;
    scaling = fresh.scaling;
    floors = fresh.floors;
    for (auto& [k, v] : fresh.rooms) rooms[k] = v;
    for (auto& [k, v] : fresh.relics) relics[k] = v;
    return true;
}

float GameData::cfg(const char* section, const char* key) const {
    return config.at(section).at(key).get<float>();
}

const ClassDef& GameData::cls(const std::string& id) const { return classes.at(id); }
const EnemyDef& GameData::enemy(const std::string& id) const { return enemies.at(id); }
const AnimMapDef& GameData::animMap(const std::string& path) const { return animMaps.at(path); }
const RoomTemplate& GameData::room(const std::string& id) const { return rooms.at(id); }

}  // namespace game
