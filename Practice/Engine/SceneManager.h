#pragma once
#include <memory>
#include "Scene.h"
#include "SceneScope.h"

class World;
class AssetPipeline;
class MeshManager;
class TextureManager;
class Input;
class PhysicsSystem;

class SceneManager
{
public:
    SceneManager(World& w, AssetPipeline& ap, MeshManager& mm, TextureManager& tm, Input& ip, PhysicsSystem& ps)
		: m_world(w), m_assets(ap), m_meshes(mm), m_textures(tm), m_input(ip), m_physics(ps)
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
	PhysicsSystem& m_physics;

    SceneScope m_scope;
    std::unique_ptr<Scene> m_current;
};
