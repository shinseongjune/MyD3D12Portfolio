#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>
#include "MeshHandle.h"

// "파일 1개를 import한 결과"를 엔진이 재사용하기 쉬운 형태로 보관
struct ModelAssetMesh
{
    std::string name;                 // mesh 이름(엔티티 이름에 사용)
    MeshHandle mesh;                  // MeshManager에 등록된 핸들
    DirectX::XMFLOAT4 baseColor{ 1,1,1,1 }; // 현재는 색만 (나중에 MaterialHandle로 교체)
};

struct ModelAsset
{
    std::string sourcePath;
    std::vector<ModelAssetMesh> meshes;
};
