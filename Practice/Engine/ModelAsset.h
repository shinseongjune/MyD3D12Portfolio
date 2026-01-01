#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>
#include "MeshHandle.h"

struct ModelAssetSubmesh
{
    uint32_t startIndex = 0;
    uint32_t indexCount = 0;
    uint32_t materialIndex = 0;
    std::string name;
};

struct ModelAssetMesh
{
    std::string name;
    MeshHandle mesh;
    DirectX::XMFLOAT4 baseColor{ 1,1,1,1 };

    std::vector<ModelAssetSubmesh> submeshes;
};

struct ModelAsset
{
    std::string sourcePath;
    std::vector<ModelAssetMesh> meshes;

    // (선택 확장) 나중에 이미지/머티리얼 원본도 보관하고 싶으면 여기에 추가
};
