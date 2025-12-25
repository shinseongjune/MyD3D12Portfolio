#pragma once
#include <cstdint>

// 지금 단계에선 "어떤 메쉬를 그릴까" 정도만 들고 간다.
// - 0 = DebugCube (현재 D3D12Renderer가 그리는 큐브)
// - 나중에 MeshHandle(ResourceManager)로 대체 예정
struct MeshComponent
{
    uint32_t meshId = 0;
};
