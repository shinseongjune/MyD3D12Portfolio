#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include "MeshHandle.h"

struct RenderItem
{
    MeshHandle mesh;
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4   color;
};
