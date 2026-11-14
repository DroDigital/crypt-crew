#include "game/player/player_system.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <raymath.h>

#include "engine/input/input_types.h"
#include "game/world/combat.h"
#include "game/world/relic_hooks.h"

namespace game {
namespace {

Vector3 flat(Vector3 v) {
    v.y = 0.f;
    const float l = Vector3Length(v);
    return l > 1e-4f ? Vector3Scale(v, 1.f / l) : Vector3{0, 0, 0};
}

float yawOf(Vector3 d) { return std::atan2(d.x, d.z); }

float approachAngle(float from, float to, float maxStep) {
    float d = to - from;
    while (d > PI) d -= 2.f * PI;
    while (d < -PI) d += 2.f * PI;
    return from + std::clamp(d, -maxStep, maxStep);
}

const AttackStep& comboStep(const Player& p, int i) {
    return p.cls->combo[static_cast<size_t>(i) % p.cls->combo.size()];
}

// With the stick idle, aim snaps to the nearest enemy within a cone around the facing.
Vector3 autoAim(World& w, const Player& p, Vector3 facing) {
    const float cone = w.cfg("aim", "assist_cone_deg") * 0.5f * DEG2RAD;
    const float range = w.cfg("aim", "assist_range");
    Vector3 best = facing;
    float bestD = range;
    for (const Enemy& e : w.enemies) {
        if (e.state == EState::Dead || e.state == EState::Spawn) continue;
        Vector3 to{e.k.pos.x - p.k.pos.x, 0, e.k.pos.z - p.k.pos.z};
        const float d = Vector3Length(to);
        if (d < 1e-3f || d > bestD) continue;
        to = Vector3Scale(to, 1.f / d);
        if (std::acos(std::clamp(Vector3DotProduct(to, facing), -1.f, 1.f)) > cone && d > 2.5f) continue;
        bestD = d;
        best = to;
    }
    return best;
}

void startAttack(World& w, Player& p, int index) {
    const AttackStep& s = comboStep(p, index);
    p.state = PState::Attack;
    p.stateTime = 0.f;
    p.combo = index;
    p.comboQueued = false;
    p.hitFired = false;
    p.k.yaw = yawOf(p.aim);
    p.vis.play(s.anim, true, 1.f, 0.06f);
    const float decay = w.cfg("combat", "knockback_decay");
    p.k.knock = Vector3Add(p.k.knock, Vector3Scale(p.aim, s.lunge * decay));
    if (p.cls->basicType == "melee") w.sound(s.heavy ? "swing_heavy" : "swing");
}

void spawnBolt(World& w, Player& p, const AttackStep& s, float angleOffset) {
    Projectile pr;
    pr.kind = ProjKind::Bolt;
    pr.owner = p.id;
    pr.fromPlayer = true;
    const Vector3 dir = Vector3RotateByAxisAngle(p.aim, Vector3{0, 1, 0}, angleOffset);
    pr.pos = pr.prev = Vector3Add(Vector3{p.k.pos.x, 1.3f, p.k.pos.z}, Vector3Scale(dir, 0.9f));
    pr.vel = Vector3Scale(dir, s.speed);
    pr.radius = s.radius;
    pr.damage = s.damage;
    pr.knock = s.knockback;
    pr.life = s.range / std::max(s.speed, 0.1f);
    w.projectiles.push_back(pr);
}

void basicHit(World& w, Player& p) {
    const AttackStep& s = comboStep(p, p.combo);
    if (p.cls->basicType == "projectile") {
        spawnBolt(w, p, s, 0.f);
        if (hooks::has(p, "mage_double_bolt")) {
            spawnBolt(w, p, s, 0.14f);
            spawnBolt(w, p, s, -0.14f);
        }
        w.sound("magic");
        return;
    }
    ActiveHit h;
    h.owner = p.id;
    h.fromPlayer = true;
    h.box.shape = eng::HitShape::Sector;
    h.box.origin = p.k.pos;
    h.box.forward = p.aim;
    h.box.radius = s.range;
    h.box.arcDeg = s.arcDeg;
    h.box.height = 2.6f;
    h.damage = s.damage;
    h.knock = s.knockback;
    h.heavy = s.heavy;
    h.knockFrom = p.k.pos;
    h.steps = 2;
    spawnHit(w, h);
}

void startSpecial(World& w, Player& p) {
    const auto& sp = p.cls->special;
    p.state = PState::Special;
    p.stateTime = 0.f;
    p.hitFired = false;
    p.specialCd = p.cls->specialCooldown * p.stats.specialCdMult;
    p.k.yaw = yawOf(p.aim);
    p.vis.play("special", true, 1.f, 0.06f);
    const std::string& t = p.cls->specialType;
    if (t == "shield_bash") {
        const float decay = w.cfg("combat", "knockback_decay");
        p.k.knock = Vector3Add(p.k.knock, Vector3Scale(p.aim, sp.value("lunge", 3.f) * decay));
        w.sound("swing_heavy");
    } else if (t == "shadow_dash") {
        p.dashDir = p.aim;
        p.dashStart = p.k.pos;
        w.svc.phys.setCharacterCollides(p.k.phys, false);
        p.hp.iframes = std::max(p.hp.iframes, sp.value("duration", 0.25f));
        w.sound("dodge", 1.f, 0.7f);
    }
}

void specialHit(World& w, Player& p) {
    const auto& sp = p.cls->special;
    const std::string& t = p.cls->specialType;
    if (t == "shield_bash") {
        ActiveHit h;
        h.owner = p.id;
        h.box.shape = eng::HitShape::Sector;
        h.box.origin = p.k.pos;
        h.box.forward = p.aim;
        h.box.radius = sp.value("range", 3.f);
        h.box.arcDeg = sp.value("arc_deg", 100.f);
        h.damage = sp.value("damage", 20.f);
        h.knock = sp.value("knockback", 10.f);
        h.heavy = true;
        h.unblockable = true;
        h.knockFrom = p.k.pos;
        h.steps = 2;
        spawnHit(w, h);
        p.guard = sp.value("guard_time", 2.f);
        // Pull aggro: nearby enemies target the knight while the guard lasts.
        const float r = sp.value("taunt_radius", 12.f);
        for (Enemy& e : w.enemies)
            if (e.state != EState::Dead && Vector3Distance(e.k.pos, p.k.pos) < r) {
                if (e.target != p.id) w.tokens.releaseAll(e.id);
                e.target = p.id;
                e.retarget = p.guard;
            }
        w.text(Vector3Add(p.k.pos, Vector3{0, 3.f, 0}), "GUARD", Color{150, 200, 255, 255}, 26.f);
        w.emit("block_spark", Vector3Add(p.k.pos, Vector3Add(Vector3{0, 1.2f, 0}, p.aim)), p.aim, 2.f);
        w.shake(0.25f);
    } else if (t == "rage") {
        p.rage = sp.value("duration", 6.f);
        w.emit("rage", Vector3Add(p.k.pos, Vector3{0, 1.2f, 0}), Vector3{0, 1, 0}, 10.f);
        w.sound("boss_roar", 0.6f, 1.6f);
        w.text(Vector3Add(p.k.pos, Vector3{0, 3.f, 0}), "RAGE!", Color{255, 90, 60, 255}, 32.f);
        w.shake(0.2f);
    } else if (t == "fire_nova") {
        ActiveHit h;
        h.owner = p.id;
        h.box.shape = eng::HitShape::Sector;
        h.box.arcDeg = 360.f;
        h.box.origin = p.k.pos;
        h.box.radius = sp.value("radius", 5.f) * p.stats.aoeMult;
        h.damage = sp.value("damage", 40.f);
        h.knock = sp.value("knockback", 8.f);
        h.heavy = true;
        h.unblockable = true;
        h.knockFrom = p.k.pos;
        h.steps = 2;
        spawnHit(w, h);
        w.emit("fire_nova", Vector3Add(p.k.pos, Vector3{0, 0.6f, 0}), Vector3{0, 0.2f, 0}, p.stats.aoeMult);
        w.sound("fire");
        w.shake(0.35f);
    }
}

void endDash(World& w, Player& p, Vector3 start) {
    const auto& sp = p.cls->special;
    w.svc.phys.setCharacterCollides(p.k.phys, true);
    ActiveHit h;
    h.owner = p.id;
    h.box.shape = eng::HitShape::Capsule;
    h.box.origin = Vector3{start.x, 1.f, start.z};
    h.box.end = Vector3{p.k.pos.x, 1.f, p.k.pos.z};
    h.box.radius = 1.0f;
    h.damage = sp.value("damage", 12.f);
    h.knock = 3.f;
    h.unblockable = true;
    h.knockFrom = start;
    h.steps = 1;
    spawnHit(w, h);
    p.critWindow = sp.value("crit_window", 2.f);
    w.text(Vector3Add(p.k.pos, Vector3{0, 3.f, 0}), "CRIT READY", Color{255, 220, 60, 255}, 22.f);
}

}  // namespace

void applyPlayerStats(Player& p, const PlayerStats& stats) {
    const float oldMax = p.hp.max;
    p.stats = stats;
    p.hp.max = p.cls->hp + stats.maxHpBonus;
    if (p.hp.max > oldMax && p.hp.alive()) p.hp.hp += p.hp.max - oldMax;
    p.hp.hp = std::min(p.hp.hp, p.hp.max);
    p.dodgeCharges = std::min(p.dodgeCharges, 1 + stats.extraDodges);
}

void resetPlayerForRoom(Player& p) {
    p.k.knock = Vector3{0, 0, 0};
    p.rage = p.guard = 0.f;
    if (p.state == PState::Attack || p.state == PState::Special || p.state == PState::Dodge || p.state == PState::Hurt) {
        p.state = PState::Move;
        p.vis.play("idle");
    }
}

int currentReviveCost(const World& w) {
    return w.svc.state.reviveCost(static_cast<int>(w.svc.data.cfg("revive", "base_cost")),
                                  static_cast<int>(w.svc.data.cfg("revive", "cost_multiplier")));
}

void updatePlayers(World& w, const std::array<PlayerCommand, 4>& cmds, float dt, float aspect) {
    const float decay = w.cfg("combat", "knockback_decay");
    const float dodgeDur = w.cfg("dodge", "duration"), dodgeIfr = w.cfg("dodge", "iframes"),
                dodgeCd = w.cfg("dodge", "cooldown"), dodgeDist = w.cfg("dodge", "distance");
    for (Player& p : w.players) {
        const PlayerCommand& c = cmds[static_cast<size_t>(std::clamp(p.slot, 0, 3))];
        p.k.prev = p.k.pos;
        p.k.prevYaw = p.k.yaw;
        p.stateTime += dt;
        p.hp.flash = std::max(0.f, p.hp.flash - dt);
        p.hp.iframes = std::max(0.f, p.hp.iframes - dt);
        p.specialCd = std::max(0.f, p.specialCd - dt);
        p.rage = std::max(0.f, p.rage - dt);
        p.guard = std::max(0.f, p.guard - dt);
        p.critWindow = std::max(0.f, p.critWindow - dt);
        const int maxCharges = 1 + p.stats.extraDodges;
        if (p.dodgeCharges < maxCharges) {
            p.dodgeCd -= dt;
            if (p.dodgeCd <= 0.f) {
                ++p.dodgeCharges;
                p.dodgeCd = p.dodgeCharges < maxCharges ? dodgeCd : 0.f;
            }
        }

        // Input -> world directions (camera looks north, so screen-down is +z).
        Vector3 move{c.move.x, 0.f, c.move.y};
        const float moveLen = std::min(Vector3Length(move), 1.f);
        p.moveDir = moveLen > 0.01f ? Vector3Scale(flat(move), moveLen) : Vector3{0, 0, 0};
        const Vector3 facing = p.k.forward();
        if (c.aimExplicit && Vector3Length(c.aim) > 0.01f) p.aim = flat(c.aim);
        else p.aim = autoAim(w, p, moveLen > 0.1f ? flat(move) : facing);

        int events[eng::Animator::kMaxEventsPerUpdate];
        const int fired = p.vis.update(dt, events);
        bool hitEvent = false;
        for (int i = 0; i < fired; ++i) hitEvent |= events[i] == kEventHit;

        Vector3 vel{0, 0, 0};
        float speed = p.cls->moveSpeed * p.stats.moveMult *
                      (p.rage > 0.f ? p.cls->special.value("speed_mult", 1.f) : 1.f);
        const bool wantAttack = (c.pressed & eng::kBtnAttack) || (p.dummy && (c.held & eng::kBtnAttack));
        const bool wantSpecial = c.pressed & eng::kBtnSpecial;
        const bool wantDodge = c.pressed & eng::kBtnDodge;
        auto tryDodge = [&]() {
            if (!wantDodge || p.dodgeCharges <= 0) return false;
            --p.dodgeCharges;
            if (p.dodgeCd <= 0.f) p.dodgeCd = dodgeCd;
            p.state = PState::Dodge;
            p.stateTime = 0.f;
            p.dodgeDir = moveLen > 0.1f ? flat(move) : facing;
            p.k.yaw = yawOf(p.dodgeDir);
            p.hp.iframes = std::max(p.hp.iframes, dodgeIfr);
            p.vis.play("dodge", true, 1.f, 0.04f);
            w.svc.phys.setCharacterCollides(p.k.phys, false);
            w.emit("dust", p.k.pos, Vector3{0, 1, 0});
            w.sound("dodge");
            hooks::onDodge(w, p);
            return true;
        };

        switch (p.state) {
            case PState::Move: {
                vel = Vector3Scale(p.moveDir, speed);
                if (moveLen > 0.1f) p.vis.play("run", false, 0.65f + 0.45f * moveLen * (speed / p.cls->moveSpeed));
                else p.vis.play("idle");
                const float wantYaw = (c.aimExplicit || moveLen < 0.1f) ? yawOf(p.aim) : yawOf(p.moveDir);
                if (moveLen > 0.1f || c.aimExplicit) p.k.yaw = approachAngle(p.k.yaw, wantYaw, 14.f * dt);
                if (tryDodge()) break;
                if (wantSpecial && p.specialCd <= 0.f) {
                    startSpecial(w, p);
                    break;
                }
                if (wantAttack) {
                    const bool chain = p.stateTime < p.cls->comboWindow && p.combo > 0;
                    startAttack(w, p, chain ? p.combo : 0);
                }
                break;
            }
            case PState::Attack: {
                const AttackStep& s = comboStep(p, p.combo);
                const ResolvedAction* a = p.vis.action(s.anim);
                const float dur = a ? a->realDuration() : 0.5f;
                const float hitT = a ? a->realHit() : dur * 0.5f;
                vel = Vector3Scale(p.moveDir, speed * p.cls->moveDuring);
                if (!p.hitFired && p.stateTime < hitT) p.k.yaw = approachAngle(p.k.yaw, yawOf(p.aim), 20.f * dt);
                if (hitEvent && !p.hitFired) {
                    p.hitFired = true;
                    p.aim = p.k.forward();
                    basicHit(w, p);
                }
                if (wantAttack && p.stateTime > hitT * 0.4f) p.comboQueued = true;
                if (p.hitFired && tryDodge()) break;
                if (p.hitFired && p.comboQueued && p.stateTime >= hitT + 0.06f) {
                    startAttack(w, p, (p.combo + 1) % static_cast<int>(p.cls->combo.size()));
                    break;
                }
                if (p.stateTime >= dur) {
                    p.state = PState::Move;
                    p.stateTime = 0.f;
                    p.combo = (p.combo + 1) % static_cast<int>(p.cls->combo.size());
                }
                break;
            }
            case PState::Special: {
                const ResolvedAction* a = p.vis.action("special");
                const float dur = a ? a->realDuration() : 0.6f;
                if (p.cls->specialType == "shadow_dash") {
                    const float dashT = p.cls->special.value("duration", 0.25f);
                    const float dist = p.cls->special.value("distance", 7.f);
                    if (p.stateTime <= dashT + dt * 0.5f) {
                        vel = Vector3Scale(p.dashDir, dist / dashT);
                        w.emit("shadow", Vector3Add(p.k.pos, Vector3{0, 1.f, 0}), Vector3{0, 1, 0});
                        if (p.stateTime + dt > dashT) {
                            p.hitFired = true;
                        }
                    }
                    if (p.hitFired && p.stateTime > dashT) {
                        endDash(w, p, p.dashStart);
                        p.hitFired = false;
                        p.state = PState::Move;
                        p.stateTime = 0.f;
                    }
                    break;
                }
                if (hitEvent && !p.hitFired) {
                    p.hitFired = true;
                    specialHit(w, p);
                }
                if (p.stateTime >= dur) {
                    if (!p.hitFired) specialHit(w, p);
                    p.state = PState::Move;
                    p.stateTime = 0.f;
                }
                break;
            }
            case PState::Dodge: {
                const float t = std::clamp(p.stateTime / dodgeDur, 0.f, 1.f);
                // Fast start, eased end.
                const float speedK = 2.f * (1.f - t);
                vel = Vector3Scale(p.dodgeDir, dodgeDist / dodgeDur * speedK);
                if (p.fireTrailTimer > 0.f) {
                    p.fireTrailTimer -= dt;
                    if (static_cast<int>(p.stateTime / dt) % 4 == 0) {
                        Hazard h;
                        h.kind = Hazard::Kind::FireTrail;
                        h.pos = p.k.pos;
                        h.radius = 1.2f * p.stats.aoeMult;
                        h.life = 2.5f;
                        h.damage = 6.f;
                        h.fromPlayer = true;
                        w.hazards.push_back(h);
                    }
                }
                if (p.stateTime >= dodgeDur) {
                    p.state = PState::Move;
                    p.stateTime = 0.f;
                    w.svc.phys.setCharacterCollides(p.k.phys, true);
                }
                break;
            }
            case PState::Hurt:
                vel = Vector3Scale(p.moveDir, speed * 0.3f);
                if (p.stateTime > 0.25f) {
                    p.state = PState::Move;
                    p.stateTime = 0.f;
                }
                if (tryDodge()) break;
                break;
            case PState::Downed: {
                const bool beingRevived = p.reviver != 0;
                if (!beingRevived) p.downedTime -= dt;
                vel = Vector3Scale(p.moveDir, w.cfg("revive", "crawl_speed"));
                if (moveLen > 0.1f) p.k.yaw = approachAngle(p.k.yaw, yawOf(p.moveDir), 4.f * dt);
                if (p.downedTime <= 0.f) {
                    p.state = PState::Dead;
                    p.stateTime = 0.f;
                    p.vis.play("death", true, 1.f, 0.2f);
                    w.svc.state.players[static_cast<size_t>(std::clamp(p.slot, 0, 3))].died = true;
                    w.text(Vector3Add(p.k.pos, Vector3{0, 2.f, 0}), "FALLEN", Color{200, 60, 60, 255}, 30.f);
                }
                break;
            }
            case PState::Revived:
                if (p.stateTime > 0.7f) {
                    p.state = PState::Move;
                    p.stateTime = 0.f;
                }
                break;
            case PState::Dead:
                break;
            case PState::Cheer:
                vel = Vector3Scale(p.moveDir, speed);
                if (moveLen > 0.1f || p.stateTime > 1.5f) {
                    p.state = PState::Move;
                    p.stateTime = 0.f;
                }
                break;
        }

        if (p.rage > 0.f && w.rng.chance(0.3f)) w.emit("rage", Vector3Add(p.k.pos, Vector3{0, 1.f, 0}), Vector3{0, 1, 0});

        updateKnockback(p.k, decay, dt);
        if (p.state != PState::Dead) {
            Vector3 v = Vector3Add(vel, p.k.knock);
            Vector3 np = w.svc.phys.moveCharacter(p.k.phys, v, dt);
            // Invisible leash: at max zoom nobody can leave the frame.
            if (w.players.size() > 1) {
                const Vector3 cl = w.camera.clampToLeash(np, aspect);
                if (cl.x != np.x || cl.z != np.z) {
                    np = Vector3{cl.x, np.y, cl.z};
                    w.svc.phys.setCharacterPosition(p.k.phys, np);
                }
            }
            p.k.pos = np;
        }
    }
}

void updateRevives(World& w, const std::array<PlayerCommand, 4>& cmds, float dt) {
    const float radius = w.cfg("revive", "radius");
    const float hold = w.cfg("revive", "hold_time");
    for (Player& d : w.players) {
        if (d.state != PState::Downed) {
            d.reviver = 0;
            continue;
        }
        Player* reviver = nullptr;
        for (Player& r : w.players) {
            if (&r == &d || !r.canAct() || r.state == PState::Dodge) continue;
            const PlayerCommand& c = cmds[static_cast<size_t>(std::clamp(r.slot, 0, 3))];
            if (!(c.held & eng::kBtnInteract)) continue;
            if (Vector3Distance(r.k.pos, d.k.pos) > radius) continue;
            reviver = &r;
            break;
        }
        const int cost = currentReviveCost(w);
        d.reviveBlockedNoGold = reviver && !w.svc.state.canAfford(cost);
        if (!reviver || d.reviveBlockedNoGold) {
            d.reviver = 0;
            d.reviveProgress = std::max(0.f, d.reviveProgress - dt * 0.5f);
            continue;
        }
        d.reviver = reviver->id;
        reviver->vis.play("interact");
        reviver->k.yaw = approachAngle(reviver->k.yaw, yawOf(Vector3Subtract(d.k.pos, reviver->k.pos)), 10.f * dt);
        d.reviveProgress += dt / (hold / std::max(reviver->stats.reviveSpeedMult, 0.1f));
        if (d.reviveProgress < 1.f) continue;
        w.svc.state.spend(cost);
        w.svc.state.recordRevive();
        w.svc.state.players[static_cast<size_t>(std::clamp(reviver->slot, 0, 3))].revivesGiven++;
        d.hp.hp = d.hp.max * w.cfg("revive", "hp_fraction");
        d.hp.iframes = 1.5f;
        d.state = PState::Revived;
        d.stateTime = 0.f;
        d.reviveProgress = 0.f;
        d.reviver = 0;
        d.vis.play("revive", true, 1.f, 0.1f);
        w.emit("revive", Vector3Add(d.k.pos, Vector3{0, 0.5f, 0}), Vector3{0, 1, 0});
        w.sound("revive");
        char buf[32];
        std::snprintf(buf, sizeof buf, "-%d GOLD", cost);
        w.text(Vector3Add(d.k.pos, Vector3{0, 2.6f, 0}), buf, Color{255, 210, 70, 255}, 26.f);
        hooks::onRevive(w, *reviver, d);
        reviver->state = PState::Move;
    }
}

}  // namespace game
