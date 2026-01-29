#pragma once
#include <DirectXMath.h>
#include "MeshHandle.h"
#include "TextureHandle.h"
#include <cstdint>

struct RenderItem
{
    MeshHandle mesh;
    DirectX::XMFLOAT4X4 world;

    // Textures
    TextureHandle albedo;

    // Tint (rgba)
    DirectX::XMFLOAT4 color{ 1,1,1,1 };

    // Material params (PBR-lite)
    float roughness = 0.4f; // 0=매끈(하이라이트 선명), 1=거침
    float metallic = 0.0f; // 0=비금속, 1=금속
    float emissive = 0.0f; // 0=없음, 1+=발광(가짜)
    float _padMat0 = 0.0f; // 16바이트 정렬 맞추기(선택)

    // flags
    bool transparent = false;
    bool unlit = false;
    uint8_t _pad0[2]{}; // 패딩

    // Per-draw (submesh)
    uint32_t startIndex = 0;
    uint32_t indexCount = 0; // 0이면 "전체"
};
