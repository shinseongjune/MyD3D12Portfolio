#pragma once

#include <DirectXMath.h>
#include <cstdint>

// Unity-style light types.
enum class LightType : uint32_t
{
    Directional = 0,
    Point       = 1,
    Spot        = 2,
};

// A renderable light component that can be attached to an Entity.
// - Directional: uses Transform rotation (forward +Z) as light direction.
// - Point: uses Transform world position.
// - Spot: uses both world position and rotation.
// All angles are radians.
struct LightComponent
{
    LightType type = LightType::Directional;
    bool enabled = true;

    DirectX::XMFLOAT3 color = { 1, 1, 1 };
    float intensity = 1.0f;

    // Point/Spot
    float range = 10.0f;

    // Spot (full cone angles, radians)
    float innerAngleRad = DirectX::XMConvertToRadians(20.0f);
    float outerAngleRad = DirectX::XMConvertToRadians(30.0f);
};
