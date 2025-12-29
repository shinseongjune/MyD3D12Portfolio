#include "MeshManager.h"
#include <cassert>

MeshHandle MeshManager::Create(const MeshCPUData& mesh)
{
    const uint32_t id = m_nextId++;
    m_meshes.emplace(id, mesh);
    return MeshHandle{ id };
}

const MeshCPUData& MeshManager::Get(MeshHandle h) const
{
    auto it = m_meshes.find(h.id);
    assert(it != m_meshes.end() && "Invalid MeshHandle");
    return it->second;
}

bool MeshManager::IsValid(MeshHandle h) const
{
    return m_meshes.find(h.id) != m_meshes.end();
}

void MeshManager::Destroy(MeshHandle h)
{
    auto it = m_meshes.find(h.id);
    if (it == m_meshes.end())
        return;

    // Renderer 쪽 GPU 캐시 해제 예약
    if (m_onDestroy)
        m_onDestroy(h.id);

    m_meshes.erase(it);
}
