// The Bone King: 3-swing combo, ground slam with a shockwave ring (roll through it), charge
// across the room; at 50% HP he summons minions and swings faster. Tunables: data/enemies/bone_king.json.
#include <algorithm>
#include <cmath>
#include <raylib.h>
#include <raymath.h>

#include "game/enemies/enemy_system.h"
#include "game/world/combat.h"

namespace game {
namespace {

float dist2D(Vector3 a, Vector3 b) { return Vector2Distance(Vector2{a.x, a.z}, Vector2{b.x, b.z}); }
Vector3 flatTo(Vector3 a, Vector3 b) {
    Vector3 d{b.x - a.x, 0.f, b.z - a.z};
    const float l = Vector3Length(d);
    return l > 1e-4f ? Vector3Scale(d, 1.f / l) : Vector3{0, 0, 1};
}

void setState(Enemy& e, EState s) {
    e.state = s;
    e.stateTime = 0.f;
    e.hitFired = false;
}

void toChase(World& w, Enemy& e) {
    setState(e, EState::Chase);
    const auto mc = e.def->raw.value("move_choice_cooldown", std::array<float, 2>{1.f, 2.f});
    e.moveChoice = w.rng.uniform(mc[0], mc[1]) * (e.phase2 ? 0.7f : 1.f);
}

float comboSpeed(const Enemy& e) { return e.phase2 ? e.def->raw.value("combo_speed_phase2", 1.35f) : 1.f; }

void startCombo(Enemy& e, int index) {
    setState(e, EState::Windup);
    e.bossCombo = index;
    static const char* kSwings[3] = {"attack_1", "attack_2", "attack_3"};
    e.vis.play(kSwings[index % 3], true, comboSpeed(e), 0.08f);
}

}  // namespace

void updateBoss(World& w, Enemy& e, float dt) {
    const auto& raw = e.def->raw;
    Player* t = pickTarget(w, e, dt);
    const float hpFrac = e.hp.hp / e.hp.max;
    // The anim event was already consumed by updateEnemies; detect hit frames by time instead.
    auto hitNow = [&](const char* action, float speedMul) {
        const ResolvedAction* a = e.vis.action(action);
        if (!a || e.hitFired) return false;
        if (e.stateTime >= a->realHit() / speedMul) {
            e.hitFired = true;
            return true;
        }
        return false;
    };
    auto actionDone = [&](const char* action, float speedMul) {
        const ResolvedAction* a = e.vis.action(action);
        return !a || e.stateTime >= a->realDuration() / speedMul;
    };

    switch (e.state) {
        case EState::Spawn:
            moveEnemy(w, e, Vector3{0, 0, 0}, dt);
            if (actionDone("spawn", 1.f)) {
                setState(e, EState::BossRoar);
                e.vis.play("roar", true);
                w.sound("boss_roar");
                w.shake(0.5f);
            }
            break;
        case EState::BossRoar:
            moveEnemy(w, e, Vector3{0, 0, 0}, dt);
            if (actionDone("roar", 1.f)) toChase(w, e);
            break;
        case EState::Chase: {
            if (!e.phase2 && hpFrac <= raw.value("phase2_hp", 0.5f)) {
                e.phase2 = true;
                setState(e, EState::BossSummon);
                e.vis.play("summon", true);
                w.sound("boss_roar", 1.f, 0.85f);
                w.text(Vector3Add(e.k.pos, Vector3{0, 6.f, 0}), "RISE, MY SERVANTS!", Color{140, 255, 120, 255}, 34.f);
                break;
            }
            if (!t) {
                moveEnemy(w, e, Vector3{0, 0, 0}, dt);
                e.vis.play("idle");
                break;
            }
            const float d = dist2D(e.k.pos, t->k.pos);
            faceToward(e, flatTo(e.k.pos, t->k.pos), 3.f, dt);
            e.moveChoice -= dt;
            if (e.moveChoice <= 0.f) {
                const float r = w.rng.uniform();
                if (d < e.def->triggerRange + 1.f) {
                    if (r < 0.65f) startCombo(e, 0);
                    else {
                        setState(e, EState::BossSlamWindup);
                        e.vis.play("slam", true, 1.f, 0.1f);
                        w.sound("telegraph", 1.f, 0.6f);
                    }
                } else if (r < 0.55f) {
                    setState(e, EState::BossChargeWindup);
                    e.lungeDir = flatTo(e.k.pos, t->k.pos);
                    e.vis.play("roar", true, 1.4f);
                    w.sound("telegraph", 1.f, 0.5f);
                } else if (r < 0.8f) {
                    setState(e, EState::BossSlamWindup);
                    e.vis.play("slam", true, 1.f, 0.1f);
                    w.sound("telegraph", 1.f, 0.6f);
                } else {
                    e.moveChoice = 0.6f;  // keep walking a bit
                }
                break;
            }
            const Vector3 vel = d > e.def->triggerRange * 0.8f
                                    ? Vector3Scale(steerTo(w, e, t->k.pos, dt), e.def->moveSpeed * (e.phase2 ? 1.25f : 1.f))
                                    : Vector3{0, 0, 0};
            moveEnemy(w, e, vel, dt);
            e.vis.play(Vector3Length(vel) > 0.5f ? "walk" : "idle", false, 1.f);
            break;
        }
        case EState::Windup: {  // combo swing
            static const char* kSwings[3] = {"attack_1", "attack_2", "attack_3"};
            const char* sw = kSwings[e.bossCombo % 3];
            const float sp = comboSpeed(e);
            const ResolvedAction* a = e.vis.action(sw);
            Vector3 vel{0, 0, 0};
            if (t && a && e.stateTime < a->realHit() / sp * 0.6f) {
                faceToward(e, flatTo(e.k.pos, t->k.pos), 4.f, dt);
                if (dist2D(e.k.pos, t->k.pos) > 3.f) vel = Vector3Scale(e.k.forward(), 3.f);
            }
            moveEnemy(w, e, vel, dt);
            if (hitNow(sw, sp)) enemyMeleeHit(w, e, 1.f, 1.f / e.vis.scale);
            if (actionDone(sw, sp)) {
                if (e.bossCombo < 2) startCombo(e, e.bossCombo + 1);
                else setState(e, EState::Recover);
            }
            break;
        }
        case EState::BossSlamWindup: {
            moveEnemy(w, e, Vector3{0, 0, 0}, dt);
            if (hitNow("slam", 1.f)) {
                const auto& s = raw.at("slam");
                Hazard ring;
                ring.kind = Hazard::Kind::ShockRing;
                ring.pos = Vector3{e.k.pos.x, 0.f, e.k.pos.z};
                ring.radius = 1.5f;
                ring.speed = s.value("ring_speed", 9.f);
                ring.width = s.value("ring_width", 1.2f);
                ring.maxRadius = s.value("max_radius", 14.f);
                ring.life = 10.f;
                ring.damage = s.value("damage", 26.f);
                w.hazards.push_back(ring);
                ActiveHit h;  // the impact itself
                h.owner = e.id;
                h.fromPlayer = false;
                h.box.shape = eng::HitShape::Sector;
                h.box.arcDeg = 360.f;
                h.box.origin = e.k.pos;
                h.box.radius = 3.2f;
                h.damage = s.value("damage", 26.f);
                h.knock = s.value("knockback", 10.f);
                h.heavy = true;
                h.knockFrom = e.k.pos;
                h.steps = 2;
                h.source = "the Bone King's slam";
                spawnHit(w, h);
                w.emit("slam_ring", Vector3Add(e.k.pos, Vector3{0, 0.3f, 0}), Vector3{0, 0.1f, 0});
                w.sound("slam");
                w.shake(w.cfg("shake", "boss_slam"));
                w.hitstop(0.08f);
            }
            if (actionDone("slam", 1.f)) setState(e, EState::Recover);
            break;
        }
        case EState::BossChargeWindup: {
            const auto& c = raw.at("charge");
            const float tele = c.value("telegraph", 0.8f);
            if (t && e.stateTime < tele * 0.75f) e.lungeDir = flatTo(e.k.pos, t->k.pos);
            faceToward(e, e.lungeDir, 8.f, dt);
            moveEnemy(w, e, Vector3{0, 0, 0}, dt);
            if (e.stateTime >= tele) {
                setState(e, EState::BossCharge);
                e.lungeHits.clear();
                e.vis.play("charge", true);
                w.sound("boss_roar", 0.7f, 1.3f);
            }
            break;
        }
        case EState::BossCharge: {
            const auto& c = raw.at("charge");
            const float speed = c.value("speed", 15.f);
            const Vector3 before = e.k.pos;
            moveEnemy(w, e, Vector3Scale(e.lungeDir, speed), dt);
            const float moved = dist2D(before, e.k.pos);
            if (w.rng.chance(0.5f)) w.emit("dust", e.k.pos, Vector3{0, 1, 0});
            eng::Hitbox hb;
            hb.shape = eng::HitShape::Sphere;
            hb.origin = Vector3{e.k.pos.x, 1.5f, e.k.pos.z};
            hb.radius = c.value("width", 2.f);
            for (Player& p : w.players) {
                if (!p.canAct() || e.lungeHits.contains(p.id) || !eng::overlaps(hb, w.hurtbox(p.k))) continue;
                e.lungeHits.add(p.id);
                DamageInfo d;
                d.amount = c.value("damage", 24.f);
                d.from = e.k.pos;
                d.knock = c.value("knockback", 12.f);
                d.heavy = true;
                d.fromPlayer = false;
                d.attacker = e.id;
                d.source = "the Bone King's charge";
                damagePlayer(w, p, d);
            }
            // Stop at walls or after the max time.
            if (e.stateTime > c.value("max_time", 1.6f) || (e.stateTime > 0.2f && moved < speed * dt * 0.3f)) {
                if (moved < speed * dt * 0.3f) {
                    w.shake(0.4f);
                    w.sound("hit_heavy", 1.f, 0.6f);
                }
                setState(e, EState::Recover);
            }
            break;
        }
        case EState::BossSummon: {
            moveEnemy(w, e, Vector3{0, 0, 0}, dt);
            if (hitNow("summon", 1.f)) {
                const auto& s = raw.at("summon");
                const int n = s.value("count", 4);
                for (int i = 0; i < n; ++i) {
                    const float a = static_cast<float>(i) / static_cast<float>(n) * 2.f * PI;
                    Vector3 p{e.k.pos.x + std::cos(a) * 4.f, 0.f, e.k.pos.z + std::sin(a) * 4.f};
                    if (!w.room.walkable(p)) p = w.room.nav.nearestWalkable(p);
                    w.emit("summon", p, Vector3{0, 1, 0});
                    w.queueSpawn(s.value("enemy", std::string("minion")), p);
                }
            }
            if (actionDone("summon", 1.f)) toChase(w, e);
            break;
        }
        case EState::Recover:
            moveEnemy(w, e, Vector3{0, 0, 0}, dt);
            e.vis.play("idle");
            if (e.stateTime > (e.phase2 ? 0.45f : 0.7f)) toChase(w, e);
            break;
        default:
            toChase(w, e);
            break;
    }
}

}  // namespace game
