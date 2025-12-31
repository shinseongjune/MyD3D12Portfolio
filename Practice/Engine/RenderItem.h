#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include "MeshHandle.h"

struct RenderItem
{
    MeshHandle mesh;
    DirectX::XMFLOAT4X4 world;

    // Per-material
    uint32_t srvIndex = 0;
    DirectX::XMFLOAT4 color{ 1,1,1,1 };
};
