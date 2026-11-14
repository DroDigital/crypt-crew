#include "game/enemies/enemy_system.h"

#include <algorithm>
#include <cmath>
#include <raymath.h>

#include "game/world/combat.h"

namespace game {
namespace {

Vector3 flatTo(Vector3 a, Vector3 b) {
    Vector3 d{b.x - a.x, 0.f, b.z - a.z};
    const float l = Vector3Length(d);
    return l > 1e-4f ? Vector3Scale(d, 1.f / l) : Vector3{0, 0, 0};
}

float dist2D(Vector3 a, Vector3 b) { return Vector2Distance(Vector2{a.x, a.z}, Vector2{b.x, b.z}); }

void toChase(World& w, Enemy& e) {
    w.tokens.releaseAll(e.id);
    e.state = EState::Chase;
    e.stateTime = 0.f;
    e.attackCd = e.def->attackCooldown * w.rng.uniform(0.8f, 1.2f);
}

void playLocomotion(Enemy& e, float speed) {
    if (speed > e.def->moveSpeed * 0.6f) e.vis.play("run", false, std::max(0.6f, speed / e.def->moveSpeed));
    else if (speed > 0.3f) e.vis.play("walk", false, 1.f);
    else e.vis.play("idle");
}

void startWindup(World& w, Enemy& e) {
    e.state = EState::Windup;
    e.stateTime = 0.f;
    e.hitFired = false;
    e.vis.play(e.def->attack.anim, true, e.speedMul, 0.08f);
    w.sound("telegraph", 0.8f);
}

// Melee attack: wind-up clip (>= 0.4 s before the hit frame), tracking the target early.
void updateWindup(World& w, Enemy& e, Player* t, bool hitEvent, float dt) {
    const ResolvedAction* a = e.vis.action(e.def->attack.anim);
    const float dur = a ? a->realDuration() / e.speedMul : 1.f;
    const float hitT = a ? a->realHit() / e.speedMul : 0.5f;
    const float turn = e.def->raw.value("turn_speed_deg", 360.f) * DEG2RAD;
    Vector3 vel{0, 0, 0};
    if (t && e.stateTime < hitT * 0.7f) {
        faceToward(e, flatTo(e.k.pos, t->k.pos), turn, dt);
        // Step in during the wind-up so the swing reaches a target that backs off slowly.
        if (dist2D(e.k.pos, t->k.pos) > e.def->attack.range * 0.7f)
            vel = Vector3Scale(e.k.forward(), e.def->moveSpeed * 0.35f);
    }
    if (hitEvent && !e.hitFired) {
        e.hitFired = true;
        enemyMeleeHit(w, e);
    }
    moveEnemy(w, e, vel, dt);
    if (e.stateTime >= dur) toChase(w, e);
}

void updateMelee(World& w, Enemy& e, Player* t, bool hitEvent, float dt) {
    switch (e.state) {
        case EState::Chase: {
            if (!t) {
                moveEnemy(w, e, Vector3{0, 0, 0}, dt);
                playLocomotion(e, 0.f);
                break;
            }
            const float d = dist2D(e.k.pos, t->k.pos);
            const bool inRange = d <= e.def->triggerRange;
            if (inRange && e.attackCd <= 0.f && w.tokens.acquire(t->id, e.id)) {
                startWindup(w, e);
                break;
            }
            Vector3 vel{0, 0, 0};
            const bool hasToken = w.tokens.holds(t->id, e.id);
            // Without a token, hang back at a ring around the target.
            const float holdDist = hasToken || e.attackCd > 0.f ? e.def->triggerRange * 0.8f : e.def->triggerRange + 1.8f;
            if (d > holdDist) {
                vel = Vector3Scale(steerTo(w, e, t->k.pos, dt), e.def->moveSpeed * e.speedMul);
            } else if (!hasToken) {
                // Shuffle sideways while waiting.
                const Vector3 to = flatTo(e.k.pos, t->k.pos);
                vel = Vector3Scale(Vector3{-to.z * e.circleSign, 0, to.x * e.circleSign}, e.def->moveSpeed * 0.3f);
            }
            const float turn = e.def->raw.value("turn_speed_deg", 540.f) * DEG2RAD;
            faceToward(e, Vector3Length(vel) > 0.5f && d > 4.f ? Vector3Normalize(vel) : flatTo(e.k.pos, t->k.pos), turn, dt);
            moveEnemy(w, e, vel, dt);
            if (e.def->behavior == "shield" && d < 5.f && Vector3Length(vel) < 1.f) e.vis.play("guard");
            else playLocomotion(e, Vector3Length(vel));
            break;
        }
        case EState::Windup: updateWindup(w, e, t, hitEvent, dt); break;
        default: break;
    }
}

void updateLunger(World& w, Enemy& e, Player* t, bool hitEvent, float dt) {
    const float circleR = e.def->raw.value("circle_radius", 6.f);
    switch (e.state) {
        case EState::Chase: {
            if (!t) {
                moveEnemy(w, e, Vector3{0, 0, 0}, dt);
                playLocomotion(e, 0.f);
                break;
            }
            if (dist2D(e.k.pos, t->k.pos) <= circleR + 1.5f) {
                e.state = EState::Circle;
                e.stateTime = 0.f;
                const auto ct = e.def->raw.value("circle_time", std::array<float, 2>{1.5f, 3.f});
                e.circleTime = w.rng.uniform(ct[0], ct[1]);
                break;
            }
            const Vector3 vel = Vector3Scale(steerTo(w, e, t->k.pos, dt), e.def->moveSpeed * e.speedMul);
            faceToward(e, vel, 10.f, dt);
            moveEnemy(w, e, vel, dt);
            playLocomotion(e, Vector3Length(vel));
            break;
        }
        case EState::Circle: {
            if (!t) {
                toChase(w, e);
                break;
            }
            const Vector3 to = flatTo(e.k.pos, t->k.pos);
            const float d = dist2D(e.k.pos, t->k.pos);
            const Vector3 tangent{-to.z * e.circleSign, 0, to.x * e.circleSign};
            const Vector3 radial = Vector3Scale(to, std::clamp((d - circleR) * 0.8f, -1.f, 1.f));
            Vector3 dir = Vector3Add(tangent, radial);
            const Vector3 sep = Vector3Scale(steerTo(w, e, Vector3Add(e.k.pos, dir), dt), 0.3f);
            dir = Vector3Normalize(Vector3Add(dir, sep));
            // Reverse direction when about to run into a wall.
            if (!w.room.walkable(Vector3Add(e.k.pos, Vector3Scale(dir, 1.5f)))) e.circleSign = -e.circleSign;
            faceToward(e, to, 8.f, dt);
            moveEnemy(w, e, Vector3Scale(dir, e.def->moveSpeed * 0.75f * e.speedMul), dt);
            e.vis.play("run", false, 0.8f);
            if (e.stateTime > e.circleTime && e.attackCd <= 0.f && d < circleR + 2.5f && w.tokens.acquire(t->id, e.id)) {
                e.state = EState::Telegraph;
                e.stateTime = 0.f;
                e.lungeDir = to;
                e.vis.play("telegraph", true, 1.f, 0.06f);
                w.sound("telegraph");
            }
            break;
        }
        case EState::Telegraph: {
            // >= 0.5 s crouch with a red line on the floor, tracking the target until the last 0.15 s.
            const float tele = e.def->raw.value("telegraph_time", 0.5f);
            if (t && e.stateTime < tele - 0.15f) e.lungeDir = flatTo(e.k.pos, t->k.pos);
            faceToward(e, e.lungeDir, 20.f, dt);
            moveEnemy(w, e, Vector3{0, 0, 0}, dt);
            if (e.stateTime >= tele) {
                e.state = EState::Lunge;
                e.stateTime = 0.f;
                e.lungeHits.clear();
                e.vis.play(e.def->attack.anim, true, 1.f, 0.04f);
                w.sound("swing");
            }
            break;
        }
        case EState::Lunge: {
            const float lt = e.def->raw.value("lunge_time", 0.35f);
            const float sp = e.def->raw.value("lunge_speed", 16.f);
            moveEnemy(w, e, Vector3Scale(e.lungeDir, sp * (1.f - 0.5f * e.stateTime / lt)), dt);
            eng::Hitbox hb;
            hb.shape = eng::HitShape::Sphere;
            hb.origin = Vector3{e.k.pos.x, 1.f, e.k.pos.z};
            hb.radius = 1.0f;
            for (Player& p : w.players) {
                if (!p.canAct() || e.lungeHits.contains(p.id) || !eng::overlaps(hb, w.hurtbox(p.k))) continue;
                e.lungeHits.add(p.id);
                DamageInfo d;
                d.amount = e.def->attack.damage;
                d.from = e.k.pos;
                d.knock = e.def->attack.knockback;
                d.fromPlayer = false;
                d.attacker = e.id;
                d.source = e.def->name;
                damagePlayer(w, p, d);
            }
            if (e.stateTime >= lt) {
                e.state = EState::Recover;
                e.stateTime = 0.f;
            }
            break;
        }
        case EState::Recover:
            moveEnemy(w, e, Vector3{0, 0, 0}, dt);
            if (e.stateTime > 0.6f) {
                w.tokens.releaseAll(e.id);
                e.attackCd = e.def->attackCooldown * w.rng.uniform(0.8f, 1.2f);
                e.state = EState::Circle;
                e.stateTime = 0.f;
                e.circleSign = w.rng.chance(0.5f) ? 1.f : -1.f;
                e.circleTime = w.rng.uniform(1.f, 2.f);
            }
            break;
        default: break;
    }
    (void)hitEvent;
}

void updateCaster(World& w, Enemy& e, Player* t, bool hitEvent, float dt) {
    const float keep = e.def->raw.value("keep_distance", 8.f);
    switch (e.state) {
        case EState::Chase: {
            if (!t) {
                moveEnemy(w, e, Vector3{0, 0, 0}, dt);
                playLocomotion(e, 0.f);
                break;
            }
            const float d = dist2D(e.k.pos, t->k.pos);
            const Vector3 to = flatTo(e.k.pos, t->k.pos);
            Vector3 vel{0, 0, 0};
            if (d > keep + 2.f) vel = Vector3Scale(steerTo(w, e, t->k.pos, dt), e.def->moveSpeed);
            else if (d < keep - 1.5f) {
                const Vector3 away = Vector3Add(e.k.pos, Vector3Scale(to, -3.f));
                if (w.room.walkable(away)) vel = Vector3Scale(steerTo(w, e, away, dt), e.def->moveSpeed);
                else vel = Vector3Scale(Vector3{-to.z * e.circleSign, 0, to.x * e.circleSign}, e.def->moveSpeed);
            } else {
                vel = Vector3Scale(Vector3{-to.z * e.circleSign, 0, to.x * e.circleSign}, e.def->moveSpeed * 0.4f);
                if (!w.room.walkable(Vector3Add(e.k.pos, Vector3Scale(vel, 0.6f)))) e.circleSign = -e.circleSign;
            }
            faceToward(e, to, 8.f, dt);
            moveEnemy(w, e, vel, dt);
            playLocomotion(e, Vector3Length(vel));
            if (e.attackCd <= 0.f && d < e.def->triggerRange && w.room.nav.lineClear(e.k.pos, t->k.pos) &&
                w.tokens.acquire(t->id, e.id)) {
                e.state = EState::Cast;
                e.stateTime = 0.f;
                e.hitFired = false;
                e.vis.play(e.def->attack.anim, true, 1.f, 0.08f);
                w.sound("telegraph", 0.7f, 0.7f);
            }
            break;
        }
        case EState::Cast: {
            const ResolvedAction* a = e.vis.action(e.def->attack.anim);
            if (t) faceToward(e, flatTo(e.k.pos, t->k.pos), 6.f, dt);
            moveEnemy(w, e, Vector3{0, 0, 0}, dt);
            if (hitEvent && !e.hitFired) {
                e.hitFired = true;
                const auto& b = e.def->raw.at("bolt");
                Projectile pr;
                pr.kind = ProjKind::EnemyBolt;
                pr.owner = e.id;
                pr.fromPlayer = false;
                const Vector3 f = e.k.forward();
                pr.pos = pr.prev = Vector3Add(Vector3{e.k.pos.x, 1.4f, e.k.pos.z}, Vector3Scale(f, 0.8f));
                pr.vel = Vector3Scale(f, b.value("speed", 6.5f));
                pr.turnRate = b.value("turn_rate_deg", 70.f) * DEG2RAD;
                pr.life = b.value("lifetime", 4.5f);
                pr.radius = b.value("radius", 0.45f);
                pr.damage = e.def->attack.damage;
                pr.knock = e.def->attack.knockback;
                pr.target = t ? t->id : 0;
                w.projectiles.push_back(pr);
                w.sound("bolt");
            }
            if (e.stateTime >= (a ? a->realDuration() : 1.2f)) toChase(w, e);
            break;
        }
        default: break;
    }
}

}  // namespace

void faceToward(Enemy& e, Vector3 dir, float maxRad, float dt) {
    if (Vector3Length(dir) < 1e-3f) return;
    const float want = std::atan2(dir.x, dir.z);
    float d = want - e.k.yaw;
    while (d > PI) d -= 2.f * PI;
    while (d < -PI) d += 2.f * PI;
    e.k.yaw += std::clamp(d, -maxRad * dt, maxRad * dt);
}

Player* pickTarget(World& w, Enemy& e, float dt) {
    e.retarget -= dt;
    Player* cur = w.player(e.target);
    if (cur && cur->canAct() && e.retarget > 0.f) return cur;
    Player* best = nullptr;
    float bestD = 1e9f;
    for (Player& p : w.players) {
        if (!p.canAct()) continue;
        float d = dist2D(p.k.pos, e.k.pos);
        // Prefer players not already swarmed (attack tokens) so groups spread out.
        d += static_cast<float>(w.tokens.count(p.id)) * 1.5f;
        if (d < bestD) {
            bestD = d;
            best = &p;
        }
    }
    if (best && best->id != e.target) w.tokens.releaseAll(e.id);
    e.target = best ? best->id : 0;
    e.retarget = 0.5f + w.rng.uniform(0.f, 0.3f);
    return best;
}

Vector3 steerTo(World& w, Enemy& e, Vector3 goal, float dt) {
    e.repath -= dt;
    if (e.repath <= 0.f || e.path.empty()) {
        // Re-plan a few times per second (staggered per enemy).
        w.room.nav.findPath(e.k.pos, goal, e.path, 3000);
        e.repath = 0.25f + w.rng.uniform(0.f, 0.1f);
    }
    while (!e.path.empty() && dist2D(e.k.pos, e.path.front()) < 0.7f) e.path.erase(e.path.begin());
    Vector3 dir = e.path.empty() ? flatTo(e.k.pos, goal) : flatTo(e.k.pos, e.path.front());
    // Separation so swarms don't stack.
    std::vector<Vector3> near;
    for (const Enemy& o : w.enemies)
        if (o.id != e.id && o.state != EState::Dead && dist2D(o.k.pos, e.k.pos) < 2.2f) near.push_back(o.k.pos);
    const Vector3 sep = eng::separation(e.k.pos, near, 2.2f);
    dir = Vector3Add(dir, Vector3Scale(sep, 1.2f));
    const float l = Vector3Length(dir);
    return l > 1e-4f ? Vector3Scale(dir, 1.f / l) : Vector3{0, 0, 0};
}

void moveEnemy(World& w, Enemy& e, Vector3 vel, float dt) {
    updateKnockback(e.k, w.cfg("combat", "knockback_decay"), dt);
    e.k.pos = w.svc.phys.moveCharacter(e.k.phys, Vector3Add(vel, e.k.knock), dt);
}

void enemyMeleeHit(World& w, Enemy& e, float damageMult, float rangeMult) {
    ActiveHit h;
    h.owner = e.id;
    h.fromPlayer = false;
    h.box.shape = eng::HitShape::Sector;
    h.box.origin = e.k.pos;
    h.box.forward = e.k.forward();
    h.box.radius = e.def->attack.range * rangeMult * e.vis.scale;
    h.box.arcDeg = e.def->attack.arcDeg;
    h.box.height = e.k.height + 0.5f;
    h.damage = e.def->attack.damage * damageMult;
    h.knock = e.def->attack.knockback;
    h.knockFrom = e.k.pos;
    h.steps = 2;
    h.source = e.def->name;
    spawnHit(w, h);
    w.sound("swing", 0.8f, 0.85f);
}

float telegraphGlow(const Enemy& e) {
    switch (e.state) {
        case EState::Windup:
        case EState::Cast:
            if (e.hitFired) return 0.f;
            return std::min(1.f, e.stateTime / 0.4f) * (0.6f + 0.4f * std::sin(e.stateTime * 30.f));
        case EState::Telegraph:
        case EState::BossSlamWindup:
        case EState::BossChargeWindup: return 0.7f + 0.3f * std::sin(e.stateTime * 30.f);
        default: return 0.f;
    }
}

void updateEnemies(World& w, float dt) {
    for (Enemy& e : w.enemies) {
        e.k.prev = e.k.pos;
        e.k.prevYaw = e.k.yaw;
        e.stateTime += dt;
        e.hp.flash = std::max(0.f, e.hp.flash - dt);
        e.attackCd = std::max(0.f, e.attackCd - dt);
        int events[eng::Animator::kMaxEventsPerUpdate];
        const int fired = e.vis.update(dt * (e.state == EState::Stagger ? 1.f : e.speedMul), events);
        bool hitEvent = false;
        for (int i = 0; i < fired; ++i) hitEvent |= events[i] == kEventHit;

        if (e.state == EState::Dead) {
            e.deadTime += dt;
            continue;
        }
        if (e.isBoss) {
            updateBoss(w, e, dt);
            continue;
        }
        if (e.state == EState::Spawn) {
            const ResolvedAction* a = e.vis.action("spawn");
            moveEnemy(w, e, Vector3{0, 0, 0}, dt);
            if (e.stateTime >= (a ? a->realDuration() : 0.8f)) toChase(w, e), e.attackCd = w.rng.uniform(0.2f, 1.f);
            continue;
        }
        if (e.state == EState::Stagger) {
            moveEnemy(w, e, Vector3{0, 0, 0}, dt);
            if (e.stateTime >= std::max(e.def->staggerTime, 0.15f)) toChase(w, e), e.attackCd *= 0.5f;
            continue;
        }
        Player* t = pickTarget(w, e, dt);
        const std::string& b = e.def->behavior;
        if (b == "lunger") updateLunger(w, e, t, hitEvent, dt);
        else if (b == "caster") updateCaster(w, e, t, hitEvent, dt);
        else updateMelee(w, e, t, hitEvent, dt);
        // Rescue enemies knocked outside the room bounds (e.g. through an open door).
        const Vector3 mn = w.room.boundsMin, mx = w.room.boundsMax;
        if (e.k.pos.x < mn.x + 0.5f || e.k.pos.x > mx.x - 0.5f || e.k.pos.z < mn.z + 0.5f || e.k.pos.z > mx.z - 0.5f) {
            const Vector3 n = w.room.nav.nearestWalkable(e.k.pos);
            e.k.pos = Vector3{n.x, e.k.pos.y, n.z};
            w.svc.phys.setCharacterPosition(e.k.phys, e.k.pos);
        }
    }
}

}  // namespace game
