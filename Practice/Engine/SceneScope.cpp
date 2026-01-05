#include "SceneScope.h"
#include "World.h"
#include "MeshManager.h"
#include "TextureManager.h"
#include "SoundManager.h"

static void UniqueById(std::vector<uint32_t>& ids)
{
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
}

void SceneScope::Cleanup(World& world, MeshManager& meshes, TextureManager& textures, SoundManager& sounds)
{
    // 1) Entities
    for (auto e : m_entities)
    {
        if (world.IsAlive(e))
            world.RequestDestroy(e);
    }

    // 2) Scene-owned resources (dedupe by id)
    {
        std::vector<uint32_t> ids;
        ids.reserve(m_meshes.size());
        for (auto h : m_meshes) if (h.IsValid()) ids.push_back(h.id);
        UniqueById(ids);
        for (uint32_t id : ids)
        {
            MeshHandle h{ id };
            if (meshes.IsValid(h))
                meshes.Destroy(h);
        }
    }

    {
        std::vector<uint32_t> ids;
        ids.reserve(m_textures.size());
        for (auto h : m_textures) if (h.IsValid()) ids.push_back(h.id);
        UniqueById(ids);
        for (uint32_t id : ids)
        {
            TextureHandle h{ id };
            if (textures.IsValid(h))
                textures.Destroy(h);
        }
    }

    {
        std::vector<uint32_t> ids;
        ids.reserve(m_sounds.size());
        for (auto h : m_sounds) if (h.IsValid()) ids.push_back(h.id);
        UniqueById(ids);
        for (uint32_t id : ids)
        {
            SoundHandle h{ id };
            if (sounds.IsValid(h))
                sounds.Destroy(h);
        }
    }

    Reset();
}
