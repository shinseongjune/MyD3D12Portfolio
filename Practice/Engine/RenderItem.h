#pragma once
#include <DirectXMath.h>
#include "MeshHandle.h"
#include "TextureHandle.h"
#include <cstdint>

struct RenderItem
{
    MeshHandle mesh;
    DirectX::XMFLOAT4X4 world;

    // Per-material
    TextureHandle albedo;
    DirectX::XMFLOAT4 color{ 1,1,1,1 };

    // Per-draw (submesh)
    uint32_t startIndex = 0;
    uint32_t indexCount = 0; // 0이면 "전체"로 취급
};
