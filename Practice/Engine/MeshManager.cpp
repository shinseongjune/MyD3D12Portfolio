#include "MeshManager.h"
#include <cassert>

MeshHandle MeshManager::Create(const MeshResource& mesh)
{
    const uint32_t id = m_nextId++;
    m_meshes.emplace(id, mesh);
    return MeshHandle{ id };
}

const MeshResource& MeshManager::Get(MeshHandle h) const
{
    auto it = m_meshes.find(h.id);
    assert(it != m_meshes.end() && "Invalid MeshHandle");
    return it->second;
}

bool MeshManager::IsValid(MeshHandle h) const
{
    return m_meshes.find(h.id) != m_meshes.end();
}
