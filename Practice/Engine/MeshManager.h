#pragma once
#include "MeshHandle.h"
#include "MeshResource.h"

#include <unordered_map>

class MeshManager
{
public:
    MeshHandle Create(const MeshResource& mesh);
    const MeshResource& Get(MeshHandle h) const;
    bool IsValid(MeshHandle h) const;

private:
    uint32_t m_nextId = 1;
    std::unordered_map<uint32_t, MeshResource> m_meshes;
};
