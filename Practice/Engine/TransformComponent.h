#pragma once
#include <DirectXMath.h>
#include <vector>
#include "EntityId.h"

struct TransformComponent
{
    // Local TRS
    DirectX::XMFLOAT3 position{ 0.f, 0.f, 0.f };
    DirectX::XMFLOAT4 rotation{ 0.f, 0.f, 0.f, 1.f }; // quaternion (x,y,z,w)
    DirectX::XMFLOAT3 scale{ 1.f, 1.f, 1.f };

    // Hierarchy
    EntityId parent = EntityId::Invalid();
    std::vector<EntityId> children;

    // Cached world matrix
    DirectX::XMFLOAT4X4 world{};

    bool dirty = true; // local 변경/부모 변경 시 true
};
