#include "game/scenes/run_scene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <raymath.h>

#include "engine/core/log.h"
#include "game/data/relic_defs.h"
#include "game/enemies/enemy_system.h"
#include "game/player/dummy_ai.h"
#include "game/rooms/floor_generator.h"
#include "game/scenes/lobby_scene.h"
#include "game/player/player_system.h"
#include "game/ui/glyphs.h"
#include "game/world/combat.h"
#include "game/world/relic_hooks.h"

namespace game {
namespace {

bool finite3(Vector3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

}  // namespace

RunScene::RunScene(App& app, const RunConfig& cfg) : app_(app), cfg_(cfg) {
    WorldServices svc{app.data(), app.state(), app.assets(), app.characters(), app.physics(), app.particles(),
                      app.audio(), app.clock(), app.fx()};
    world_ = std::make_unique<World>(svc);
    world_->rng.reseed(cfg.seed);
    app.state().startRun(cfg.seed);
    for (const PlayerSetup& ps : cfg.players) {
        auto& rec = app.state().players[static_cast<size_t>(ps.slot)];
        rec.active = true;
        rec.classId = ps.classId;
        world_->addPlayer(ps.slot, ps.classId, Vector3{static_cast<float>(ps.slot) * 1.5f, 0, 0}, ps.dummy || cfg.allBots);
    }
    app.particles().clear();
    startFloor(0);
    if (cfg.reviveDemo && world_->players.size() > 1) {
        app.state().addGold(60);
        setPlayerDowned(*world_, world_->players[1], "a demo");
    }
    app.audio().playMusic("explore", 1.0f);
}

RunScene::~RunScene() { world_.reset(); }

float RunScene::aspect() const {
    return static_cast<float>(std::max(GetScreenWidth(), 1)) / static_cast<float>(std::max(GetScreenHeight(), 1));
}

void RunScene::startFloor(int floor) {
    GameData& d = app_.data();
    app_.state().startFloor(floor);
    world_->theme = &d.floors[static_cast<size_t>(std::clamp(floor, 0, static_cast<int>(d.floors.size()) - 1))];
    plan_ = FloorPlan{};
    if (cfg_.singleRoom.empty()) {
        plan_ = generateFloor(d, floor, cfg_.seed);
    } else {
        // Single-room mode (captures, M1 sandbox): one room whose doors all lead back to itself.
        RoomNode n;
        n.templateId = cfg_.singleRoom;
        n.type = d.room(n.templateId).type;
        n.neighbor = {0, 0, 0, 0};
        plan_.rooms.push_back(n);
        plan_.start = 0;
    }
    enterRoom(plan_.start, -1);
}

void RunScene::enterRoom(int index, int fromSide) {
    World& w = *world_;
    current_ = index;
    RoomNode& node = plan_.rooms[static_cast<size_t>(index)];
    node.visited = true;
    const RoomTemplate& tpl = app_.data().room(node.templateId);
    eng::log::info("floor %d: enter room %d (%s, %s)", app_.state().floor + 1, index, node.templateId.c_str(),
                   roomTypeName(node.type));
    w.clearEnemies();
    w.gold.clear();
    w.texts.clear();
    picker_.close();
    std::array<bool, 4> connected{};
    for (int s = 0; s < 4; ++s) connected[static_cast<size_t>(s)] = node.neighbor[static_cast<size_t>(s)] >= 0;
    // The floor's last room gets an extra door: the way down (shrine) or out (boss).
    exitSide_ = -1;
    if (index == plan_.exit && cfg_.singleRoom.empty()) {
        for (int s : {kNorth, kEast, kWest, kSouth})
            if (tpl.hasDoor(static_cast<Side>(s)) && !connected[static_cast<size_t>(s)] && s != fromSide) {
                exitSide_ = s;
                connected[static_cast<size_t>(s)] = true;
                break;
            }
    }
    buildRoom(tpl, *w.theme, app_.assets(), app_.physics(), connected, w.room);
    // Players arrive through the door they walked into (opposite side of the previous room).
    Vector3 base{0, 0, 0};
    Vector3 inward{0, 0, -1};
    if (fromSide >= 0 && w.room.doors[static_cast<size_t>(fromSide)].socket) {
        base = w.room.doorEntry(static_cast<Side>(fromSide), 2.5f);
        inward = w.room.doors[static_cast<size_t>(fromSide)].inward;
    } else {
        for (const TileMarker& m : tpl.markers)
            if (m.kind == "start") base = w.room.tileCenter(m.tx, m.tz);
        if (tpl.hasDoor(kSouth)) {
            base = w.room.doorEntry(kSouth, 3.f);
            inward = Vector3{0, 0, -1};
        }
    }
    const Vector3 side{inward.z, 0, -inward.x};
    for (size_t i = 0; i < w.players.size(); ++i) {
        Player& p = w.players[i];
        const float off = (static_cast<float>(i) - (static_cast<float>(w.players.size()) - 1.f) * 0.5f) * 1.6f;
        Vector3 pos = Vector3Add(base, Vector3Scale(side, off));
        pos = w.room.nav.walkable(pos) ? pos : w.room.nav.nearestWalkable(pos);
        w.placePlayer(p, pos);
        p.k.yaw = p.k.prevYaw = std::atan2(inward.x, inward.z);
        resetPlayerForRoom(p);
    }
    std::vector<Vector3> pts;
    for (const Player& p : w.players) pts.push_back(p.k.pos);
    w.camera.setBounds(w.room.boundsMin, w.room.boundsMax);
    w.camera.snap(pts, aspect());
    phase_ = Phase::Enter;
    phaseTime_ = 0.f;
    spawned_ = false;
    fade_ = 1.f;
    app_.state().roomIndex = index;
    // Room content: treasure chests, trap grates.
    traps_.clear();
    trapClock_ = 0.f;
    for (const TileMarker& m : tpl.markers) {
        if (m.kind == "chest" && node.type == RoomType::Treasure && !node.cleared) {
            Chest c;
            c.pos = w.room.tileCenter(m.tx, m.tz);
            c.autoOpen = false;
            c.gold = static_cast<int>(w.rng.rangeInclusive(30, 50));
            w.chests.push_back(c);
        } else if (m.kind == "trap") {
            traps_.push_back(Trap{w.room.tileCenter(m.tx, m.tz), static_cast<float>((m.tx + m.tz) % 3) * 1.1f});
        }
    }
    // Rooms without enemies count as cleared at once (doors open).
    if (tpl.spawns.empty()) node.cleared = true;  // treasure/shrine rooms start open, no banner
    if (node.cleared || tpl.spawns.empty()) {
        for (int s = 0; s < 4; ++s) w.room.setDoorOpen(static_cast<Side>(s), true, app_.physics());
        for (auto& d : w.room.doors) d.openT = d.open ? 1.f : 0.f;
        spawned_ = true;
    }
}

void RunScene::spawnRoomEnemies() {
    World& w = *world_;
    const RoomTemplate& tpl = app_.data().room(plan_.rooms[static_cast<size_t>(current_)].templateId);
    const int players = std::max(1, w.playerCount());
    for (const TileMarker& m : tpl.spawns) {
        const std::string kind = pickPoolEnemy(m.kind);
        const EnemyDef& def = app_.data().enemy(kind);
        const int base = w.rng.rangeInclusive(def.group[0], def.group[1]);
        const int count = def.behavior == "boss" ? 1 : app_.data().scaling.spawnCount(base, players);
        const Vector3 c = w.room.tileCenter(m.tx, m.tz);
        for (int i = 0; i < count; ++i) {
            const float a = static_cast<float>(i) / static_cast<float>(count) * 2.f * PI + w.rng.uniform(0.f, 0.5f);
            const float r = count > 1 ? 1.2f + 0.35f * static_cast<float>(i % 3) : 0.f;
            Vector3 pos{c.x + std::cos(a) * r, 0.f, c.z + std::sin(a) * r};
            if (!w.room.walkable(pos)) pos = w.room.nav.nearestWalkable(pos);
            w.spawnEnemy(kind, pos);
        }
    }
    spawned_ = true;
    for (int s = 0; s < 4; ++s) w.room.setDoorOpen(static_cast<Side>(s), false, app_.physics());
}

void RunScene::updateBotGoal() {
    // Where AI teammates head when there is nothing to fight (only used when no human leads).
    World& w = *world_;
    w.botGoalActive = false;
    for (const Chest& c : w.chests)
        if (!c.opened) {
            w.botGoal = c.pos;
            w.botGoalActive = true;
            return;
        }
    const RoomNode& node = plan_.rooms[static_cast<size_t>(current_)];
    if (!node.cleared || picker_.active()) return;
    int side = exitSide_;
    if (side < 0) {
        // Prefer an unvisited neighbor, the branch first, then the next room on the chain.
        int best = -1, bestScore = -1;
        for (int s = 0; s < 4; ++s) {
            const int nb = node.neighbor[static_cast<size_t>(s)];
            if (nb < 0 || nb == current_) continue;
            const RoomNode& o = plan_.rooms[static_cast<size_t>(nb)];
            const int score = (!o.visited ? 10 : 0) + (o.branch ? 5 : 0) + (nb > current_ ? 2 : 0);
            if (score > bestScore) {
                bestScore = score;
                best = s;
            }
        }
        side = best;
    }
    if (side < 0 && !cfg_.singleRoom.empty()) side = kNorth;
    if (side < 0) return;
    const DoorInstance& d = w.room.doors[static_cast<size_t>(side)];
    w.botGoal = Vector3Subtract(d.center, Vector3Scale(d.inward, 1.5f));
    w.botGoalActive = true;
}

std::string RunScene::pickPoolEnemy(const std::string& kind) {
    // "random" and enemies not in this floor's pool become a random non-minion from the pool.
    const auto& pool = world_->theme->enemies;
    if (kind == "bone_king" || kind == "minion") return kind;
    if (kind != "random" && std::find(pool.begin(), pool.end(), kind) != pool.end()) return kind;
    std::vector<std::string> options;
    for (const auto& e : pool)
        if (e != "minion") options.push_back(e);
    if (options.empty()) return "minion";
    return options[static_cast<size_t>(world_->rng.range(static_cast<int>(options.size())))];
}

void RunScene::buildCommands(float dt) {
    World& w = *world_;
    eng::InputRouter& in = app_.input();
    for (Player& p : w.players) {
        PlayerCommand c;
        if (p.dummy) {
            c = dummyCommand(w, p, dt);
        } else if (in.slotActive(p.slot)) {
            const eng::SlotInput& si = in.input(p.slot);
            c.move = si.move;
            c.held = si.held;
            c.pressed = in.consumePressed(p.slot);
            if (si.usesMouse) {
                // Mouse aim: project the cursor onto the ground plane at hip height.
                const Ray ray = app_.renderer().screenRay(si.mouse);
                if (std::fabs(ray.direction.y) > 1e-4f) {
                    const float t = (1.0f - ray.position.y) / ray.direction.y;
                    const Vector3 hit = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
                    const Vector3 d{hit.x - p.k.pos.x, 0, hit.z - p.k.pos.z};
                    if (Vector3Length(d) > 0.3f) {
                        c.aim = Vector3Normalize(d);
                        c.aimExplicit = true;
                    }
                }
            } else if (Vector2Length(si.aimStick) > 0.3f) {
                c.aim = Vector3Normalize(Vector3{si.aimStick.x, 0, si.aimStick.y});
                c.aimExplicit = true;
            }
        }
        if (picker_.picking(p.slot)) {
            c = PlayerCommand{};  // the relic panel owns this player's input
            if (!p.dummy) in.consumePressed(p.slot);
        }
        cmds_[static_cast<size_t>(std::clamp(p.slot, 0, 3))] = c;
    }
}

void RunScene::onRoomCleared() {
    World& w = *world_;
    RoomNode& node = plan_.rooms[static_cast<size_t>(current_)];
    node.cleared = true;
    ++app_.state().roomsCleared;
    phase_ = Phase::Cleared;
    phaseTime_ = 0.f;
    for (int s = 0; s < 4; ++s) w.room.setDoorOpen(static_cast<Side>(s), true, app_.physics());
    w.sound("door");
    banner_ = "ROOM CLEARED";
    bannerTime_ = 2.0f;
    hooks::onRoomClear(w);
    if (node.type == RoomType::Combat || node.type == RoomType::Trap) {
        // A chest pops up in the middle of the room and opens by itself.
        Chest c;
        c.pos = w.room.nav.nearestWalkable(Vector3{w.camera.focus().x, 0, w.camera.focus().z});
        c.autoOpen = true;
        const auto cg = w.svc.data.config.at("gold").at("chest_gold");
        c.gold = w.rng.rangeInclusive(cg[0].get<int>(), cg[1].get<int>());
        w.chests.push_back(c);
    }
    for (Player& p : w.players)
        if (p.canAct() && p.state == PState::Move) {
            p.state = PState::Cheer;
            p.stateTime = 0.f;
            p.vis.play("cheer", true);
        }
}

void RunScene::checkDoors() {
    World& w = *world_;
    const RoomNode& node = plan_.rooms[static_cast<size_t>(current_)];
    const float depth = w.cfg("room", "door_trigger_depth");
    for (int s = 0; s < 4; ++s) {
        const DoorInstance& d = w.room.doors[static_cast<size_t>(s)];
        if (!d.connected || !d.open || d.openT < 0.9f) continue;
        for (const Player& p : w.players) {
            if (!p.canAct()) continue;
            const Vector3 rel = Vector3Subtract(p.k.pos, d.center);
            const float along = -Vector3DotProduct(rel, d.inward);  // > 0 when past the door line
            const float lateral = std::fabs(Vector3DotProduct(rel, Vector3{d.inward.z, 0, -d.inward.x}));
            if (along > -depth && lateral < w.room.tile * 0.4f) {
                if (s == exitSide_) {
                    floorExit_ = true;
                    pendingRoom_ = -1;
                    phase_ = Phase::Exit;
                    phaseTime_ = 0.f;
                    return;
                }
                pendingRoom_ = node.neighbor[static_cast<size_t>(s)];
                pendingSide_ = static_cast<int>(oppositeSide(static_cast<Side>(s)));
                phase_ = Phase::Exit;
                phaseTime_ = 0.f;
                return;
            }
        }
    }
}

void RunScene::updateChests(float dt) {
    World& w = *world_;
    for (Chest& c : w.chests) {
        c.age += dt;
        if (c.opened) {
            c.openAge += dt;
            continue;
        }
        bool open = c.autoOpen && c.age > 0.8f;
        if (!c.autoOpen)
            for (const Player& p : w.players)
                if (p.canAct() && Vector3Distance(p.k.pos, c.pos) < 2.6f) open = true;
        if (!open) continue;
        c.opened = true;
        c.openAge = 0.f;
        w.sound("chest");
        w.emit("gold_sparkle", Vector3Add(c.pos, Vector3{0, 1.f, 0}), Vector3{0, 1, 0}, 5.f);
        w.emit("relic_glow", Vector3Add(c.pos, Vector3{0, 0.8f, 0}), Vector3{0, 1, 0}, 6.f);
        w.spawnGold(Vector3Add(c.pos, Vector3{0, 0.5f, 0}), c.gold);
        picker_.open(w);
    }
}

void RunScene::updateTraps(float dt) {
    World& w = *world_;
    if (traps_.empty()) return;
    const float period = 3.4f, warm = 0.9f, burn = 1.3f;
    const float before = trapClock_;
    trapClock_ += dt;
    for (const Trap& t : traps_) {
        // Start a cycle when the trap's phase-shifted clock wraps.
        const float a = std::fmod(before + t.phase, period), b = std::fmod(trapClock_ + t.phase, period);
        if (b < a) {
            Hazard h;
            h.kind = Hazard::Kind::TrapFire;
            h.pos = t.pos;
            h.radius = 1.7f;
            h.warmup = warm;  // red warning decal first
            h.life = warm + burn;
            h.damage = 9.f;
            w.hazards.push_back(h);
        }
    }
}

void RunScene::nextFloor() {
    GameState& st = app_.state();
    if (st.floor + 1 >= static_cast<int>(app_.data().floors.size())) {
        endRun(true);
        return;
    }
    startFloor(st.floor + 1);
    banner_ = world_->theme->name;
    bannerTime_ = 2.5f;
}

bool RunScene::humanDisconnected() const {
    for (const Player& p : world_->players)
        if (!p.dummy && app_.input().slotDisconnected(p.slot)) return true;
    return false;
}

void RunScene::endRun(bool victory) {
    app_.state().victory = victory;
    // The run summary scene arrives in M4; until then go back to the lobby.
    app_.switchTo(std::make_unique<LobbyScene>(app_));
}

void RunScene::fixedUpdate(float dt) {
    if (paused_ || disconnectedSlot_ >= 0) return;  // simulation frozen
    World& w = *world_;
    w.time += dt;
    phaseTime_ += dt;
    app_.state().runTime += dt;
    bannerTime_ = std::max(0.f, bannerTime_ - dt);
    updateBotGoal();
    buildCommands(dt);
    updatePlayers(w, cmds_, dt, aspect());
    updateRevives(w, cmds_, dt);
    updateEnemies(w, dt);
    updateHits(w);
    updateProjectiles(w, dt);
    updateHazards(w, dt);
    updateGold(w, dt);
    updateTexts(w, dt);
    w.removeDeadEnemies();
    updateChests(dt);
    updateTraps(dt);
    w.room.updateDoors(dt);
    app_.particles().update(dt);

    std::vector<Vector3> pts;
    for (const Player& p : w.players)
        if (p.state != PState::Dead) pts.push_back(p.k.pos);
    w.camera.update(pts, aspect(), dt);

    const float transition = w.cfg("room", "transition_time");
    switch (phase_) {
        case Phase::Enter:
            fade_ = std::max(0.f, 1.f - phaseTime_ / transition);
            if (!spawned_ && phaseTime_ > 0.5f) spawnRoomEnemies();
            if (spawned_ && phaseTime_ > transition) phase_ = Phase::Fight;
            break;
        case Phase::Fight:
            if (spawned_ && w.livingEnemies() == 0 && !plan_.rooms[static_cast<size_t>(current_)].cleared &&
                phaseTime_ > w.cfg("room", "clear_delay"))
                onRoomCleared();
            else if (plan_.rooms[static_cast<size_t>(current_)].cleared)
                checkDoors();
            break;
        case Phase::Cleared:
            if (phaseTime_ > 0.5f) checkDoors();
            break;
        case Phase::Exit:
            fade_ = std::min(1.f, phaseTime_ / transition);
            if (phaseTime_ >= transition && floorExit_) {
                floorExit_ = false;
                nextFloor();
            } else if (phaseTime_ >= transition && pendingRoom_ >= 0) {
                // Single-room mode re-arms the room so it can be fought again.
                if (!cfg_.singleRoom.empty()) plan_.rooms[static_cast<size_t>(pendingRoom_)].cleared = false;
                enterRoom(pendingRoom_, pendingSide_);
                pendingRoom_ = -1;
            }
            break;
    }
    // Everyone down (or dead) = run over.
    if (w.allPlayersDown()) {
        if (gameOverTime_ == 0.f) {
            banner_ = "THE CREW HAS FALLEN";
            bannerTime_ = 5.f;
            app_.audio().stopMusic(2.f);
        }
        gameOverTime_ += dt;
        fade_ = std::clamp((gameOverTime_ - 3.f) / 1.5f, 0.f, 1.f);
        if (gameOverTime_ > 4.6f) endRun(false);
    }
    if (app_.state().purse != lastPurse_) {
        goldPulse_ = 1.f;
        lastPurse_ = app_.state().purse;
    }
    goldPulse_ = std::max(0.f, goldPulse_ - dt * 3.f);
}

void RunScene::frameUpdate(float) {
    eng::InputRouter& in = app_.input();
    // A disconnected pad pauses the game until it comes back.
    disconnectedSlot_ = -1;
    for (const Player& p : world_->players)
        if (!p.dummy && in.slotDisconnected(p.slot)) {
            disconnectedSlot_ = p.slot;
            break;
        }
    if (disconnectedSlot_ >= 0) {
        in.clearLatched();
        return;
    }
    if (!paused_ && picker_.active()) picker_.update(*world_, in, 1.f / 60.f);
    if (!paused_) {
        for (const Player& p : world_->players) {
            if (p.dummy || !(in.framePressed(p.slot) & eng::kBtnPause)) continue;
            paused_ = true;
            pauseSlot_ = p.slot;
            pauseFocus_ = 0;
            app_.audio().play("ui_confirm");
            break;
        }
        return;
    }
    // Pause menu: the player who paused navigates (keyboard always works too).
    uint32_t p = in.framePressed(pauseSlot_);
    const int kb = in.slotOfDevice(eng::kKeyboardDevice);
    if (kb >= 0 && kb != pauseSlot_) p |= in.framePressed(kb);
    if (p & eng::kBtnUp) pauseFocus_ = (pauseFocus_ + 1) % 2, app_.audio().play("ui_move");
    if (p & eng::kBtnDown) pauseFocus_ = (pauseFocus_ + 1) % 2, app_.audio().play("ui_move");
    if ((p & eng::kBtnBack) || ((p & eng::kBtnPause) && !(p & eng::kBtnBack))) {
        paused_ = false;
        in.clearLatched();
        app_.audio().play("ui_back");
        return;
    }
    if (p & eng::kBtnConfirm) {
        app_.audio().play("ui_confirm");
        if (pauseFocus_ == 0) {
            paused_ = false;
            in.clearLatched();
        } else {
            endRun(false);
        }
    }
}

void RunScene::drawPause() {
    const eng::Ui& ui = app_.ui();
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 150});
    char buf[96];
    if (disconnectedSlot_ >= 0) {
        std::snprintf(buf, sizeof buf, "Reconnect P%d's controller", disconnectedSlot_ + 1);
        ui.text("PAUSED", Vector2{ui.width() * 0.5f, ui.height() * 0.36f}, 64, Color{240, 220, 180, 255}, eng::Align::Center);
        ui.text(buf, Vector2{ui.width() * 0.5f, ui.height() * 0.46f}, 40, slotColor(disconnectedSlot_), eng::Align::Center);
        return;
    }
    std::snprintf(buf, sizeof buf, "PAUSED (P%d)", pauseSlot_ + 1);
    ui.text(buf, Vector2{ui.width() * 0.5f, ui.height() * 0.3f}, 64, Color{240, 220, 180, 255}, eng::Align::Center);
    const float bw = 420, bh = 70;
    const char* labels[2] = {"Resume", "Quit to Lobby"};
    for (int i = 0; i < 2; ++i)
        ui.button(labels[i], Rectangle{(ui.width() - bw) * 0.5f, ui.height() * 0.42f + static_cast<float>(i) * (bh + 18), bw, bh},
                  pauseFocus_ == i, false);
    const eng::PadFamily fam = app_.input().slotFamily(pauseSlot_);
    drawPrompt(ui, fam, Prompt::Confirm, "Select", Vector2{ui.width() * 0.5f - 200, ui.height() * 0.42f + 200}, 36);
    drawPrompt(ui, fam, Prompt::Back, "Resume", Vector2{ui.width() * 0.5f + 40, ui.height() * 0.42f + 200}, 36);
}

