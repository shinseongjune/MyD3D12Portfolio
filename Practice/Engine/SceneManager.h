// SceneManager.h
#pragma once
#include <memory>

#include "Scene.h"
#include "AssetPipeline.h"
#include "SceneScope.h"
#include "Input.h"

class SceneManager
{
public:
    SceneManager(World& w, AssetPipeline& ap, MeshManager& mm, TextureManager& tm, Input& ip)
		: m_world(w), m_assets(ap), m_meshes(mm), m_textures(tm), m_input(ip)
    {
    }

    void Load(std::unique_ptr<Scene> scene);
    void Update(float dt);
    void FixedUpdate(float fixedDt);

    Scene* Current() const { return m_current.get(); }

private:
    World& m_world;
    AssetPipeline& m_assets;
    MeshManager& m_meshes;
    TextureManager& m_textures;
    Input& m_input;

    SceneScope m_scope;
    std::unique_ptr<Scene> m_current;
};
