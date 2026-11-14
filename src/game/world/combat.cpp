#include "game/world/combat.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <raymath.h>

#include "game/world/relic_hooks.h"

namespace game {
namespace {

Vector3 flatDir(Vector3 from, Vector3 to) {
    Vector3 d{to.x - from.x, 0.f, to.z - from.z};
    const float l = Vector3Length(d);
    return l > 1e-4f ? Vector3Scale(d, 1.f / l) : Vector3{0, 0, 1};
}

Vector3 chest(const Kinematic& k) { return Vector3{k.pos.x, k.pos.y + k.height * 0.55f, k.pos.z}; }

void numberText(World& w, Vector3 at, float dmg, bool crit, Color c) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%d", static_cast<int>(std::lround(dmg)));
    at.x += w.rng.uniform(-0.4f, 0.4f);
    at.y += w.rng.uniform(0.f, 0.4f);
    w.text(at, buf, crit ? Color{255, 220, 40, 255} : c,
           crit ? w.cfg("damage_numbers", "crit_size") : w.cfg("damage_numbers", "size"));
}

PlayerRecord* record(World& w, const Player& p) {
    if (p.slot < 0 || p.slot >= 4) return nullptr;
    return &w.svc.state.players[static_cast<size_t>(p.slot)];
}

}  // namespace

void updateKnockback(Kinematic& k, float decay, float dt) {
    const float f = std::exp(-decay * dt);
    k.knock = Vector3Scale(k.knock, f);
    if (Vector3Length(k.knock) < 0.05f) k.knock = Vector3{0, 0, 0};
}

void killEnemy(World& w, Enemy& e, Player* killer) {
    if (e.state == EState::Dead) return;
    e.hp.hp = 0.f;
    e.state = EState::Dead;
    e.stateTime = 0.f;
    e.deadTime = 0.f;
    e.vis.play("death", true, 1.f, 0.08f);
    w.tokens.releaseAll(e.id);
    w.svc.phys.setCharacterCollides(e.k.phys, false);
    w.emit("bone_shards", chest(e.k), Vector3{0, 1, 0}, e.isBoss ? 4.f : 1.f);
    w.sound("enemy_die");
    float goldMult = 1.f;
    if (killer) {
        goldMult = killer->stats.goldMult;
        if (PlayerRecord* r = record(w, *killer)) ++r->kills;
    }
    const int amount = static_cast<int>(std::lround(
        static_cast<float>(w.rng.rangeInclusive(e.def->gold[0], e.def->gold[1])) * goldMult));
    if (amount > 0) w.spawnGold(e.k.pos, amount);
    if (killer) hooks::onKill(w, *killer, e);
}

