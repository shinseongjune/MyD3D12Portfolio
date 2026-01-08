#pragma once
#include <string>
#include <vector>

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
#include "UITextDraw.h"

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

    // ---- Per-frame text overlay sink (owned by Application) ----
    std::vector<UITextDraw>& text;

    float dt = 0.0f;

	// ---- Skybox 관리용 포인터 (SceneManager가 소유) ----
    TextureHandle* skybox = nullptr;

	// ---- Skybox helpers ----
    void SetSkybox(TextureHandle h)
    {
        if (!skybox) return;
        *skybox = h; // invalid면 제거 효과
    }

    void ClearSkybox()
    {
        if (!skybox) return;
        *skybox = {};
    }

    // Cubemap (6-face) load
    Result<TextureHandle> LoadCubemapScoped(const std::array<std::string, 6>& utf8Paths);

    // ---- Unity-like helpers ----
    EntityId Instantiate(const std::string& name = "");

    void Destroy(EntityId e);    

	// 모델 임포트 (리소스만 생성)
    Result<ModelAsset> ImportModel(const std::string& path, const ImportOptions& importOpt);

    // Import + Instantiate를 한 번에(씬에서 제일 많이 쓰는 UX)
    Result<EntityId> SpawnModel(const std::string& path, const ImportOptions& importOpt, const SpawnModelOptions& spawnOpt);

    Result<EntityId> SpawnModel(const ModelAsset& asset, const SpawnModelOptions& spawnOpt);

    // 텍스처 로드
    Result<TextureHandle> LoadTextureScoped(const std::string& utf8Path);

    Result<TextureHandle> LoadTextureShared(const std::string& utf8Path);

    Result<SoundHandle> LoadSoundScoped(const std::string& utf8Path);

    Result<SoundHandle> LoadSoundShared(const std::string& utf8Path);

    void PlaySFX(SoundHandle clip, float volume = 1.0f, float pitch = 1.0f);

    void PlayBGM(SoundHandle clip, float volume = 1.0f);

    void StopBGM();

    // ---- Text overlay helpers (scene-friendly) ----
    void DrawText(float x, float y, const std::wstring& str,
                  float sizePx = 16.0f,
                  const DirectX::XMFLOAT4& color = DirectX::XMFLOAT4(1, 1, 1, 1),
                  const std::wstring& fontFamily = L"Segoe UI");

};
