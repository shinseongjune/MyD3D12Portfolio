#pragma once
#include <string>

#include "World.h"
#include "AssetPipeline.h"
#include "MeshManager.h"
#include "TextureManager.h"
#include "SceneScope.h"
#include "ImportTypes.h"
#include "Utilities.h"
#include "Input.h"
#include "SoundManager.h"
#include "SoundImporterMF.h"
#include "SoundHandle.h"
#include "AudioSystem.h"
#include "AudioCommand.h"
#include "TextureHandle.h"

class PhysicsSystem;

struct SceneContext
{
    World& world;
    AssetPipeline& assets;
    MeshManager& meshes;
    TextureManager& textures;
    SceneScope& scope;
    Input& input;
	PhysicsSystem& physics;
    SoundManager& sounds;
    AudioSystem& audio;

    float dt = 0.0f;

    // ---- Unity-like helpers ----
    EntityId Instantiate(const std::string& name = "");

    void Destroy(EntityId e);    

    // Import + Instantiate를 한 번에(씬에서 제일 많이 쓰는 UX)
    Result<EntityId> SpawnModel(
        const std::string& path,
        const ImportOptions& importOpt,
        const SpawnModelOptions& spawnOpt);

    // 텍스처 로드
    Result<TextureHandle> LoadTextureScoped(const std::string& utf8Path);

    Result<TextureHandle> LoadTextureShared(const std::string& utf8Path);

    Result<SoundHandle> LoadSoundScoped(const std::string& utf8Path);

    Result<SoundHandle> LoadSoundShared(const std::string& utf8Path);

    void PlaySFX(SoundHandle clip, float volume = 1.0f, float pitch = 1.0f);

    void PlayBGM(SoundHandle clip, float volume = 1.0f);

    void StopBGM();

};
