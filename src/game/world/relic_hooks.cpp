#include "game/world/relic_hooks.h"

#include <algorithm>
#include <map>
#include <raylib.h>
#include <raymath.h>

#include "game/world/combat.h"
#include "game/world/world.h"

namespace game::hooks {
namespace {

struct Hook {
    void (*onHit)(World&, Player&, Enemy&, float, bool) = nullptr;
    void (*onKill)(World&, Player&, Enemy&) = nullptr;
    void (*onDodge)(World&, Player&) = nullptr;
    void (*onRevive)(World&, Player&, Player&) = nullptr;
    void (*onRoomClear)(World&, Player&) = nullptr;
};

// Every hook id a relic may reference. Pure queries (checked elsewhere via has()) have no callbacks.
const std::map<std::string, Hook>& table() {
    static const std::map<std::string, Hook> t = {
        {"fire_trail", Hook{nullptr, nullptr,
                            [](World& w, Player& p) { p.fireTrailTimer = w.cfg("dodge", "duration"); }, nullptr, nullptr}},
        {"mage_double_bolt", Hook{}},
        {"backstab_crit", Hook{}},
        {"revive_shield", Hook{nullptr, nullptr, nullptr,
                               [](World& w, Player& reviver, Player&) {
                                   reviver.shield = std::max(reviver.shield, 30.f);
                                   w.text(Vector3Add(reviver.k.pos, Vector3{0, 3.f, 0}), "SHIELD", Color{150, 200, 255, 255}, 24.f);
                               },
                               nullptr}},
        {"bomb_on_death", Hook{nullptr,
                               [](World& w, Player& p, Enemy& e) {
                                   Projectile b;
                                   b.kind = ProjKind::Bomb;
                                   b.owner = p.id;
                                   b.fromPlayer = true;
                                   b.pos = b.prev = Vector3{e.k.pos.x, 0.4f, e.k.pos.z};
                                   b.life = 0.8f;
                                   b.damage = 25.f;
                                   b.radius = 3.0f * p.stats.aoeMult;
                                   w.projectiles.push_back(b);
                               },
                               nullptr, nullptr, nullptr}},
    };
    return t;
}

}  // namespace

bool exists(const std::string& id) { return table().count(id) > 0; }

std::vector<std::string> registeredIds() {
    std::vector<std::string> ids;
    for (const auto& [k, v] : table()) ids.push_back(k);
    return ids;
}

bool has(const Player& p, const std::string& id) {
    return std::find(p.relicHooks.begin(), p.relicHooks.end(), id) != p.relicHooks.end();
}

void onHit(World& w, Player& p, Enemy& e, float damage, bool crit) {
    for (const auto& id : p.relicHooks)
        if (auto it = table().find(id); it != table().end() && it->second.onHit) it->second.onHit(w, p, e, damage, crit);
}

void onKill(World& w, Player& p, Enemy& e) {
    if (p.stats.killHeal > 0.f) healPlayer(w, p, p.stats.killHeal);
    for (const auto& id : p.relicHooks)
        if (auto it = table().find(id); it != table().end() && it->second.onKill) it->second.onKill(w, p, e);
}

void onDodge(World& w, Player& p) {
    for (const auto& id : p.relicHooks)
        if (auto it = table().find(id); it != table().end() && it->second.onDodge) it->second.onDodge(w, p);
}

void onRevive(World& w, Player& reviver, Player& revived) {
    for (const auto& id : reviver.relicHooks)
        if (auto it = table().find(id); it != table().end() && it->second.onRevive) it->second.onRevive(w, reviver, revived);
}

void onRoomClear(World& w) {
    for (Player& p : w.players)
        for (const auto& id : p.relicHooks)
            if (auto it = table().find(id); it != table().end() && it->second.onRoomClear) it->second.onRoomClear(w, p);
}

}  // namespace game::hooks
