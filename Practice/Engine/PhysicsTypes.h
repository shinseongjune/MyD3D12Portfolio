#pragma once
#include <DirectXMath.h>
#include <cstdint>

using namespace DirectX;

enum class BodyType : uint8_t { Static, Dynamic /*, Kinematic*/ };
enum class ShapeType : uint8_t { Sphere, Box };

struct PhysicsMaterial
{
    float restitution = 0.0f; // 0: 비탄성, 1: 완전탄성
    float friction = 0.5f;
};

struct AABB
{
    XMFLOAT3 min;
    XMFLOAT3 max;
};

struct Contact
{
    // A를 normal 방향으로 밀면 분리되는 규약
    XMFLOAT3 normal;
    float    penetration;
    XMFLOAT3 point;

    // --- Sequential Impulses용(누적 임펄스) ---
    float normalImpulseSum = 0.0f;   // λn 누적 (>=0)
    float tangentImpulseSum = 0.0f;  // λt 누적 (마찰)
};