#pragma once
#include "MeshCPUData.h"
#include <cstdint>

namespace PrimitiveMeshes
{
    // 중심 (0,0,0), 한 변 길이 1.0 (즉 -0.5 ~ +0.5)
    MeshCPUData MakeUnitBox();

    // 중심 (0,0,0), 반지름 0.5 (즉 지름 1.0)
    // stacks: 위도 분할, slices: 경도 분할
    MeshCPUData MakeUnitSphereUV(uint32_t stacks = 6, uint32_t slices = 12);
}
