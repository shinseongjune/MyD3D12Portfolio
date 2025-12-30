#pragma once
#include <DirectXMath.h>
#include <vector>

struct MeshCPUData
{
    std::vector<DirectX::XMFLOAT3> positions;
    std::vector<DirectX::XMFLOAT2> uvs;
    std::vector<uint16_t> indices;
};
