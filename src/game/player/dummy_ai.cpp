#include "game/player/dummy_ai.h"

#include <cmath>
#include <raymath.h>

#include "engine/input/input_types.h"

namespace game {
namespace {
float dist2D(Vector3 a, Vector3 b) { return Vector2Distance(Vector2{a.x, a.z}, Vector2{b.x, b.z}); }
Vector2 toMove(Vector3 d) {
    const float l = std::sqrt(d.x * d.x + d.z * d.z);
    return l > 1e-3f ? Vector2{d.x / l, d.z / l} : Vector2{0, 0};
}
}  // namespace

PlayerCommand dummyCommand(World& w, Player& self, float dt) {
    (void)dt;
    PlayerCommand c;
    if (!self.canAct()) return c;
    // 1) Revive downed teammates.
    for (Player& o : w.players) {
        if (&o == &self || o.state != PState::Downed) continue;
        const float d = dist2D(self.k.pos, o.k.pos);
        if (d < w.cfg("revive", "radius") * 0.8f) {
            c.held |= eng::kBtnInteract;
            return c;
        }
        c.move = toMove(Vector3Subtract(o.k.pos, self.k.pos));
        return c;
    }
    // 2) Fight the nearest enemy.
    const Enemy* target = nullptr;
    float best = 1e9f;
    for (const Enemy& e : w.enemies) {
        if (e.state == EState::Dead || e.state == EState::Spawn) continue;
        const float d = dist2D(self.k.pos, e.k.pos);
        if (d < best) {
            best = d;
            target = &e;
        }
        // Dodge away from a close wind-up aimed at us.
        if (d < 3.2f && (e.state == EState::Windup || e.state == EState::Telegraph) && e.target == self.id &&
            w.rng.chance(0.08f)) {
            c.move = toMove(Vector3Subtract(self.k.pos, e.k.pos));
            c.pressed |= eng::kBtnDodge;
            return c;
        }
    }
    const Player* leader = nullptr;
    for (const Player& p : w.players)
        if (!p.dummy && p.canAct()) {
            leader = &p;
            break;
        }
    if (target) {
        const bool ranged = self.cls->basicType == "projectile";
        const float want = ranged ? 7.f : 1.8f;
        const Vector3 to = Vector3Subtract(target->k.pos, self.k.pos);
        c.aim = Vector3Normalize(Vector3{to.x, 0, to.z});
        c.aimExplicit = true;
        if (best > want + 0.5f) c.move = toMove(to);
        else if (ranged && best < want - 2.f) c.move = toMove(Vector3Scale(to, -1.f));
        if (best < (ranged ? 14.f : 2.8f)) c.held |= eng::kBtnAttack;
        if (best < 4.f && self.specialCd <= 0.f && w.rng.chance(0.02f)) c.pressed |= eng::kBtnSpecial;
        // Don't wander off from the leader.
        if (leader && dist2D(self.k.pos, leader->k.pos) > 12.f) c.move = toMove(Vector3Subtract(leader->k.pos, self.k.pos));
        return c;
    }
    // 3) Follow the leader (or walk to an open door when the room is clear).
    if (leader && dist2D(self.k.pos, leader->k.pos) > 3.5f) c.move = toMove(Vector3Subtract(leader->k.pos, self.k.pos));
    return c;
}

}  // namespace game
