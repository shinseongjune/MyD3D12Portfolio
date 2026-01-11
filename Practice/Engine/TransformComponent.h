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

    inline DirectX::XMFLOAT3 ForwardLocal(const TransformComponent& t)
    {
        using namespace DirectX;
        XMVECTOR q = XMLoadFloat4(&t.rotation);
        XMVECTOR f = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), q); // +Z forward
        XMFLOAT3 out; XMStoreFloat3(&out, f);
        return out;
    }

    inline DirectX::XMFLOAT3 RightLocal(const TransformComponent& t)
    {
        using namespace DirectX;
        XMVECTOR q = XMLoadFloat4(&t.rotation);
        XMVECTOR r = XMVector3Rotate(XMVectorSet(1, 0, 0, 0), q); // +X right
        XMFLOAT3 out; XMStoreFloat3(&out, r);
        return out;
    }

    inline DirectX::XMFLOAT3 UpLocal(const TransformComponent& t)
    {
        using namespace DirectX;
        XMVECTOR q = XMLoadFloat4(&t.rotation);
        XMVECTOR u = XMVector3Rotate(XMVectorSet(0, 1, 0, 0), q); // +Y up
        XMFLOAT3 out; XMStoreFloat3(&out, u);
        return out;
    }
};
