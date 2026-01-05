#pragma once
#include <vector>
#include <algorithm>

#include "EntityId.h"
#include "MeshHandle.h"
#include "TextureHandle.h"
#include "SoundHandle.h"

class World;
class MeshManager;
class TextureManager;
class SoundManager;

class SceneScope
{
public:
    inline void Reset()
    {
        m_entities.clear();
        m_meshes.clear();
        m_textures.clear();
        m_sounds.clear();
    }

    inline void Track(EntityId e)
    {
        if (!e.IsValid()) return;
        m_entities.push_back(e);
    }

    inline void Track(MeshHandle h)
    {
        if (!h.IsValid()) return;
        m_meshes.push_back(h);
    }

    inline void Track(TextureHandle h)
    {
        if (!h.IsValid()) return;
        m_textures.push_back(h);
    }

    void Track(SoundHandle h)
    {
        if (!h.IsValid()) return;
        m_sounds.push_back(h);
    }

    // æ¿¿Ã º“¿Ø«— ∞Õ ¿œ∞˝ ¡§∏Æ
    void Cleanup(World& world, MeshManager& meshes, TextureManager& textures, SoundManager& sounds);

private:
    std::vector<EntityId>      m_entities;
    std::vector<MeshHandle>    m_meshes;
    std::vector<TextureHandle> m_textures;
    std::vector<SoundHandle>   m_sounds;
};
