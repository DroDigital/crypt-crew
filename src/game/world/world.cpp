#include "game/world/world.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <raymath.h>

#include "engine/core/log.h"

namespace game {

World::World(const WorldServices& s) : svc(s) {
    tokens.setMax(static_cast<int>(s.data.cfg("attack_tokens", "max_per_player")));
    const auto& c = s.data.config.at("camera");
    CameraSettings cs;
    cs.pitchDeg = c.at("pitch_deg").get<float>();
    cs.yawDeg = c.at("yaw_deg").get<float>();
    cs.fovy = c.at("fovy").get<float>();
    cs.minDistance = c.at("min_distance").get<float>();
    cs.maxDistance = c.at("max_distance").get<float>();
    cs.fitMargin = c.at("fit_margin").get<float>();
    cs.followRate = c.at("follow_rate").get<float>();
    cs.zoomRate = c.at("zoom_rate").get<float>();
    cs.leashMargin = c.at("leash_margin").get<float>();
    cs.shakeDecay = s.data.cfg("shake", "decay");
    cs.shakeMaxOffset = s.data.cfg("shake", "max_offset");
    camera.configure(cs);
}

World::~World() {
    svc.phys.clearCharacters();
    svc.phys.clearStatic();
}

Player* World::player(EntityId id) {
    for (auto& p : players)
        if (p.id == id) return &p;
    return nullptr;
}

Enemy* World::enemy(EntityId id) {
    for (auto& e : enemies)
        if (e.id == id) return &e;
    return nullptr;
}

Player& World::addPlayer(int slot, const std::string& classId, Vector3 pos, bool dummy) {
    Player p;
    p.id = nextId();
    p.slot = slot;
    p.dummy = dummy;
    p.cls = &svc.data.cls(classId);
    p.vis.initHero(svc.chars, *p.cls);
    p.k.radius = p.cls->radius;
    p.k.height = p.cls->height;
    p.k.pos = p.k.prev = pos;
    p.k.phys = svc.phys.createCharacter(pos, p.k.radius, p.k.height);
    p.hp.max = p.hp.hp = p.cls->hp;
    players.push_back(std::move(p));
    return players.back();
}

void World::placePlayer(Player& p, Vector3 pos) {
    p.k.pos = p.k.prev = pos;
    if (p.k.phys < 0) p.k.phys = svc.phys.createCharacter(pos, p.k.radius, p.k.height);
    svc.phys.setCharacterPosition(p.k.phys, pos);
    p.k.knock = Vector3{0, 0, 0};
}

Enemy& World::spawnEnemy(const std::string& enemyId, Vector3 pos, bool boss) {
    Enemy e;
    e.id = nextId();
    e.def = &svc.data.enemy(enemyId);
    e.vis.initEnemy(svc.chars, *e.def);
    e.isBoss = boss || e.def->behavior == "boss";
    e.k.radius = e.def->radius;
    e.k.height = e.def->height;
    e.k.pos = e.k.prev = pos;
    e.k.yaw = e.k.prevYaw = rng.uniform(-PI, PI);
    e.k.phys = svc.phys.createCharacter(pos, e.k.radius, e.k.height);
    const float mult = svc.data.scaling.hpMult(playerCount());
    e.hp.max = e.hp.hp = e.def->hp * mult;
    e.circleSign = rng.chance(0.5f) ? 1.f : -1.f;
    e.attackCd = rng.uniform(0.3f, 1.2f);
    e.repath = rng.uniform(0.f, 0.25f);
    e.vis.play("spawn", true, 1.f, 0.f);
    enemies.push_back(std::move(e));
    return enemies.back();
}

void World::removeDeadEnemies() {
    for (auto& e : enemies)
        if (e.state == EState::Dead && e.deadTime > 3.0f && e.k.phys >= 0) {
            svc.phys.removeCharacter(e.k.phys);
            e.k.phys = -1;
        }
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
                                 [](const Enemy& e) { return e.state == EState::Dead && e.deadTime > 3.0f; }),
                  enemies.end());
}

void World::flushSpawns() {
    for (const auto& p : pendingSpawns_) spawnEnemy(p.id, p.pos);
    pendingSpawns_.clear();
}

void World::clearEnemies() {
    pendingSpawns_.clear();
    for (auto& e : enemies)
        if (e.k.phys >= 0) svc.phys.removeCharacter(e.k.phys);
    enemies.clear();
    projectiles.clear();
    hits.clear();
    hazards.clear();
    chests.clear();
    tokens.clear();
}

void World::spawnGold(Vector3 pos, int total) {
    // Split into a few coins that pop out.
    int left = total;
    const int coins = std::clamp(total / 3 + 1, 1, 8);
    for (int i = 0; i < coins && left > 0; ++i) {
        const int amt = (i == coins - 1) ? left : std::max(1, total / coins);
        left -= amt;
        GoldPickup g;
        g.pos = Vector3{pos.x, 0.8f, pos.z};
        const float a = rng.uniform(0.f, 2.f * PI);
        const float s = rng.uniform(2.f, 5.f);
        g.vel = Vector3{std::cos(a) * s, rng.uniform(4.f, 7.f), std::sin(a) * s};
        g.amount = amt;
        gold.push_back(g);
    }
}

void World::text(Vector3 pos, const char* s, Color c, float size) {
    FloatText t;
    t.pos = pos;
    std::snprintf(t.text, sizeof t.text, "%s", s);
    t.color = c;
    t.maxLife = t.life = cfg("damage_numbers", "lifetime");
    t.size = size;
    texts.push_back(t);
}

void World::emit(const std::string& fxId, Vector3 pos, Vector3 dir, float countScale) {
    if (const eng::EmitterDef* e = svc.fx.find(fxId)) svc.particles.emit(*e, pos, dir, rng, countScale);
}

void World::sound(const std::string& id, float volume, float pitch) { svc.audio.play(id, volume, pitch); }

void World::hitstop(float seconds) { svc.clock.hitstop(seconds); }

void World::shake(float trauma) {
    if (screenShake) camera.addTrauma(trauma);
}

int World::standingPlayers() const {
    int n = 0;
    for (const auto& p : players) n += p.canAct() ? 1 : 0;
    return n;
}

bool World::allPlayersDown() const { return !players.empty() && standingPlayers() == 0; }

int World::livingEnemies() const {
    int n = 0;
    for (const auto& e : enemies) n += (e.state != EState::Dead) ? 1 : 0;
    return n;
}

}  // namespace game
