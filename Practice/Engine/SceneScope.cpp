#include "SceneScope.h"
#include "World.h"
#include "MeshManager.h"
#include "TextureManager.h"

void SceneScope::Cleanup(World& world, MeshManager& meshes, TextureManager& textures)
{
    for (auto e : m_entities)
    {
        if (world.IsAlive(e))
            world.RequestDestroy(e);
    }

    //for (auto h : m_meshes)
    //{
    //    if (meshes.IsValid(h))
    //        meshes.Destroy(h);
    //}
    //
    //for (auto h : m_textures)
    //{
    //    if (h.IsValid())
    //        textures.Destroy(h);
    //}

    Reset();
}
