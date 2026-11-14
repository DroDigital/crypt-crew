#pragma once
// Thin wrapper around Jolt: static box colliders for rooms and JPH::CharacterVirtual for
// every character (players, enemies). Jolt types stay inside physics_world.cpp.
#include <raylib.h>

#include <memory>

namespace eng {

struct PhysicsImpl;

class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    bool init();
    void shutdown();

    // Static geometry (rebuilt per room).
    void clearStatic();
    // Returns a handle usable with removeStatic (e.g. doors that open).
    int addStaticBox(Vector3 center, Vector3 halfExtents, float yawRad = 0.f);
    void removeStatic(int handle);
    void finalizeStatic();  // optimize broadphase after adding boxes

    // Characters: capsule standing on its feet at `feet`.
    int createCharacter(Vector3 feet, float radius, float height);
    void removeCharacter(int id);
    void clearCharacters();
    // Moves the character with the given horizontal velocity (gravity handled here). Returns feet position.
    Vector3 moveCharacter(int id, Vector3 velocity, float dt);
    void setCharacterPosition(int id, Vector3 feet);
    Vector3 characterPosition(int id) const;
    void setCharacterCollides(int id, bool collidesWithCharacters);

    // Ray against static geometry. Returns true and the hit fraction (0..1) along from->to.
    bool raycastStatic(Vector3 from, Vector3 to, float* fraction) const;
    int staticBodyCount() const;

private:
    std::unique_ptr<PhysicsImpl> impl_;
};

}  // namespace eng
