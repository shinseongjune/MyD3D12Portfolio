#pragma once
#include <string>

#include "World.h"
#include "AssetPipeline.h"
#include "MeshManager.h"
#include "TextureManager.h"
#include "SceneScope.h"
#include "ImportTypes.h"   // Result, ImportOptions
#include "Input.h"

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

    float dt = 0.0f;

    // ---- Unity-like helpers ----
    EntityId Instantiate(const std::string& name = "")
    {
        EntityId e = world.CreateEntity(name);
        scope.Track(e);
        return e;
    }

    void Destroy(EntityId e)
    {
        if (world.IsAlive(e))
            world.RequestDestroy(e);
        // scope에서 제거까지 굳이 안 해도 됨(중복 요청은 Flush에서 처리하면 됨)
    }

    // Import + Instantiate를 한 번에(씬에서 제일 많이 쓰는 UX)
    Result<EntityId> SpawnModel(
        const std::string& path,
        const ImportOptions& importOpt,
        const SpawnModelOptions& spawnOpt)
    {
        auto imported = assets.ImportModel(path, importOpt);
        if (!imported.IsOk())
            return Result<EntityId>::Fail(imported.error->message);

        auto spawned = assets.InstantiateModel(world, imported.value, spawnOpt);
        if (!spawned.IsOk())
            return Result<EntityId>::Fail(spawned.error->message);

        scope.Track(spawned.value);
        return spawned;
    }

    //// 텍스처 로드(지금 단계에서는 "씬 소유"로 추적할지 선택 가능)
    //Result<TextureHandle> LoadTextureScoped(const std::string& utf8Path)
    //{
    //    auto r = textures.Load(utf8Path);
    //    if (r.IsOk())
    //        scope.Track(r.value);
    //    return r;
    //}

    Result<TextureHandle> LoadTextureShared(const std::string& utf8Path)
    {
        // 추적 안 함: 공유 리소스로 취급
        return textures.Load(utf8Path);
    }
};
