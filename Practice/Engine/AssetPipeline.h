#pragma once
#include <string>
#include "ImportRegistry.h"
#include "MeshManager.h"
#include "ModelAsset.h"
#include "World.h"
#include "EntityId.h"

struct SpawnModelOptions
{
    std::string name = "ImportedModel";
    bool spawnAsSingleEntity = true; // (향후) true: 1 entity + 여러 submesh 렌더
};

class AssetPipeline
{
public:
    AssetPipeline(ImportRegistry& registry, MeshManager& meshManager)
        : m_registry(registry), m_meshManager(meshManager) {
    }

    // 1) Import: 파일 -> ModelAsset(리소스만 생성)
    Result<ModelAsset> ImportModel(
        const std::string& path,
        const ImportOptions& importOpt);

    // 2) Instantiate: ModelAsset -> World에 엔티티 생성
    Result<EntityId> InstantiateModel(
        World& world,
        const ModelAsset& asset,
        const SpawnModelOptions& spawnOpt);

private:
    ImportRegistry& m_registry;
    MeshManager& m_meshManager;
};
