#pragma once
#include "MeshHandle.h"
#include "MeshCPUData.h"
#include <unordered_map>
#include <functional>

class MeshManager
{
public:
    MeshHandle Create(const MeshCPUData& mesh);
    const MeshCPUData& Get(MeshHandle h) const;
    bool IsValid(MeshHandle h) const;

    void Destroy(MeshHandle h);

    using OnDestroyCallback = std::function<void(uint32_t meshId)>;
    void SetOnDestroy(OnDestroyCallback cb) { m_onDestroy = std::move(cb); }

private:
    uint32_t m_nextId = 1;
    std::unordered_map<uint32_t, MeshCPUData> m_meshes;

    OnDestroyCallback m_onDestroy;
};
