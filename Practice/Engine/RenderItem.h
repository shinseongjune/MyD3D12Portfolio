#pragma once
#include <DirectXMath.h>
#include <cstdint>

struct RenderItem
{
    DirectX::XMFLOAT4X4 world;     // 월드행렬
    DirectX::XMFLOAT4   color;     // 디버그 색(예: 소켓=노랑)
    uint32_t            meshId = 0; // 0=DebugCube(임시 규약)
};
