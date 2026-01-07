#pragma once
#include <memory>
#include <vector>
#include "Scene.h"
#include "SceneScope.h"
#include "UITextDraw.h"

class World;
class AssetPipeline;
class MeshManager;
class TextureManager;
class Input;
class PhysicsSystem;
class SoundManager;
class AudioSystem;

class SceneManager
{
public:
    SceneManager(World& w, AssetPipeline& ap, MeshManager& mm, TextureManager& tm, SoundManager& sm, AudioSystem& au, Input& ip, PhysicsSystem& ps, std::vector<UITextDraw>& textItems)
		: m_world(w), m_assets(ap), m_meshes(mm), m_textures(tm), m_input(ip), m_physics(ps), m_sounds(sm), m_audio(au), m_textItems(textItems)
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
    SoundManager& m_sounds;
    AudioSystem& m_audio;
    std::vector<UITextDraw>& m_textItems;

    SceneScope m_scope;
    std::unique_ptr<Scene> m_current;
};
