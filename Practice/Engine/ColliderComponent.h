#pragma once
#include "PhysicsTypes.h"

struct SphereShape { float radius = 0.5f; };
struct BoxShape { XMFLOAT3 halfExtents{ 0.5f,0.5f,0.5f }; }; // AABB/로컬박스

struct ColliderComponent
{
    ShapeType shapeType = ShapeType::Sphere;

    // 로컬 오프셋(트랜스폼 기준)
    XMFLOAT3 localCenter{ 0,0,0 };

    bool isTrigger = false;

    uint32_t layer = 0;           // 0~31
    uint32_t collideMask = ~0u;   // 기본: 전부와 충돌

    PhysicsMaterial material{};

    SphereShape sphere{};
    BoxShape box{};
};