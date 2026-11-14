#include "engine/physics/physics_world.h"

// Jolt must be included first.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <cstdarg>
#include <cstdio>
#include <vector>

#include "engine/core/log.h"

namespace eng {
namespace {

namespace Layers {
constexpr JPH::ObjectLayer kStatic = 0;
constexpr JPH::ObjectLayer kMoving = 1;
constexpr JPH::ObjectLayer kCount = 2;
}  // namespace Layers
namespace BPLayers {
constexpr JPH::BroadPhaseLayer kStatic(0);
constexpr JPH::BroadPhaseLayer kMoving(1);
constexpr unsigned kCount = 2;
}  // namespace BPLayers

class BPLayerInterface final : public JPH::BroadPhaseLayerInterface {
public:
    unsigned GetNumBroadPhaseLayers() const override { return BPLayers::kCount; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer l) const override {
        return l == Layers::kStatic ? BPLayers::kStatic : BPLayers::kMoving;
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer l) const override {
        return l == BPLayers::kStatic ? "static" : "moving";
    }
#endif
};

class ObjVsBPFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::BroadPhaseLayer b) const override {
        return a == Layers::kMoving || b == BPLayers::kMoving;
    }
};

class ObjPairFilter final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        return a == Layers::kMoving || b == Layers::kMoving;
    }
};

void traceImpl(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[512];
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    log::info("jolt: %s", buf);
}

#ifdef JPH_ENABLE_ASSERTS
bool assertFailed(const char* expr, const char* msg, const char* file, unsigned line) {
    log::error("jolt assert %s (%s) at %s:%u", expr, msg ? msg : "", file, line);
    return false;  // don't break
}
#endif

int gJoltRefs = 0;

JPH::Vec3 v3(Vector3 v) { return JPH::Vec3(v.x, v.y, v.z); }

}  // namespace

struct PhysicsImpl {
    BPLayerInterface bpInterface;
    ObjVsBPFilter objVsBp;
    ObjPairFilter objPair;
    std::unique_ptr<JPH::TempAllocatorImpl> temp;
    std::unique_ptr<JPH::PhysicsSystem> system;
    std::vector<JPH::BodyID> statics;
    struct Char {
        JPH::Ref<JPH::CharacterVirtual> ch;
        float verticalVel = 0.f;
        bool alive = false;
    };
    std::vector<Char> chars;
    JPH::CharacterVsCharacterCollisionSimple charVsChar;
};

PhysicsWorld::PhysicsWorld() = default;
PhysicsWorld::~PhysicsWorld() { shutdown(); }

bool PhysicsWorld::init() {
    if (impl_) return true;
    if (gJoltRefs++ == 0) {
        JPH::RegisterDefaultAllocator();
        JPH::Trace = traceImpl;
        JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = assertFailed;)
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
    }
    impl_ = std::make_unique<PhysicsImpl>();
    impl_->temp = std::make_unique<JPH::TempAllocatorImpl>(8 * 1024 * 1024);
    impl_->system = std::make_unique<JPH::PhysicsSystem>();
    impl_->system->Init(4096, 0, 4096, 2048, impl_->bpInterface, impl_->objVsBp, impl_->objPair);
    impl_->system->SetGravity(JPH::Vec3(0, -25.f, 0));
    return true;
}

void PhysicsWorld::shutdown() {
    if (!impl_) return;
    clearCharacters();
    clearStatic();
    impl_.reset();
    if (--gJoltRefs == 0) {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}

void PhysicsWorld::clearStatic() {
    if (!impl_) return;
    JPH::BodyInterface& bi = impl_->system->GetBodyInterface();
    for (JPH::BodyID id : impl_->statics) {
        if (id.IsInvalid()) continue;
        bi.RemoveBody(id);
        bi.DestroyBody(id);
    }
    impl_->statics.clear();
}

int PhysicsWorld::addStaticBox(Vector3 c, Vector3 he, float yaw) {
    if (!impl_) return -1;
    JPH::BodyInterface& bi = impl_->system->GetBodyInterface();
    JPH::BoxShapeSettings shape(JPH::Vec3(std::max(he.x, 0.05f), std::max(he.y, 0.05f), std::max(he.z, 0.05f)), 0.02f);
    auto res = shape.Create();
    if (res.HasError()) {
        log::error("jolt box: %s", res.GetError().c_str());
        return -1;
    }
    JPH::BodyCreationSettings bcs(res.Get(), JPH::RVec3(c.x, c.y, c.z), JPH::Quat::sRotation(JPH::Vec3::sAxisY(), yaw),
                                  JPH::EMotionType::Static, Layers::kStatic);
    JPH::BodyID id = bi.CreateAndAddBody(bcs, JPH::EActivation::DontActivate);
    if (id.IsInvalid()) return -1;
    impl_->statics.push_back(id);
    return static_cast<int>(impl_->statics.size()) - 1;
}

void PhysicsWorld::removeStatic(int handle) {
    if (!impl_ || handle < 0 || handle >= static_cast<int>(impl_->statics.size())) return;
    JPH::BodyID& id = impl_->statics[static_cast<size_t>(handle)];
    if (id.IsInvalid()) return;
    JPH::BodyInterface& bi = impl_->system->GetBodyInterface();
    bi.RemoveBody(id);
    bi.DestroyBody(id);
    id = JPH::BodyID();
}

void PhysicsWorld::finalizeStatic() {
    if (impl_) impl_->system->OptimizeBroadPhase();
}

int PhysicsWorld::staticBodyCount() const { return impl_ ? static_cast<int>(impl_->statics.size()) : 0; }

int PhysicsWorld::createCharacter(Vector3 feet, float radius, float height) {
    if (!impl_) return -1;
    const float half = std::max(height * 0.5f - radius, 0.05f);
    JPH::Ref<JPH::Shape> capsule =
        JPH::RotatedTranslatedShapeSettings(JPH::Vec3(0, half + radius, 0), JPH::Quat::sIdentity(),
                                            new JPH::CapsuleShape(half, radius))
            .Create()
            .Get();
    JPH::Ref<JPH::CharacterVirtualSettings> s = new JPH::CharacterVirtualSettings();
    s->mShape = capsule;
    s->mMaxSlopeAngle = JPH::DegreesToRadians(50.f);
    s->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -radius);
    s->mCharacterPadding = 0.02f;
    s->mPenetrationRecoverySpeed = 1.0f;
    PhysicsImpl::Char c;
    c.ch = new JPH::CharacterVirtual(s, JPH::RVec3(feet.x, feet.y, feet.z), JPH::Quat::sIdentity(), 0,
                                     impl_->system.get());
    c.ch->SetCharacterVsCharacterCollision(&impl_->charVsChar);
    impl_->charVsChar.Add(c.ch);
    c.alive = true;
    for (size_t i = 0; i < impl_->chars.size(); ++i)
        if (!impl_->chars[i].alive) {
            impl_->chars[i] = c;
            return static_cast<int>(i);
        }
    impl_->chars.push_back(c);
    return static_cast<int>(impl_->chars.size()) - 1;
}