bool damageEnemy(World& w, Enemy& e, DamageInfo d) {
    if (e.state == EState::Dead) return false;
    Player* attacker = d.fromPlayer ? w.player(d.attacker) : nullptr;
    const Vector3 toAttacker = flatDir(e.k.pos, d.from);
    const Vector3 fwd = e.k.forward();
    const float facingDot = Vector3DotProduct(fwd, toAttacker);

    if (attacker) {
        d.amount *= attacker->stats.damageMult;
        if (attacker->rage > 0.f) d.amount *= attacker->cls->special.value("damage_mult", 1.f);
        if (attacker->critWindow > 0.f) {
            d.crit = true;
            d.amount *= attacker->cls->special.value("crit_mult", 2.5f);
            attacker->critWindow = 0.f;
        }
        // Backstab relic: hits from behind crit.
        const float backArc = w.cfg("combat", "backstab_arc_deg") * 0.5f * DEG2RAD;
        if (!d.crit && hooks::has(*attacker, "backstab_crit") && facingDot < -std::cos(backArc)) {
            d.crit = true;
            d.amount *= w.cfg("combat", "crit_damage_mult");
        }
    }

    // Shielded enemies block frontal hits.
    if (e.def->behavior == "shield" && !d.unblockable && e.state != EState::Stagger && e.state != EState::Spawn) {
        const float arc = e.def->raw.value("shield_arc_deg", 120.f) * 0.5f * DEG2RAD;
        if (facingDot > std::cos(arc)) {
            const float mult = e.def->raw.value("shield_damage_mult", 0.f);
            const Vector3 sp = Vector3Add(chest(e.k), Vector3Scale(fwd, e.k.radius + 0.3f));
            w.emit("block_spark", sp, toAttacker);
            w.sound("block");
            w.text(Vector3Add(chest(e.k), Vector3{0, 1.2f, 0}), "BLOCK", Color{170, 200, 255, 255}, 24.f);
            e.k.knock = Vector3Add(e.k.knock, Vector3Scale(toAttacker, -d.knock * 0.4f));
            e.vis.play("block", true);
            w.hitstop(0.04f);
            if (mult <= 0.f) {
                if (d.heavy) {  // a heavy blow knocks the guard open
                    e.state = EState::Stagger;
                    e.stateTime = 0.f;
                    w.tokens.releaseAll(e.id);
                }
                return false;
            }
            d.amount *= mult;
        }
    }

    e.hp.hp -= d.amount;
    e.hp.flash = w.cfg("combat", "enemy_flash");
    e.lastHitBy = d.attacker;
    numberText(w, Vector3Add(chest(e.k), Vector3{0, 0.9f + (e.isBoss ? 2.5f : 0.f), 0}), d.amount, d.crit,
               Color{255, 255, 255, 255});
    const Vector3 away = Vector3Scale(toAttacker, -1.f);
    w.emit(d.crit ? "crit_spark" : "hit_spark", chest(e.k), away);
    w.sound(d.crit ? "crit" : (d.heavy ? "hit_heavy" : "hit"));
    w.hitstop((d.crit || d.heavy) ? w.cfg("hitstop", "crit") : w.cfg("hitstop", "hit"));
    w.shake(d.heavy || d.crit ? w.cfg("shake", "heavy") * 0.5f : w.cfg("shake", "hit") * 0.5f);
    const float resist = std::clamp(e.def->knockbackResist, 0.f, 1.f);
    e.k.knock = Vector3Add(e.k.knock, Vector3Scale(away, d.knock * (1.f - resist)));

    if (attacker) {
        if (PlayerRecord* r = record(w, *attacker)) r->damageDealt += d.amount;
        if (attacker->stats.lifesteal > 0.f) healPlayer(w, *attacker, d.amount * attacker->stats.lifesteal, false);
        hooks::onHit(w, *attacker, e, d.amount, d.crit);
    }
    if (e.hp.hp <= 0.f) {
        killEnemy(w, e, attacker);
        return true;
    }
    // Stagger interrupts the wind-up (and frees the attack token).
    if (!e.isBoss && e.def->staggerTime > 0.f && e.state != EState::Spawn &&
        (d.heavy || d.crit || e.state != EState::Lunge)) {
        e.state = EState::Stagger;
        e.stateTime = 0.f;
        e.vis.play("hit", true, 1.f, 0.05f);
        w.tokens.releaseAll(e.id);
    }
    return true;
}

void healPlayer(World& w, Player& p, float amount, bool showText) {
    if (!p.canAct() || amount <= 0.f) return;
    const float before = p.hp.hp;
    p.hp.hp = std::min(p.hp.max, p.hp.hp + amount);
    if (showText && p.hp.hp - before >= 1.f) {
        char buf[16];
        std::snprintf(buf, sizeof buf, "+%d", static_cast<int>(std::lround(p.hp.hp - before)));
        w.text(Vector3Add(chest(p.k), Vector3{0, 1.2f, 0}), buf, Color{120, 255, 140, 255}, 24.f);
    }
}

void setPlayerDowned(World& w, Player& p, const std::string& source) {
    p.hp.hp = 0.f;
    p.state = PState::Downed;
    p.stateTime = 0.f;
    p.downedTime = w.cfg("revive", "downed_time");
    p.reviveProgress = 0.f;
    p.rage = p.guard = 0.f;
    p.vis.play("downed", true, 1.f, 0.2f);
    w.sound("downed");
    w.text(Vector3Add(chest(p.k), Vector3{0, 1.4f, 0}), "DOWNED", Color{255, 90, 80, 255}, 30.f);
    w.tokens.clear();  // enemies re-pick targets
    if (PlayerRecord* r = record(w, p)) {
        ++r->downs;
        r->killedBy = source;
    }
}

