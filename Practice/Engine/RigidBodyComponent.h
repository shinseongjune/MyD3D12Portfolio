#pragma once
#include "PhysicsTypes.h"

struct RigidBodyComponent
{
    BodyType type = BodyType::Static;

    XMFLOAT3 velocity{ 0,0,0 };
    float mass = 1.0f;
    float invMass = 1.0f;

    float gravityScale = 1.0f;
    float linearDamping = 0.01f; // 아주 약하게

    bool useGravity = true;

	// Sleep
    bool isAwake = true;
    float sleepTimer = 0.0f;
    bool allowSleep = true;

    void RecalcInvMass()
    {
        invMass = (type == BodyType::Dynamic && mass > 0.0f) ? (1.0f / mass) : 0.0f;
    }
};