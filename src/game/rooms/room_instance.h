#pragma once
// A room built from a template: draw list, static colliders, nav grid, lights, doors.
#include <array>
#include <string>
#include <vector>

#include "engine/assets/asset_cache.h"
#include "engine/nav/nav_grid.h"
#include "engine/physics/physics_world.h"
#include "engine/render/renderer.h"
#include "game/rooms/room_template.h"

namespace game {

struct FloorTheme;

struct RoomDraw {
    const eng::ModelAsset* model = nullptr;
    Matrix xf{};
    Vector3 center{};  // for light selection / culling
};

struct DoorInstance {
    bool socket = false;     // template has a socket here
    bool connected = false;  // leads to another room (otherwise drawn as wall)
    bool open = false;
    float openT = 0.f;       // 0 closed .. 1 open (gate sinks into the floor)
    Vector3 center{};        // edge midpoint at floor height
    Vector3 inward{};        // unit vector into the room
    float yaw = 0.f;
    int collider = -1;
};

struct RoomInstance {
    const RoomTemplate* tpl = nullptr;
    float tile = 4.f;
    std::vector<RoomDraw> statics;
    std::array<DoorInstance, 4> doors{};
    std::vector<eng::PointLight> lights;
    eng::NavGrid nav;
    Vector3 boundsMin{}, boundsMax{};
    const eng::ModelAsset* gateModel = nullptr;
    const eng::ModelAsset* doorFrameModel = nullptr;
    const eng::ModelAsset* wallModel = nullptr;
    const eng::ModelAsset* lowWallModel = nullptr;

    Vector3 tileCenter(int tx, int tz) const;
    // Spawn point just inside a door.
    Vector3 doorEntry(Side s, float depth) const;
    void setDoorOpen(Side s, bool open, eng::PhysicsWorld& phys);
    void updateDoors(float dt);
    void draw(eng::Renderer& r) const;
    bool walkable(Vector3 p) const { return nav.walkable(p); }
};

// Builds meshes, colliders, nav grid and lights. `connected` marks which door sockets lead
// somewhere (others become walls). Clears the physics world's static bodies first.
void buildRoom(const RoomTemplate& tpl, const FloorTheme& theme, eng::AssetCache& assets, eng::PhysicsWorld& phys,
               const std::array<bool, 4>& connected, RoomInstance& out);

std::string dungeonModelPath(const std::string& name);

}  // namespace game