void damagePlayer(World& w, Player& p, DamageInfo d) {
    if (!p.canAct() || p.hp.iframes > 0.f) return;
    if (p.guard > 0.f) d.amount *= p.cls->special.value("guard_damage_mult", 1.f);
    if (p.shield > 0.f) {
        const float absorbed = std::min(p.shield, d.amount);
        p.shield -= absorbed;
        d.amount -= absorbed;
    }
    p.hp.hp -= d.amount;
    p.hp.flash = 0.15f;
    p.hp.iframes = w.cfg("combat", "player_iframes_on_hit");
    const Vector3 away = flatDir(d.from, p.k.pos);
    p.k.knock = Vector3Add(p.k.knock, Vector3Scale(away, d.knock));
    numberText(w, Vector3Add(chest(p.k), Vector3{0, 0.9f, 0}), d.amount, false, Color{255, 90, 80, 255});
    w.sound("player_hurt");
    w.shake(d.heavy ? w.cfg("shake", "heavy") : w.cfg("shake", "hit"));
    w.hitstop(w.cfg("hitstop", "hit") * 0.6f);
    // Thorns relic reflects damage back to the attacker.
    if (p.stats.thorns > 0.f && !d.fromPlayer)
        if (Enemy* e = w.enemy(d.attacker)) {
            DamageInfo t;
            t.amount = d.amount * p.stats.thorns;
            t.from = p.k.pos;
            t.fromPlayer = true;
            t.attacker = p.id;
            t.unblockable = true;
            damageEnemy(w, *e, t);
        }
    if (p.hp.hp <= 0.f) {
        setPlayerDowned(w, p, d.source);
        return;
    }
    if (p.state == PState::Move) {
        p.state = PState::Hurt;
        p.stateTime = 0.f;
        p.vis.play("hit", true, 1.f, 0.05f);
    }
}

void spawnHit(World& w, const ActiveHit& h) { w.hits.push_back(h); }

void updateHits(World& w) {
    for (ActiveHit& h : w.hits) {
        if (h.fromPlayer) {
            for (Enemy& e : w.enemies) {
                if (e.state == EState::Dead || h.hits.contains(e.id)) continue;
                if (!eng::overlaps(h.box, w.hurtbox(e.k))) continue;
                h.hits.add(e.id);
                DamageInfo d;
                d.amount = h.damage;
                d.from = h.knockFrom;
                d.knock = h.knock;
                d.heavy = h.heavy;
                d.crit = h.crit;
                d.unblockable = h.unblockable;
                d.attacker = h.owner;
                d.fromPlayer = true;
                damageEnemy(w, e, d);
            }
        } else {
            for (Player& p : w.players) {
                if (!p.canAct() || h.hits.contains(p.id)) continue;
                if (!eng::overlaps(h.box, w.hurtbox(p.k))) continue;
                h.hits.add(p.id);
                DamageInfo d;
                d.amount = h.damage;
                d.from = h.knockFrom;
                d.knock = h.knock;
                d.heavy = h.heavy;
                d.fromPlayer = false;
                d.attacker = h.owner;
                d.source = h.source;
                damagePlayer(w, p, d);
            }
        }
        --h.steps;
    }
    w.hits.erase(std::remove_if(w.hits.begin(), w.hits.end(), [](const ActiveHit& h) { return h.steps <= 0; }), w.hits.end());
}

