#include "PhysicsSystem.h"
#include "DebugDraw.h"
#include "CollisionEvents.h"
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include <unordered_set>

using namespace DirectX;

// Helpers
static inline XMFLOAT3 Add(const XMFLOAT3& a, const XMFLOAT3& b) { return { a.x + b.x,a.y + b.y,a.z + b.z }; }
static inline XMFLOAT3 Sub(const XMFLOAT3& a, const XMFLOAT3& b) { return { a.x - b.x,a.y - b.y,a.z - b.z }; }
static inline XMFLOAT3 Mul(const XMFLOAT3& a, float s) { return { a.x * s,a.y * s,a.z * s }; }
static inline float Dot(const XMFLOAT3& a, const XMFLOAT3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

static inline float Len3(const XMFLOAT3& v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static inline XMFLOAT3 NormalizeSafe(const XMFLOAT3& v, const XMFLOAT3& fallback = { 0,0,0 })
{
    const float l = Len3(v);
    if (l <= 1e-6f) return fallback;
    return { v.x / l, v.y / l, v.z / l };
}

static inline XMFLOAT3 Clamp3(const XMFLOAT3& v, const XMFLOAT3& mn, const XMFLOAT3& mx)
{
    return {
        std::max(mn.x, std::min(v.x, mx.x)),
        std::max(mn.y, std::min(v.y, mx.y)),
        std::max(mn.z, std::min(v.z, mx.z))
    };
}

static inline float Clamp(float x, float mn, float mx)
{
    return std::max(mn, std::min(x, mx));
}

static inline void SortPair(EntityId& a, EntityId& b)
{
    if (b.index < a.index || (b.index == a.index && b.generation < a.generation))
        std::swap(a, b);
}

static inline uint64_t Pack(EntityId e)
{
    return (uint64_t(e.index) << 32) | uint64_t(e.generation);
}

static inline uint64_t MakePairKey(EntityId a, EntityId b)
{
    SortPair(a, b);
    // 간단/충분: A ^ (B * 큰 상수)
    return Pack(a) ^ (Pack(b) * 0x9E3779B97F4A7C15ull);
}

static inline void GetSphereWorld_RowVector(
    const World& world, EntityId e,
    DirectX::XMFLOAT3& outCenter, float& outRadius)
{
    using namespace DirectX;

    const ColliderComponent& col = world.GetCollider(e);
    XMFLOAT4X4 m = world.GetWorldMatrix(e);

    XMFLOAT3 t{ m._41, m._42, m._43 };
    XMFLOAT3 row0{ m._11, m._12, m._13 };
    XMFLOAT3 row1{ m._21, m._22, m._23 };
    XMFLOAT3 row2{ m._31, m._32, m._33 };

    // centerWorld = t + cx*row0 + cy*row1 + cz*row2
    XMFLOAT3 c = col.localCenter;
    outCenter = {
        t.x + c.x * row0.x + c.y * row1.x + c.z * row2.x,
        t.y + c.x * row0.y + c.y * row1.y + c.z * row2.y,
        t.z + c.x * row0.z + c.y * row1.z + c.z * row2.z
    };

    float sx = Len3(row0);
    float sy = Len3(row1);
    float sz = Len3(row2);
    outRadius = col.sphere.radius * std::max(sx, std::max(sy, sz));
}

static void Wake(World& world, EntityId e)
{
    if (!world.HasRigidBody(e)) return;
    auto& rb = world.GetRigidBody(e);
    if (rb.type != BodyType::Dynamic) return;

    rb.isAwake = true;
    rb.sleepTimer = 0.0f;
}

static void PutToSleep(World& world, EntityId e)
{
    if (!world.HasRigidBody(e)) return;
    auto& rb = world.GetRigidBody(e);
    if (rb.type != BodyType::Dynamic) return;

    rb.isAwake = false;
    rb.sleepTimer = 0.0f;
    rb.velocity = { 0,0,0 };
}

static XMFLOAT3 Cross3(const XMFLOAT3& a, const XMFLOAT3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static XMFLOAT3 OrthoTangentFromNormal(const XMFLOAT3& n)
{
    // n과 거의 평행하지 않은 축을 골라 cross로 안정적인 tangent 생성
    XMFLOAT3 a = (std::fabs(n.y) < 0.9f) ? XMFLOAT3{ 0,1,0 } : XMFLOAT3{ 1,0,0 };
    XMFLOAT3 t = Cross3(a, n);
    float l2 = Dot(t, t);
    if (l2 < 1e-12f) return { 1,0,0 }; // 마지막 안전망
    return Mul(t, 1.0f / std::sqrt(l2));
}

static bool RaySphere(const DirectX::XMFLOAT3& ro, const DirectX::XMFLOAT3& rdN, float maxDist, const DirectX::XMFLOAT3& c, float r, float& outT, DirectX::XMFLOAT3& outN)
{
    // Solve |ro + t*rd - c|^2 = r^2
    DirectX::XMFLOAT3 oc{ ro.x - c.x, ro.y - c.y, ro.z - c.z };
    float b = oc.x * rdN.x + oc.y * rdN.y + oc.z * rdN.z;
    float cTerm = oc.x * oc.x + oc.y * oc.y + oc.z * oc.z - r * r;
    float disc = b * b - cTerm; // because a=1
    if (disc < 0.0f) return false;

    float s = std::sqrt(disc);
    float t0 = -b - s;
    float t1 = -b + s;

    float t = (t0 >= 0.0f) ? t0 : t1;
    if (t < 0.0f || t > maxDist) return false;

    outT = t;
    DirectX::XMFLOAT3 p{ ro.x + rdN.x * t, ro.y + rdN.y * t, ro.z + rdN.z * t };
    DirectX::XMFLOAT3 n{ p.x - c.x, p.y - c.y, p.z - c.z };
    float l2 = n.x * n.x + n.y * n.y + n.z * n.z;
    if (l2 > 1e-12f)
    {
        float invL = 1.0f / std::sqrt(l2);
        outN = { n.x * invL, n.y * invL, n.z * invL };
    }
    else outN = { 0,1,0 };
    return true;
}

static bool RayAABB(const DirectX::XMFLOAT3& ro, const DirectX::XMFLOAT3& rdN, float maxDist, const AABB& b, float& outT, DirectX::XMFLOAT3& outN)
{
    // slab
    float tmin = 0.0f;
    float tmax = maxDist;
    DirectX::XMFLOAT3 nEnter{ 0,0,0 };

    auto axis = [&](float roA, float rdA, float minA, float maxA,
        DirectX::XMFLOAT3 nNeg, DirectX::XMFLOAT3 nPos) -> bool
        {
            if (std::fabs(rdA) < 1e-8f)
            {
                // parallel: must be within slab
                return (roA >= minA && roA <= maxA);
            }
            float inv = 1.0f / rdA;
            float t1 = (minA - roA) * inv;
            float t2 = (maxA - roA) * inv;
            DirectX::XMFLOAT3 n1 = nNeg;
            DirectX::XMFLOAT3 n2 = nPos;
            if (t1 > t2) { std::swap(t1, t2); std::swap(n1, n2); }

            if (t1 > tmin) { tmin = t1; nEnter = n1; }
            if (t2 < tmax) { tmax = t2; }
            return tmin <= tmax;
        };

    if (!axis(ro.x, rdN.x, b.min.x, b.max.x, { -1,0,0 }, { 1,0,0 })) return false;
    if (!axis(ro.y, rdN.y, b.min.y, b.max.y, { 0,-1,0 }, { 0, 1,0 })) return false;
    if (!axis(ro.z, rdN.z, b.min.z, b.max.z, { 0,0,-1 }, { 0,0, 1 })) return false;

    if (tmin < 0.0f || tmin > maxDist) return false;
    outT = tmin;
    outN = nEnter;
    return true;
}

// Integration
void PhysicsSystem::Step(World& world, float dt)
{
    // 1) Integrate (forces -> velocity -> position)
    Integrate(world, dt);
    world.UpdateTransforms(); // 충돌에 필요한 world matrix 최신화

    // 2) Broadphase pairs
    std::vector<std::pair<EntityId, EntityId>> pairs;
    BuildPairs(world, pairs);

    // 3) Narrowphase contacts
    std::vector<std::tuple<EntityId, EntityId, Contact>> contacts;
    Narrowphase(world, pairs, contacts);

	// 4) Warm Start : 전 프레임 누적 임펄스를 속도에 미리 적용
    WarmStart(world, contacts);

    // 5) Solve (penetration + velocity response)
    Solve(world, contacts, dt);
	world.UpdateTransforms(); // 충돌 후 위치 보정 등으로 world matrix 다시 최신화

    // 6) 이번 프레임 contact 누적값을 캐시에 저장(Exit는 자동으로 떨어짐)
    StoreContactCache(contacts);

	// 7) Collision Events
    EmitCollisionEvents(world, contacts);

	DebugDrawColliders(world, contacts); // 디버그 드로우

	// 8) Sleep 업데이트
    UpdateSleep(world, dt);
}

void PhysicsSystem::Integrate(World& world, float dt)
{
    const auto& cols = world.GetColliderEntities();
    for (EntityId e : cols)
    {
        if (!world.HasRigidBody(e) || !world.HasTransform(e)) continue;

        auto& rb = world.GetRigidBody(e);
        if (rb.type != BodyType::Dynamic) continue;
		if (!rb.isAwake) continue; // 잠자기 상태

        // gravity
        if (rb.useGravity && m_gravityEnabled)
            rb.velocity = Add(rb.velocity, Mul(m_gravity, rb.gravityScale * dt));

        // damping
        rb.velocity = Mul(rb.velocity, std::max(0.0f, 1.0f - rb.linearDamping));

        // position update (semi-implicit Euler)
        XMFLOAT3 p = world.GetLocalPosition(e);
        p = Add(p, Mul(rb.velocity, dt));
        world.SetLocalPosition(e, p); // dirty 처리 포함
    }
}

// Broadphase
void PhysicsSystem::BuildPairs(World& world, std::vector<std::pair<EntityId, EntityId>>& outPairs)
{
    outPairs.clear();

    const auto& ents = world.GetColliderEntities();
    const size_t n = ents.size();

    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 1; j < n; ++j)
        {
            EntityId a = ents[i];
            EntityId b = ents[j];
            if (!world.HasCollider(a) || !world.HasCollider(b)) continue;

            const auto& ca = world.GetCollider(a);
            const auto& cb = world.GetCollider(b);

            if (!LayerMatch(ca, cb)) continue;

            // 둘 다 Static이면 굳이 처리할 필요 없음(트리거는 예외로 할 수도)
            bool aDyn = world.HasRigidBody(a) && world.GetRigidBody(a).type == BodyType::Dynamic;
            bool bDyn = world.HasRigidBody(b) && world.GetRigidBody(b).type == BodyType::Dynamic;
            if (!aDyn && !bDyn && !(ca.isTrigger || cb.isTrigger)) continue;

            // 둘 다 dynamic인데 둘 다 sleep이면 후보에서 제외
            if (aDyn && bDyn && !(ca.isTrigger || cb.isTrigger))
            {
                auto& ra = world.GetRigidBody(a);
                auto& rb = world.GetRigidBody(b);
                if (!ra.isAwake && !rb.isAwake)
                    continue;
            }

            // AABB로 대충 걸러도 되고, Phase1은 생략 가능
            outPairs.push_back({ a,b });
        }
}

bool PhysicsSystem::LayerMatch(const ColliderComponent& a, const ColliderComponent& b) const
{
    const uint32_t bitA = 1u << a.layer;
    const uint32_t bitB = 1u << b.layer;
    return (a.collideMask & bitB) && (b.collideMask & bitA);
}

// Narrowphase
void PhysicsSystem::Narrowphase(World& world, const std::vector<std::pair<EntityId, EntityId>>& pairs, std::vector<std::tuple<EntityId, EntityId, Contact>>& outContacts)
{
    outContacts.clear();

    for (auto [a, b] : pairs)
    {
        if (!world.HasTransform(a) || !world.HasTransform(b)) continue;

        const auto& ca = world.GetCollider(a);
        const auto& cb = world.GetCollider(b);

        Contact c{};
        bool hit = false;

        // Sphere-Sphere
        if (ca.shapeType == ShapeType::Sphere && cb.shapeType == ShapeType::Sphere)
        {
            hit = Collide_SphereSphere(world, a, b, c);
        }
        // Sphere-Box
        else if (ca.shapeType == ShapeType::Sphere && cb.shapeType == ShapeType::Box)
        {
            XMFLOAT3 cWorld{};
            float r = 0.0f;
            GetSphereWorld_RowVector(world, a, cWorld, r);

            AABB box = ComputeWorldAABB(world, b);

            hit = Collide_SphereAABB(cWorld, r, box, c);
        }
        // Box-Sphere
        else if (ca.shapeType == ShapeType::Box && cb.shapeType == ShapeType::Sphere)
        {
            XMFLOAT3 cWorld{};
            float r = 0.0f;
            GetSphereWorld_RowVector(world, b, cWorld, r);

            AABB box = ComputeWorldAABB(world, a);

            hit = Collide_SphereAABB(cWorld, r, box, c);
            if (hit)
                c.normal = { -c.normal.x, -c.normal.y, -c.normal.z }; // 규약 맞추기
        }
        else
        {
            // 나머지는 Phase1: AABB-AABB
            AABB aa = ComputeWorldAABB(world, a);
            AABB bb = ComputeWorldAABB(world, b);
            hit = Collide_AABBAABB(aa, bb, c);
        }

        if (hit) outContacts.push_back({ a,b,c });
    }
}

// Solve
void PhysicsSystem::Solve(World& world, std::vector<std::tuple<EntityId, EntityId, Contact>>& contacts, float dt)
{
    const int iterations = (m_iterations > 0) ? m_iterations : 10;

    // 안정화 파라미터(게임 물리 기본값 느낌)
    const float slop = 0.01f;      // 관통 허용량
    const float beta = 0.10f;  // bias 강도
    const float bounceThreshold = 2.0f; // 이보다 빠를 때만 반발 적용(바닥 떨림 방지)
	const float percent = 0.35;     // positional correction 비율(0.2~0.8)

    for (int it = 0; it < iterations; ++it)
    {
        for (auto& [a0, b0, c] : contacts)
        {
            EntityId a = a0, b = b0;

            auto& ca = world.GetCollider(a);
            auto& cb = world.GetCollider(b);

            // trigger는 밀어내지 않음(이벤트만)
            if (ca.isTrigger || cb.isTrigger)
                continue;

            const bool aDyn = world.HasRigidBody(a) && world.GetRigidBody(a).type == BodyType::Dynamic;
            const bool bDyn = world.HasRigidBody(b) && world.GetRigidBody(b).type == BodyType::Dynamic;

            float invMassA = aDyn ? world.GetRigidBody(a).invMass : 0.0f;
            float invMassB = bDyn ? world.GetRigidBody(b).invMass : 0.0f;
            float invMassSum = invMassA + invMassB;
            if (invMassSum <= 0.0f)
                continue;

            XMFLOAT3 n = c.normal;

            // 상대속도
            XMFLOAT3 vA{ 0,0,0 }, vB{ 0,0,0 };
            if (aDyn) vA = world.GetRigidBody(a).velocity;
            if (bDyn) vB = world.GetRigidBody(b).velocity;
            XMFLOAT3 vRel = Sub(vB, vA);

            float vn = Dot(vRel, n);

            // --- bias (관통 제거용 속도항) ---
            float pen = std::max(0.0f, c.penetration - slop);
            float bias = (dt > 0.0f) ? (beta * (pen / dt)) : 0.0f;
			bias = std::min(bias, 5.0f); // 너무 크지 않게 클램프

            // --- restitution(빠른 충돌에만) ---
            float e = std::min(ca.material.restitution, cb.material.restitution);
            float bounceVel = 0.0f;
            if (vn < -bounceThreshold)
                bounceVel = -e * vn;   // 목표: vn_after = -e*vn_before
            else
                bounceVel = 0.0f;

            // --- positional correction (Baumgarte / split impulse 스타일) ---
            if (pen > 0.0f)
            {
                float corrMag = (pen / invMassSum) * percent;
                corrMag = std::min(corrMag, 0.2f); // 프레임당 최대 20cm 같은 식(스케일에 맞게)
                XMFLOAT3 correction = Mul(n, corrMag);

                if (aDyn)
                {
                    XMFLOAT3 p = world.GetLocalPosition(a);
                    p = Sub(p, Mul(correction, invMassA));
                    world.SetLocalPosition(a, p);
                }
                if (bDyn)
                {
                    XMFLOAT3 p = world.GetLocalPosition(b);
                    p = Add(p, Mul(correction, invMassB));
                    world.SetLocalPosition(b, p);
                }
            }

            // ============================
            // (1) Normal impulse
            // ============================
            // 제약: vn + bias + bounceVel >= 0 이 되도록(침투/접근 방지)
            float lambdaN = -(vn + bias + bounceVel);
            lambdaN /= invMassSum;

            // 누적 & 클램프(당기는 힘 금지)
            float oldN = c.normalImpulseSum;
            c.normalImpulseSum = std::max(0.0f, oldN + lambdaN);
            float dN = c.normalImpulseSum - oldN;

            const float wakeImpulse = 0.02f;

            if (lambdaN > wakeImpulse)
            {
                Wake(world, a);
                Wake(world, b);
            }

            XMFLOAT3 Pn = Mul(n, dN);

            if (aDyn)
            {
                auto& rbA = world.GetRigidBody(a);
                rbA.velocity = Sub(rbA.velocity, Mul(Pn, invMassA));
            }
            if (bDyn)
            {
                auto& rbB = world.GetRigidBody(b);
                rbB.velocity = Add(rbB.velocity, Mul(Pn, invMassB));
            }

            // ============================
            // (2) Friction impulse (tangent)
            // ============================
            // 업데이트된 속도로 다시 vRel 계산
            vA = { 0,0,0 }; vB = { 0,0,0 };
            if (aDyn) vA = world.GetRigidBody(a).velocity;
            if (bDyn) vB = world.GetRigidBody(b).velocity;
            vRel = Sub(vB, vA);

            vn = Dot(vRel, n);
            XMFLOAT3 t = Sub(vRel, Mul(n, vn));
            if (Dot(t, t) < 1e-12f)
                t = OrthoTangentFromNormal(n);
            else
                t = NormalizeSafe(t, OrthoTangentFromNormal(n));

            float vt = Dot(vRel, t);
            if (std::fabs(vt) < 1e-6f)
            {
                c.tangentImpulseSum = 0.0f;
                continue;
            }

            float lambdaT = -(vt);
            lambdaT /= invMassSum;

            // 마찰계수(간단히 max 사용 or sqrt(곱) 사용)
            float mu = std::max(ca.material.friction, cb.material.friction);

            // 쿨롱 마찰: |λt| <= mu * λn
            float maxF = mu * c.normalImpulseSum;

            float oldT = c.tangentImpulseSum;
            c.tangentImpulseSum = Clamp(oldT + lambdaT, -maxF, +maxF);
            float dT = c.tangentImpulseSum - oldT;

            if (std::fabs(lambdaT) > wakeImpulse)
            {
                Wake(world, a);
                Wake(world, b);
            }

            XMFLOAT3 Pt = Mul(t, dT);

            if (aDyn)
            {
                auto& rbA = world.GetRigidBody(a);
                rbA.velocity = Sub(rbA.velocity, Mul(Pt, invMassA));
            }
            if (bDyn)
            {
                auto& rbB = world.GetRigidBody(b);
                rbB.velocity = Add(rbB.velocity, Mul(Pt, invMassB));
            }
        }
    }
}

AABB PhysicsSystem::ComputeWorldAABB(const World& world, EntityId e) const
{
    AABB out{};
    out.min = { 0,0,0 };
    out.max = { 0,0,0 };

    if (!world.HasTransform(e) || !world.HasCollider(e))
        return out;

    const ColliderComponent& col = world.GetCollider(e);

    // row-vector convention 기준으로 world matrix 해석:
    // - translation: 4th row (m._41, _42, _43)
    // - axis rows: row0(_11,_12,_13), row1(_21,_22,_23), row2(_31,_32,_33)
    XMFLOAT4X4 m = world.GetWorldMatrix(e);

    // world position (translation)
    XMFLOAT3 t{ m._41, m._42, m._43 };

    // localCenter를 world로 옮기기(회전/스케일 포함) - row-vector에서는 center * M 이므로
    // centerWorld = t + center.x*row0 + center.y*row1 + center.z*row2
    XMFLOAT3 row0{ m._11, m._12, m._13 };
    XMFLOAT3 row1{ m._21, m._22, m._23 };
    XMFLOAT3 row2{ m._31, m._32, m._33 };

    XMFLOAT3 c = col.localCenter;
    XMFLOAT3 cWorld{
        t.x + c.x * row0.x + c.y * row1.x + c.z * row2.x,
        t.y + c.x * row0.y + c.y * row1.y + c.z * row2.y,
        t.z + c.x * row0.z + c.y * row1.z + c.z * row2.z
    };

    // axis 길이 = 각 row의 길이 (scale 포함)
    float sx = Len3(row0);
    float sy = Len3(row1);
    float sz = Len3(row2);

    if (col.shapeType == ShapeType::Sphere)
    {
        float r = col.sphere.radius * std::max(sx, std::max(sy, sz));
        out.min = { cWorld.x - r, cWorld.y - r, cWorld.z - r };
        out.max = { cWorld.x + r, cWorld.y + r, cWorld.z + r };
        return out;
    }
    else
    {
        // AABB(OBB 근사): extWorld = |row0|*ex + |row1|*ey + |row2|*ez
        XMFLOAT3 ex = col.box.halfExtents;

        XMFLOAT3 a0{ std::abs(row0.x), std::abs(row0.y), std::abs(row0.z) };
        XMFLOAT3 a1{ std::abs(row1.x), std::abs(row1.y), std::abs(row1.z) };
        XMFLOAT3 a2{ std::abs(row2.x), std::abs(row2.y), std::abs(row2.z) };

        XMFLOAT3 eWorld{
            a0.x * ex.x + a1.x * ex.y + a2.x * ex.z,
            a0.y * ex.x + a1.y * ex.y + a2.y * ex.z,
            a0.z * ex.x + a1.z * ex.y + a2.z * ex.z
        };

        out.min = { cWorld.x - eWorld.x, cWorld.y - eWorld.y, cWorld.z - eWorld.z };
        out.max = { cWorld.x + eWorld.x, cWorld.y + eWorld.y, cWorld.z + eWorld.z };
        return out;
    }
}

bool PhysicsSystem::Collide_SphereSphere(const World& world, EntityId a, EntityId b, Contact& out) const
{
    XMFLOAT3 aC{}, bC{};
    float ra = 0.0f, rb = 0.0f;

    GetSphereWorld_RowVector(world, a, aC, ra);
    GetSphereWorld_RowVector(world, b, bC, rb);

    XMFLOAT3 d = Sub(bC, aC);
    float dist2 = Dot(d, d);
    float rSum = ra + rb;
    if (dist2 >= rSum * rSum) return false;

    float dist = std::sqrt(std::max(dist2, 0.0f));
    XMFLOAT3 n = NormalizeSafe(d, { 1,0,0 });

    out.normal = n;
    out.penetration = rSum - dist;
    out.point = Add(aC, Mul(n, ra));
    return true;
}

bool PhysicsSystem::Collide_AABBAABB(const AABB& a, const AABB& b, Contact& out) const
{
    // separating axis test
    if (a.max.x < b.min.x || a.min.x > b.max.x) return false;
    if (a.max.y < b.min.y || a.min.y > b.max.y) return false;
    if (a.max.z < b.min.z || a.min.z > b.max.z) return false;

    // overlap amounts
    float ox1 = (a.max.x - b.min.x);
    float ox2 = (b.max.x - a.min.x);
    float oy1 = (a.max.y - b.min.y);
    float oy2 = (b.max.y - a.min.y);
    float oz1 = (a.max.z - b.min.z);
    float oz2 = (b.max.z - a.min.z);

    float ox = std::min(ox1, ox2);
    float oy = std::min(oy1, oy2);
    float oz = std::min(oz1, oz2);

    // choose minimum penetration axis
    out.penetration = ox;
    out.normal = { 1,0,0 };

    // centers for normal direction
    XMFLOAT3 ca{ (a.min.x + a.max.x) * 0.5f, (a.min.y + a.max.y) * 0.5f, (a.min.z + a.max.z) * 0.5f };
    XMFLOAT3 cb{ (b.min.x + b.max.x) * 0.5f, (b.min.y + b.max.y) * 0.5f, (b.min.z + b.max.z) * 0.5f };
    XMFLOAT3 d = Sub(cb, ca);

    if (oy < out.penetration) { out.penetration = oy; out.normal = { 0,1,0 }; }
    if (oz < out.penetration) { out.penetration = oz; out.normal = { 0,0,1 }; }

    // 방향 결정: normal은 A -> B (solver 규약)
    if (out.normal.x != 0) out.normal.x = (d.x >= 0) ? 1.0f : -1.0f;
    if (out.normal.y != 0) out.normal.y = (d.y >= 0) ? 1.0f : -1.0f;
    if (out.normal.z != 0) out.normal.z = (d.z >= 0) ? 1.0f : -1.0f;

    // (옵션) 접점: 두 중심의 중간쯤
    out.point = { (ca.x + cb.x) * 0.5f, (ca.y + cb.y) * 0.5f, (ca.z + cb.z) * 0.5f };
    return true;
}

bool PhysicsSystem::Collide_SphereAABB(const XMFLOAT3& c, float r, const AABB& b, Contact& out) const
{
    XMFLOAT3 closest = Clamp3(c, b.min, b.max);
    XMFLOAT3 d = Sub(closest, c);            // 공 -> 박스 (A->B)
    float dist2 = Dot(d, d);
    if (dist2 > r * r) return false;

    if (dist2 > 1e-12f)
    {
        float dist = std::sqrt(dist2);
        out.normal = Mul(d, 1.0f / dist);    // A->B
        out.penetration = r - dist;
        out.point = closest;
        return true;
    }

    // dist==0: sphere center is inside the box
    // Choose the smallest translation to move the sphere center OUT of the box by at least radius r.
    // Separation direction = dir; Contact normal n is opposite (since solver pushes A by -n).
    {
        float toMinX = (c.x - b.min.x) + r;  // move to -X outside: (b.min.x - r)
        float toMaxX = (b.max.x - c.x) + r;  // move to +X outside: (b.max.x + r)
        float toMinY = (c.y - b.min.y) + r;
        float toMaxY = (b.max.y - c.y) + r;
        float toMinZ = (c.z - b.min.z) + r;
        float toMaxZ = (b.max.z - c.z) + r;

        // pick minimum translation among 6 directions
        float best = toMinX;
        XMFLOAT3 sepDir = { -1,0,0 }; // direction to move sphere center

        if (toMaxX < best) { best = toMaxX; sepDir = { 1,0,0 }; }
        if (toMinY < best) { best = toMinY; sepDir = { 0,-1,0 }; }
        if (toMaxY < best) { best = toMaxY; sepDir = { 0, 1,0 }; }
        if (toMinZ < best) { best = toMinZ; sepDir = { 0,0,-1 }; }
        if (toMaxZ < best) { best = toMaxZ; sepDir = { 0,0, 1 }; }

        // Our solver expects n such that A is pushed by -n, so n = -sepDir
        out.normal = { -sepDir.x, -sepDir.y, -sepDir.z };
        out.penetration = best;   // how much we'd like to separate (will be clamped in correction anyway)
        out.point = closest;
        return true;
    }
}

static inline uint64_t MakeKey(EntityId e)
{
    return (uint64_t(e.index) << 32) | uint64_t(e.generation);
}

void PhysicsSystem::DebugDrawColliders(World& world, const std::vector<std::tuple<EntityId, EntityId, Contact>>& contacts)
{
    using namespace DirectX;

    // 충돌에 관여한 엔티티는 빨강으로 칠하려고 set 구성
    std::unordered_set<uint64_t> hit;
    hit.reserve(contacts.size() * 2);

    for (auto& [a, b, c] : contacts)
    {
        (void)c;
        hit.insert(MakeKey(a));
        hit.insert(MakeKey(b));

        // 컨택트 노멀(간단히)
        XMFLOAT3 p = c.point;
        XMFLOAT3 tip{ p.x + c.normal.x * 0.5f, p.y + c.normal.y * 0.5f, p.z + c.normal.z * 0.5f };
        DebugDraw::Line(p, tip, { 1,1,0,1 });
    }

    const auto& ents = world.GetColliderEntities();
    for (EntityId e : ents)
    {
        if (!world.HasCollider(e) || !world.HasTransform(e))
            continue;

        AABB aabb = ComputeWorldAABB(world, e);

        const bool isHit = hit.find(MakeKey(e)) != hit.end();
        const XMFLOAT4 color = isHit ? XMFLOAT4{ 1,0,0,1 } : XMFLOAT4{ 0,1,0,1 };

        DrawAABB(aabb, color);
    }
}

void PhysicsSystem::DrawAABB(const AABB& b, const DirectX::XMFLOAT4& c)
{
    using namespace DirectX;

    XMFLOAT3 p000{ b.min.x, b.min.y, b.min.z };
    XMFLOAT3 p001{ b.min.x, b.min.y, b.max.z };
    XMFLOAT3 p010{ b.min.x, b.max.y, b.min.z };
    XMFLOAT3 p011{ b.min.x, b.max.y, b.max.z };
    XMFLOAT3 p100{ b.max.x, b.min.y, b.min.z };
    XMFLOAT3 p101{ b.max.x, b.min.y, b.max.z };
    XMFLOAT3 p110{ b.max.x, b.max.y, b.min.z };
    XMFLOAT3 p111{ b.max.x, b.max.y, b.max.z };

    // bottom
    DebugDraw::Line(p000, p001, c);
    DebugDraw::Line(p001, p101, c);
    DebugDraw::Line(p101, p100, c);
    DebugDraw::Line(p100, p000, c);

    // top
    DebugDraw::Line(p010, p011, c);
    DebugDraw::Line(p011, p111, c);
    DebugDraw::Line(p111, p110, c);
    DebugDraw::Line(p110, p010, c);

    // verticals
    DebugDraw::Line(p000, p010, c);
    DebugDraw::Line(p001, p011, c);
    DebugDraw::Line(p100, p110, c);
    DebugDraw::Line(p101, p111, c);
}

void PhysicsSystem::EmitCollisionEvents(World& world, const std::vector<std::tuple<EntityId, EntityId, Contact>>& contacts)
{
    std::unordered_map<uint64_t, std::pair<EntityId, EntityId>> cur;
    cur.reserve(contacts.size() * 2 + 8);

    // Enter/Stay
    for (const auto& t : contacts)
    {
        EntityId a, b;
        Contact c;
        std::tie(a, b, c) = t;
        (void)c;

        if (!world.IsAlive(a) || !world.IsAlive(b)) continue;
        if (!world.HasCollider(a) || !world.HasCollider(b)) continue;

        SortPair(a, b);
        uint64_t key = MakePairKey(a, b);
        cur.emplace(key, std::make_pair(a, b));

        const auto& ca = world.GetCollider(a);
        const auto& cb = world.GetCollider(b);

        CollisionEvent ev{};
        ev.a = a; ev.b = b;
        ev.aIsTrigger = ca.isTrigger;
        ev.bIsTrigger = cb.isTrigger;
        ev.type = (m_prevPairs.find(key) == m_prevPairs.end())
            ? CollisionEventType::Enter
            : CollisionEventType::Stay;

        world.PushCollisionEvent(ev);
    }

    // Exit
    for (const auto& it : m_prevPairs)
    {
        uint64_t key = it.first;
        if (cur.find(key) != cur.end()) continue;

        EntityId a = it.second.first;
        EntityId b = it.second.second;

        // 이미 삭제된 엔티티면 이벤트 스킵
        if (!world.IsAlive(a) || !world.IsAlive(b)) continue;
        if (!world.HasCollider(a) || !world.HasCollider(b)) continue;

        const auto& ca = world.GetCollider(a);
        const auto& cb = world.GetCollider(b);

        CollisionEvent ev{};
        ev.type = CollisionEventType::Exit;
        ev.a = a; ev.b = b;
        ev.aIsTrigger = ca.isTrigger;
        ev.bIsTrigger = cb.isTrigger;

        world.PushCollisionEvent(ev);
    }

    m_prevPairs = std::move(cur);
}

void PhysicsSystem::WarmStart(World& world, std::vector<std::tuple<EntityId, EntityId, Contact>>& contacts)
{
    for (auto& [a0, b0, c] : contacts)
    {
        EntityId a = a0, b = b0;

        // key만 정렬해서 찾기
        EntityId sa = a0, sb = b0;
        SortPair(sa, sb);
        uint64_t key = MakePairKey(sa, sb);

        auto it = m_contactCache.find(key);
        if (it == m_contactCache.end())
            continue;

        // 캐시에서 누적 임펄스 복원하기 전에 normal 유사도 체크
        const float kMinDot = 0.7f; // 보수적으로
        float dn = Dot(it->second.normal, c.normal);
        if (dn < kMinDot)
        {
            // 방향이 너무 다르면 워밍스타트 끔
            c.normalImpulseSum = 0.0f;
            c.tangentImpulseSum = 0.0f;
            continue;
        }

        // 캐시에서 누적 임펄스 복원
        c.normalImpulseSum = it->second.normalImpulseSum;
        c.tangentImpulseSum = it->second.tangentImpulseSum;

        // 웜스타트 임펄스 적용(속도에 미리 반영)
        const bool aDyn = world.HasRigidBody(a) && world.GetRigidBody(a).type == BodyType::Dynamic;
        const bool bDyn = world.HasRigidBody(b) && world.GetRigidBody(b).type == BodyType::Dynamic;

        float invMassA = aDyn ? world.GetRigidBody(a).invMass : 0.0f;
        float invMassB = bDyn ? world.GetRigidBody(b).invMass : 0.0f;
        float invMassSum = invMassA + invMassB;
        if (invMassSum <= 0.0f) continue;

        XMFLOAT3 n = c.normal;

        // 마찰 탄젠트(현재 상대속도로 계산)
        XMFLOAT3 vA{ 0,0,0 }, vB{ 0,0,0 };
        if (aDyn) vA = world.GetRigidBody(a).velocity;
        if (bDyn) vB = world.GetRigidBody(b).velocity;
        XMFLOAT3 vRel = Sub(vB, vA);
        float vn = Dot(vRel, n);
        XMFLOAT3 t = Sub(vRel, Mul(n, vn));
        if (Dot(t, t) < 1e-12f)
            t = OrthoTangentFromNormal(n);
        else
            t = NormalizeSafe(t, OrthoTangentFromNormal(n));

        XMFLOAT3 Pn = Mul(n, c.normalImpulseSum);
        XMFLOAT3 Pt = Mul(t, c.tangentImpulseSum);
        XMFLOAT3 P = Add(Pn, Pt);

        if (aDyn)
        {
            auto& rbA = world.GetRigidBody(a);
            rbA.velocity = Sub(rbA.velocity, Mul(P, invMassA));
        }
        if (bDyn)
        {
            auto& rbB = world.GetRigidBody(b);
            rbB.velocity = Add(rbB.velocity, Mul(P, invMassB));
        }
    }
}

void PhysicsSystem::StoreContactCache(const std::vector<std::tuple<EntityId, EntityId, Contact>>& contacts)
{
    std::unordered_map<uint64_t, CachedContact> next;
    next.reserve(contacts.size() * 2 + 8);

    for (const auto& [a0, b0, c] : contacts)
    {
        EntityId a = a0, b = b0;
        SortPair(a, b);

        uint64_t key = MakePairKey(a, b);
        CachedContact cc{};
        cc.normal = c.normal;
        cc.point = c.point;
        cc.normalImpulseSum = c.normalImpulseSum;
        cc.tangentImpulseSum = c.tangentImpulseSum;

        next.emplace(key, cc);
    }

    m_contactCache = std::move(next);
}

void PhysicsSystem::UpdateSleep(World& world, float dt)
{
    const float sleepLinThreshold = 0.05f;   // 스케일에 따라 조절
    const float sleepTime = 0.5f;            // 0.2~1.0s

    const float th2 = sleepLinThreshold * sleepLinThreshold;

    const auto& cols = world.GetColliderEntities();
    for (EntityId e : cols)
    {
        if (!world.HasRigidBody(e)) continue;

        auto& rb = world.GetRigidBody(e);
        if (rb.type != BodyType::Dynamic) continue;
        if (!rb.allowSleep) { Wake(world, e); continue; }
        if (!rb.isAwake) continue;

        const float v2 = rb.velocity.x * rb.velocity.x + rb.velocity.y * rb.velocity.y + rb.velocity.z * rb.velocity.z;
        if (v2 < th2)
        {
            // micro-jitter kill: clamp tiny velocity to 0 so timer can accumulate
            rb.velocity = { 0,0,0 };
            rb.sleepTimer += dt;
        }
        else
        {
            rb.sleepTimer = 0.0f;
        }

        if (rb.sleepTimer >= sleepTime)
            PutToSleep(world, e);
    }
}

bool PhysicsSystem::Raycast(const World& world, const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dirNormalized, float maxDist, RaycastHit& outHit, uint32_t collideMask, bool hitTriggers) const
{
    outHit = {};

    float bestT = maxDist;
    bool found = false;

    const auto& ents = world.GetColliderEntities();
    for (EntityId e : ents)
    {
        const auto& col = world.GetCollider(e);

        // layer/mask
        if ((col.layer & collideMask) == 0) continue;
        if (!hitTriggers && col.isTrigger) continue;

        float t = 0.0f;
        DirectX::XMFLOAT3 n{ 0,0,0 };

        if (col.shapeType == ShapeType::Sphere)
        {
            DirectX::XMFLOAT3 cW{};
            float r = 0.0f;
            GetSphereWorld_RowVector(world, e, cW, r);
            if (!RaySphere(origin, dirNormalized, bestT, cW, r, t, n)) continue;
        }
        else // Box
        {
            AABB box = ComputeWorldAABB(world, e);
            if (!RayAABB(origin, dirNormalized, bestT, box, t, n)) continue;
        }

        if (t < bestT)
        {
            bestT = t;
            found = true;
            outHit.entity = e;
            outHit.t = t;
            outHit.point = { origin.x + dirNormalized.x * t,
                             origin.y + dirNormalized.y * t,
                             origin.z + dirNormalized.z * t };
            outHit.normal = n;
            outHit.isTrigger = col.isTrigger;
        }
    }

    return found;
}

int PhysicsSystem::OverlapSphere(const World& world, const DirectX::XMFLOAT3& center, float radius, std::vector<EntityId>& outHits, uint32_t collideMask, bool includeTriggers) const
{
    outHits.clear();
    const float r2 = radius * radius;

    const auto& ents = world.GetColliderEntities();
    for (EntityId e : ents)
    {
        const auto& col = world.GetCollider(e);
        if ((col.layer & collideMask) == 0) continue;
        if (!includeTriggers && col.isTrigger) continue;

        bool hit = false;

        if (col.shapeType == ShapeType::Sphere)
        {
            DirectX::XMFLOAT3 cW{};
            float r = 0.0f;
            GetSphereWorld_RowVector(world, e, cW, r);
            float rr = radius + r;
            DirectX::XMFLOAT3 d{ cW.x - center.x, cW.y - center.y, cW.z - center.z };
            hit = (d.x * d.x + d.y * d.y + d.z * d.z) <= (rr * rr);
        }
        else
        {
            // sphere vs AABB
            AABB box = ComputeWorldAABB(world, e);
            float x = std::max(box.min.x, std::min(center.x, box.max.x));
            float y = std::max(box.min.y, std::min(center.y, box.max.y));
            float z = std::max(box.min.z, std::min(center.z, box.max.z));
            float dx = center.x - x, dy = center.y - y, dz = center.z - z;
            hit = (dx * dx + dy * dy + dz * dz) <= r2;
        }

        if (hit) outHits.push_back(e);
    }

    return (int)outHits.size();
}