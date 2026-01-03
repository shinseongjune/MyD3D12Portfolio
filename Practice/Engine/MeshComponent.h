#pragma once
#include "MeshHandle.h"
#include <vector>
#include <cstdint>

struct MeshSubmeshDraw
{
    MeshHandle mesh;            // 같은 ImportedMesh면 동일 mesh handle
    uint32_t startIndex = 0;    // DrawIndexedInstanced의 StartIndexLocation
    uint32_t indexCount = 0;    // 0이면 "전체"로 취급
    uint32_t materialIndex = 0; // MaterialSetComponent에서 참조할 인덱스
};

struct MeshComponent
{
    std::vector<MeshSubmeshDraw> draws;

    MeshComponent() = default;

    // 기존 호출 형태 MeshComponent{ handle } 대응(전체 범위 0으로)
    explicit MeshComponent(MeshHandle h)
    {
        draws.push_back(MeshSubmeshDraw{ h, 0u, 0u, 0u });
    }

    // 서브메쉬용
    MeshComponent(MeshHandle h, uint32_t start, uint32_t count, uint32_t matIndex)
    {
        draws.push_back(MeshSubmeshDraw{ h, start, count, matIndex });
    }
};