void updateProjectiles(World& w, float dt) {
    for (Projectile& pr : w.projectiles) {
        pr.prev = pr.pos;
        pr.life -= dt;
        if (pr.kind == ProjKind::Bomb) {
            if (w.rng.chance(0.4f)) w.emit("fire", pr.pos, Vector3{0, 1, 0}, 0.3f);
            if (pr.life > 0.f) continue;
            w.emit("bomb", pr.pos, Vector3{0, 1, 0});
            w.sound("hit_heavy");
            w.shake(0.2f);
            ActiveHit h;
            h.owner = pr.owner;
            h.fromPlayer = true;
            h.box.shape = eng::HitShape::Sector;
            h.box.arcDeg = 360.f;
            h.box.origin = Vector3{pr.pos.x, 0.f, pr.pos.z};
            h.box.radius = pr.radius;
            h.damage = pr.damage;
            h.knock = 6.f;
            h.unblockable = true;
            h.knockFrom = h.box.origin;
            h.steps = 1;
            spawnHit(w, h);
            continue;
        }
        // Homing: turn toward the target at a limited rate.
        if (pr.turnRate > 0.f) {
            Vector3 tgt{};
            bool have = false;
            if (pr.fromPlayer) {
                if (Enemy* e = w.enemy(pr.target); e && e->state != EState::Dead) {
                    tgt = chest(e->k);
                    have = true;
                }
            } else if (Player* p = w.player(pr.target); p && p->canAct()) {
                tgt = chest(p->k);
                have = true;
            }
            if (have) {
                const float speed = Vector3Length(pr.vel);
                const Vector3 want = Vector3Normalize(Vector3Subtract(tgt, pr.pos));
                const Vector3 cur = Vector3Scale(pr.vel, 1.f / std::max(speed, 1e-4f));
                const float ang = std::acos(std::clamp(Vector3DotProduct(cur, want), -1.f, 1.f));
                const float t = ang > 1e-4f ? std::min(1.f, pr.turnRate * dt / ang) : 1.f;
                pr.vel = Vector3Scale(Vector3Normalize(Vector3Lerp(cur, want, t)), speed);
                pr.vel.y = 0.f;
            }
        }
        pr.pos = Vector3Add(pr.pos, Vector3Scale(pr.vel, dt));
        w.emit(pr.fromPlayer ? "bolt_trail" : "enemy_bolt_trail", pr.pos, Vector3{0, 1, 0});
        if (!w.room.walkable(Vector3{pr.pos.x, 0, pr.pos.z}) && !w.room.nav.walkable(pr.prev)) {
            // Left the walkable area (hit a wall or prop).
            pr.life = 0.f;
        } else if (!w.room.walkable(Vector3{pr.pos.x, 0, pr.pos.z})) {
            float frac = 0.f;
            if (w.svc.phys.raycastStatic(pr.prev, pr.pos, &frac)) pr.life = 0.f;
        }
        if (pr.life <= 0.f) {
            w.emit("bolt_burst", pr.pos, Vector3{0, 1, 0}, 0.6f);
            continue;
        }
        eng::Hitbox hb;
        hb.shape = eng::HitShape::Sphere;
        hb.origin = pr.pos;
        hb.radius = pr.radius;
        if (pr.fromPlayer) {
            for (Enemy& e : w.enemies) {
                if (e.state == EState::Dead || pr.hits.contains(e.id) || !eng::overlaps(hb, w.hurtbox(e.k))) continue;
                pr.hits.add(e.id);
                DamageInfo d;
                d.amount = pr.damage;
                d.from = pr.prev;
                d.knock = pr.knock;
                d.crit = pr.crit;
                d.attacker = pr.owner;
                damageEnemy(w, e, d);
                w.emit("bolt_burst", pr.pos, Vector3{0, 1, 0});
                pr.life = 0.f;
                break;
            }
        } else {
            for (Player& p : w.players) {
                if (!p.canAct() || pr.hits.contains(p.id) || !eng::overlaps(hb, w.hurtbox(p.k))) continue;
                if (p.hp.iframes > 0.f) continue;  // dodge through
                pr.hits.add(p.id);
                DamageInfo d;
                d.amount = pr.damage;
                d.from = pr.prev;
                d.knock = pr.knock;
                d.fromPlayer = false;
                d.attacker = pr.owner;
                d.source = "a Skeleton Mage's bolt";
                damagePlayer(w, p, d);
                w.emit("bolt_burst", pr.pos, Vector3{0, 1, 0});
                pr.life = 0.f;
                break;
            }
        }
    }
    w.projectiles.erase(std::remove_if(w.projectiles.begin(), w.projectiles.end(), [](const Projectile& p) { return p.life <= 0.f; }),
                        w.projectiles.end());
}

