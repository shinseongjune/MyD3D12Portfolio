#pragma once

#include <DirectXMath.h>
#include <cstdint>

// CPU-side light data passed from RenderSystem/Application to the renderer.
// This is intentionally simple POD so it can be copied into GPU constant buffer easily.

static constexpr uint32_t MaxLightsPerFrame = 32;

struct FrameLight
{
    uint32_t type = 0; // 0=Directional, 1=Point, 2=Spot
    uint32_t _pad0[3]{};

    // NOTE: This layout MUST match the HLSL Light struct in D3D12Renderer.cpp.
    // Keep it 16-byte aligned and avoid XMFLOAT4 packing tricks.

    DirectX::XMFLOAT3 positionWS = { 0, 0, 0 };
    float range = 10.0f;

    DirectX::XMFLOAT3 directionWS = { 0, 0, 1 };
    float intensity = 1.0f;

    DirectX::XMFLOAT3 color = { 1, 1, 1 };
    float innerCos = 1.0f;

    float outerCos = 1.0f;
    float _pad1[3]{};
};

struct FrameLights
{
    DirectX::XMFLOAT3 cameraPosWS = { 0, 0, 0 };
    uint32_t numLights = 0;

    FrameLight lights[MaxLightsPerFrame]{};
};
