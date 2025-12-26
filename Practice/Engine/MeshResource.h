#pragma once
#include <DirectXMath.h>
#include <vector>

struct MeshResource
{
    std::vector<DirectX::XMFLOAT3> positions;
    std::vector<uint16_t> indices;
};