void updateHazards(World& w, float dt) {
    for (Hazard& h : w.hazards) {
        h.life -= dt;
        if (h.warmup > 0.f) {
            h.warmup -= dt;
            continue;
        }
        switch (h.kind) {
            case Hazard::Kind::ShockRing: {
                h.radius = std::min(h.maxRadius, h.radius + h.speed * dt);
                h.inner = std::max(0.f, h.radius - h.width);
                if (h.radius >= h.maxRadius) h.life = 0.f;
                eng::Hitbox hb;
                hb.shape = eng::HitShape::Ring;
                hb.origin = h.pos;
                hb.radius = h.radius;
                hb.innerRadius = h.inner;
                hb.height = 1.0f;  // low: roll through it
                for (Player& p : w.players) {
                    if (!p.canAct() || h.hits.contains(p.id) || p.hp.iframes > 0.f) continue;
                    if (!eng::overlaps(hb, w.hurtbox(p.k))) continue;
                    h.hits.add(p.id);
                    DamageInfo d;
                    d.amount = h.damage;
                    d.from = h.pos;
                    d.knock = 8.f;
                    d.heavy = true;
                    d.fromPlayer = false;
                    d.source = "the Bone King's shockwave";
                    damagePlayer(w, p, d);
                }
                break;
            }
            case Hazard::Kind::FireTrail:
            case Hazard::Kind::TrapFire: {
                h.tick -= dt;
                if (w.rng.chance(0.5f)) w.emit("fire", Vector3Add(h.pos, Vector3{0, 0.2f, 0}), Vector3{0, 1, 0}, 0.4f);
                if (h.tick > 0.f) break;
                h.tick = 0.4f;
                eng::Hitbox hb;
                hb.shape = eng::HitShape::Sector;
                hb.arcDeg = 360.f;
                hb.origin = h.pos;
                hb.radius = h.radius;
                hb.height = 2.f;
                for (Enemy& e : w.enemies) {
                    if (e.state == EState::Dead || !eng::overlaps(hb, w.hurtbox(e.k))) continue;
                    DamageInfo d;
                    d.amount = h.damage;
                    d.from = h.pos;
                    d.unblockable = true;
                    d.fromPlayer = true;
                    d.attacker = 0;
                    damageEnemy(w, e, d);
                }
                if (h.kind == Hazard::Kind::TrapFire)
                    for (Player& p : w.players) {
                        if (!p.canAct() || !eng::overlaps(hb, w.hurtbox(p.k))) continue;
                        DamageInfo d;
                        d.amount = h.damage;
                        d.from = h.pos;
                        d.knock = 3.f;
                        d.fromPlayer = false;
                        d.source = "a fire trap";
                        damagePlayer(w, p, d);
                    }
                break;
            }
        }
    }
    w.hazards.erase(std::remove_if(w.hazards.begin(), w.hazards.end(), [](const Hazard& h) { return h.life <= 0.f; }),
                    w.hazards.end());
}

void updateGold(World& w, float dt) {
    const float pickR = w.cfg("gold", "pickup_radius"), magR = w.cfg("gold", "magnet_radius"),
                magS = w.cfg("gold", "magnet_speed");
    for (GoldPickup& g : w.gold) {
        g.age += dt;
        Player* nearest = nullptr;
        float best = 1e9f;
        for (Player& p : w.players) {
            if (!p.canAct()) continue;
            const float d = Vector3Distance(Vector3{g.pos.x, 0, g.pos.z}, Vector3{p.k.pos.x, 0, p.k.pos.z});
            if (d < best) {
                best = d;
                nearest = &p;
            }
        }
        if (nearest && g.age > 0.35f && best < magR) {
            const Vector3 to = Vector3Normalize(Vector3Subtract(Vector3Add(nearest->k.pos, Vector3{0, 1, 0}), g.pos));
            g.vel = Vector3Lerp(g.vel, Vector3Scale(to, magS), std::min(1.f, dt * 8.f));
        } else {
            g.vel.y -= 25.f * dt;
            g.vel.x *= std::exp(-2.f * dt);
            g.vel.z *= std::exp(-2.f * dt);
        }
        g.pos = Vector3Add(g.pos, Vector3Scale(g.vel, dt));
        if (g.pos.y < 0.25f) {
            g.pos.y = 0.25f;
            g.vel.y = std::fabs(g.vel.y) * 0.35f;
        }
        if (nearest && g.age > 0.35f && best < pickR) {
            const int amt = std::max(1, static_cast<int>(std::lround(static_cast<float>(g.amount) * nearest->stats.goldMult)));
            w.svc.state.addGold(amt);
            w.emit("gold_sparkle", g.pos, Vector3{0, 1, 0});
            w.sound("gold");
            g.amount = 0;
        }
    }
    w.gold.erase(std::remove_if(w.gold.begin(), w.gold.end(), [](const GoldPickup& g) { return g.amount <= 0; }), w.gold.end());
}

void updateTexts(World& w, float dt) {
    const float rise = w.cfg("damage_numbers", "rise");
    for (FloatText& t : w.texts) {
        t.life -= dt;
        t.pos.y += rise * dt * (t.life / t.maxLife + 0.3f);
    }
    w.texts.erase(std::remove_if(w.texts.begin(), w.texts.end(), [](const FloatText& t) { return t.life <= 0.f; }), w.texts.end());
}

}  // namespace game
