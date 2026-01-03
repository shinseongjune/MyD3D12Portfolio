#pragma once
#include "World.h"
#include "PhysicsTypes.h"
#include <unordered_map>
#include <cstdint>
#include <vector>
#include <utility>

class PhysicsSystem
{
public:
    void Step(World& world, float dt);

    void SetGravity(const XMFLOAT3& g) { m_gravity = g; }
    void SetIterations(int it) { m_iterations = it; }

    void SetGravityEnabled(bool enabled) { m_gravityEnabled = enabled; }
    bool IsGravityEnabled() const { return m_gravityEnabled; }

	// Raycast
    struct RaycastHit
    {
        EntityId entity = EntityId::Invalid();
        float t = 0.0f;                 // ray parameter (distance along dir if dir normalized)
        DirectX::XMFLOAT3 point{ 0,0,0 };
        DirectX::XMFLOAT3 normal{ 0,0,0 };
        bool isTrigger = false;
    };

    bool Raycast(const World& world, const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dirNormalized, float maxDist, RaycastHit& outHit, uint32_t collideMask = ~0u, bool hitTriggers = false) const;

	// Overlap Sphere
    int OverlapSphere(const World& world, const DirectX::XMFLOAT3& center, float radius, std::vector<EntityId>& outHits, uint32_t collideMask = ~0u, bool includeTriggers = true) const;

private:
    XMFLOAT3 m_gravity{ 0.0f, -9.81f, 0.0f };
    int m_iterations = 10; // solver 반복
    bool m_gravityEnabled = true;

    // --- pipeline stages ---
    void Integrate(World& world, float dt);
    void BuildPairs(World& world, std::vector<std::pair<EntityId, EntityId>>& outPairs);
    void Narrowphase(World& world, const std::vector<std::pair<EntityId, EntityId>>& pairs, std::vector<std::tuple<EntityId, EntityId, Contact>>& outContacts);

    void Solve(World& world, std::vector<std::tuple<EntityId, EntityId, Contact>>& contacts, float dt);

    // --- helpers ---
    AABB ComputeWorldAABB(const World& world, EntityId e) const;
    bool LayerMatch(const ColliderComponent& a, const ColliderComponent& b) const;

    bool Collide_SphereSphere(const World& world, EntityId a, EntityId b, Contact& out) const;
    bool Collide_AABBAABB(const AABB& a, const AABB& b, Contact& out) const;
    bool Collide_SphereAABB(const XMFLOAT3& c, float r, const AABB& b, Contact& out) const;

    // Collision Events
    std::unordered_map<uint64_t, std::pair<EntityId, EntityId>> m_prevPairs;

    void EmitCollisionEvents(World& world, const std::vector<std::tuple<EntityId, EntityId, Contact>>& contacts);

	// Sequencial Impulses용 컨택트 캐시
    struct CachedContact
    {
        XMFLOAT3 normal{};
        XMFLOAT3 point{};
        float normalImpulseSum = 0.0f;
        float tangentImpulseSum = 0.0f;
    };

    std::unordered_map<uint64_t, CachedContact> m_contactCache;

    void WarmStart(World& world, std::vector<std::tuple<EntityId, EntityId, Contact>>& contacts);
    void StoreContactCache(const std::vector<std::tuple<EntityId, EntityId, Contact>>& contacts);
    void UpdateSleep(World& world, float dt);

private:
    void DebugDrawColliders(World& world, const std::vector<std::tuple<EntityId, EntityId, Contact>>& contacts);
    void DrawAABB(const AABB& aabb, const DirectX::XMFLOAT4& color);
};