void PhysicsWorld::removeCharacter(int id) {
    if (!impl_ || id < 0 || id >= static_cast<int>(impl_->chars.size())) return;
    auto& c = impl_->chars[static_cast<size_t>(id)];
    if (!c.alive) return;
    impl_->charVsChar.Remove(c.ch);
    c.ch = nullptr;
    c.alive = false;
}

void PhysicsWorld::clearCharacters() {
    if (!impl_) return;
    for (size_t i = 0; i < impl_->chars.size(); ++i) removeCharacter(static_cast<int>(i));
    impl_->chars.clear();
}

void PhysicsWorld::setCharacterCollides(int id, bool collides) {
    if (!impl_ || id < 0 || id >= static_cast<int>(impl_->chars.size())) return;
    auto& c = impl_->chars[static_cast<size_t>(id)];
    if (!c.alive) return;
    impl_->charVsChar.Remove(c.ch);
    if (collides) impl_->charVsChar.Add(c.ch);
}

Vector3 PhysicsWorld::moveCharacter(int id, Vector3 vel, float dt) {
    if (!impl_ || id < 0 || id >= static_cast<int>(impl_->chars.size())) return Vector3{0, 0, 0};
    auto& c = impl_->chars[static_cast<size_t>(id)];
    if (!c.alive) return Vector3{0, 0, 0};
    const JPH::Vec3 g = impl_->system->GetGravity();
    if (c.ch->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround) c.verticalVel = 0.f;
    else c.verticalVel += g.GetY() * dt;
    c.ch->SetLinearVelocity(JPH::Vec3(vel.x, c.verticalVel, vel.z));
    const auto bpFilter = impl_->system->GetDefaultBroadPhaseLayerFilter(Layers::kMoving);
    const auto objFilter = impl_->system->GetDefaultLayerFilter(Layers::kMoving);
    c.ch->Update(dt, g, bpFilter, objFilter, {}, {}, *impl_->temp);
    JPH::RVec3 p = c.ch->GetPosition();
    // Safety net: never fall through the floor.
    if (p.GetY() < -0.5f) {
        p.SetY(0.f);
        c.ch->SetPosition(p);
        c.verticalVel = 0.f;
    }
    return Vector3{static_cast<float>(p.GetX()), static_cast<float>(p.GetY()), static_cast<float>(p.GetZ())};
}

void PhysicsWorld::setCharacterPosition(int id, Vector3 feet) {
    if (!impl_ || id < 0 || id >= static_cast<int>(impl_->chars.size())) return;
    auto& c = impl_->chars[static_cast<size_t>(id)];
    if (c.alive) {
        c.ch->SetPosition(JPH::RVec3(feet.x, feet.y, feet.z));
        c.verticalVel = 0.f;
    }
}

Vector3 PhysicsWorld::characterPosition(int id) const {
    if (!impl_ || id < 0 || id >= static_cast<int>(impl_->chars.size())) return Vector3{0, 0, 0};
    const auto& c = impl_->chars[static_cast<size_t>(id)];
    if (!c.alive) return Vector3{0, 0, 0};
    JPH::RVec3 p = c.ch->GetPosition();
    return Vector3{static_cast<float>(p.GetX()), static_cast<float>(p.GetY()), static_cast<float>(p.GetZ())};
}

bool PhysicsWorld::raycastStatic(Vector3 from, Vector3 to, float* fraction) const {
    if (!impl_) return false;
    JPH::RRayCast ray(JPH::RVec3(from.x, from.y, from.z), v3(Vector3{to.x - from.x, to.y - from.y, to.z - from.z}));
    JPH::RayCastResult hit;
    const auto bp = impl_->system->GetDefaultBroadPhaseLayerFilter(Layers::kMoving);
    const auto obj = impl_->system->GetDefaultLayerFilter(Layers::kMoving);
    if (impl_->system->GetNarrowPhaseQuery().CastRay(ray, hit, bp, obj)) {
        if (fraction) *fraction = hit.mFraction;
        return true;
    }
    return false;
}

}  // namespace eng