bool RunScene::healthy(std::string& why) const {
    for (const Player& p : world_->players)
        if (!finite3(p.k.pos) || !std::isfinite(p.hp.hp)) {
            why = "player " + std::to_string(p.slot) + " has a NaN position/hp";
            return false;
        }
    for (const Enemy& e : world_->enemies)
        if (!finite3(e.k.pos) || !std::isfinite(e.hp.hp)) {
            why = "enemy " + e.def->id + " has a NaN position/hp";
            return false;
        }
    for (const Projectile& pr : world_->projectiles)
        if (!finite3(pr.pos)) {
            why = "projectile NaN";
            return false;
        }
    return true;
}

void RunScene::renderWorld(float alpha) {
    World& w = *world_;
    eng::Renderer& r = app_.renderer();
    const Camera3D cam = w.camera.camera(w.time);
    std::vector<eng::PointLight> lights = w.room.lights;
    for (const Projectile& pr : w.projectiles) {
        if (lights.size() > 40) break;
        lights.push_back(eng::PointLight{pr.pos, pr.fromPlayer ? Vector3{1.2f, 0.7f, 2.2f} : Vector3{0.6f, 2.f, 0.5f}, 5.f});
    }
    for (const Hazard& h : w.hazards)
        if (h.kind != Hazard::Kind::ShockRing && lights.size() < 48)
            lights.push_back(eng::PointLight{Vector3Add(h.pos, Vector3{0, 1, 0}), Vector3{1.6f, 0.7f, 0.2f}, 4.f});
    r.beginScene(cam, w.theme->lighting, lights);
    w.room.draw(r);

    for (Player& p : w.players) {
        const Vector3 pos = p.k.lerpPos(alpha);
        Color tint = WHITE;
        Vector3 em{0, 0, 0};
        if (p.rage > 0.f) em = Vector3{0.35f, 0.05f, 0.f};
        if (p.guard > 0.f) em = Vector3{0.05f, 0.12f, 0.3f};
        if (p.state == PState::Downed) tint = Color{170, 150, 150, 255};
        if (p.state == PState::Dead) tint = Color{110, 100, 100, 255};
        if (p.state == PState::Dodge || p.hp.iframes > 0.f) {
            // Brief blink while invulnerable after a hit.
            if (p.state != PState::Dodge && std::fmod(w.time, 0.16f) < 0.05f) tint.a = 255, em = Vector3Add(em, Vector3{0.15f, 0.15f, 0.15f});
        }
        p.vis.draw(r, pos, p.k.lerpYaw(alpha), p.hp.flash > 0.f ? 1.f : 0.f, tint, em);
        r.shadow(pos, 0.9f, 0.5f);
        // Player ring in slot color; revive progress ring under downed players.
        Color sc = slotColor(p.slot);
        sc.a = 150;
        r.decal(pos, 1.0f, sc, eng::DecalShape::Ring);
        if (p.state == PState::Downed) {
            const float k = 0.5f + 0.5f * std::sin(w.time * 6.f);
            r.decal(pos, w.cfg("revive", "radius"), Color{255, 80, 70, static_cast<unsigned char>(80 + 60 * k)}, eng::DecalShape::Ring);
        }
    }
    for (Enemy& e : w.enemies) {
        Vector3 pos = e.k.lerpPos(alpha);
        if (e.state == EState::Dead && e.deadTime > 1.8f) pos.y -= (e.deadTime - 1.8f) * 1.2f;  // sink away
        const float glow = telegraphGlow(e);
        const Vector3 em = e.def->behavior == "caster" ? Vector3{0.2f * glow, 0.9f * glow, 0.2f * glow}
                                                       : Vector3{1.0f * glow, 0.1f * glow, 0.05f * glow};
        e.vis.draw(r, pos, e.k.lerpYaw(alpha), e.hp.flash > 0.f ? 1.f : 0.f, e.def->tint, em);
        if (e.state != EState::Dead) r.shadow(pos, e.k.radius * 1.7f, 0.5f);
        // Telegraph decals: lunge lane, wind-up warning.
        if (e.state == EState::Telegraph) {
            for (int i = 1; i <= 6; ++i)
                r.decal(Vector3Add(pos, Vector3Scale(e.lungeDir, static_cast<float>(i) * 1.2f)), 0.7f,
                        Color{255, 40, 30, static_cast<unsigned char>(110 + 80 * glow)}, eng::DecalShape::Disc);
        }
    }
    const eng::ModelAsset* chestModel = app_.assets().find("assets/kaykit_dungeon/models/trunk_large_A.glb");
    for (const Chest& c : w.chests) {
        if (!chestModel) break;
        // Pop in, then hop when opened.
        const float pop = std::min(1.f, c.age * 3.f);
        const float hop = c.opened ? std::max(0.f, std::sin(std::min(c.openAge, 0.5f) * PI * 2.f)) * 0.4f : 0.f;
        const float sc = 1.7f * pop * (1.f + (c.opened ? 0.f : 0.05f * std::sin(c.age * 5.f)));
        r.drawModel(*chestModel,
                    MatrixMultiply(MatrixMultiply(MatrixScale(sc, sc, sc), MatrixRotateY(c.yaw)), MatrixTranslate(c.pos.x, hop, c.pos.z)),
                    eng::DrawParams{nullptr, 0, WHITE, 0.f, c.opened ? Vector3{0, 0, 0} : Vector3{0.25f, 0.18f, 0.f}, 0});
        r.shadow(c.pos, 1.2f, 0.5f);
        if (!c.opened && !c.autoOpen) r.decal(c.pos, 2.6f, Color{255, 210, 90, 90}, eng::DecalShape::Ring);
    }
    const eng::ModelAsset* coin = app_.assets().find("assets/kaykit_dungeon/models/coin.glb");
    for (const GoldPickup& g : w.gold) {
        if (coin) r.drawModel(*coin, MatrixMultiply(MatrixMultiply(MatrixScale(1.6f, 1.6f, 1.6f), MatrixRotateY(g.age * 6.f)),
                                                     MatrixTranslate(g.pos.x, g.pos.y, g.pos.z)),
                              eng::DrawParams{nullptr, 0, WHITE, 0.f, Vector3{0.35f, 0.25f, 0.f}, 0});
        r.shadow(Vector3{g.pos.x, 0, g.pos.z}, 0.3f, 0.4f);
    }
    for (const Hazard& h : w.hazards) {
        if (h.kind == Hazard::Kind::ShockRing) {
            r.decal(h.pos, h.radius, Color{255, 120, 60, 220}, eng::DecalShape::Ring);
        } else if (h.warmup > 0.f) {
            r.decal(h.pos, h.radius, Color{255, 40, 30, 160}, eng::DecalShape::Disc);
        } else {
            r.decal(h.pos, h.radius, Color{255, 120, 30, 90}, eng::DecalShape::Disc);
        }
    }
    for (const Projectile& pr : w.projectiles)
        if (pr.kind == ProjKind::Bomb)
            r.decal(Vector3{pr.pos.x, 0, pr.pos.z}, pr.radius, Color{255, 60, 30, 140}, eng::DecalShape::Ring);
    r.flushDecals();
    app_.particles().draw(r);
    r.endScene();
}

void RunScene::render(float alpha) {
    World& w = *world_;
    renderWorld(alpha);
    app_.renderer().present(fade_);
    const eng::Ui& ui = app_.ui();
    drawFloatingTexts(ui, app_.renderer(), w);
    HudInfo info;
    info.floor = app_.state().floor;
    info.room = plan_.rooms[static_cast<size_t>(current_)].branch ? 0 : current_ + 1;
    info.rooms = plan_.mainLength();
    info.floorName = w.theme->name.c_str();
    info.roomType = roomTypeName(plan_.rooms[static_cast<size_t>(current_)].type);
    info.goldPulse = goldPulse_;
    info.banner = bannerTime_ > 0.f ? banner_.c_str() : nullptr;
    info.bannerAlpha = std::min(1.f, bannerTime_);
    drawHud(ui, app_.renderer(), w, app_.input(), info);
    picker_.draw(ui, w, app_.input());
    if (paused_ || disconnectedSlot_ >= 0) drawPause();
}

}  // namespace game
