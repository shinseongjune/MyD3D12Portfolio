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
class ScriptSystem;

class SceneManager
{
public:
    SceneManager(World& w, AssetPipeline& ap, MeshManager& mm, TextureManager& tm, SoundManager& sm, AudioSystem& au, Input& ip, PhysicsSystem& ps, std::vector<UITextDraw>& textItems, ScriptSystem& ss)
		: m_world(w), m_assets(ap), m_meshes(mm), m_textures(tm), m_input(ip), m_physics(ps), m_sounds(sm), m_audio(au), m_textItems(textItems), m_scripts(ss)
    {
    }

    void Load(std::unique_ptr<Scene> scene);
    void Update(float dt);
    void FixedUpdate(float fixedDt);

    Scene* Current() const { return m_current.get(); }

    TextureHandle GetSkybox() const { return m_skybox; }
    void ClearSkybox() { m_skybox = {}; }

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
    ScriptSystem& m_scripts;

    SceneScope m_scope;
    std::unique_ptr<Scene> m_current;

    TextureHandle m_skybox{}; // id=0 => none
};
