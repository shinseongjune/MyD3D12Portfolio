#pragma once
#include <string>
#include "ImportRegistry.h"
#include "MeshManager.h"
#include "ModelAsset.h"
#include "World.h"
#include "EntityId.h"
#include "Utilities.h"

struct SpawnModelOptions
{
    std::string name = "ImportedModel";
};

class AssetPipeline
{
public:
    AssetPipeline(ImportRegistry& registry, MeshManager& meshManager)
        : m_registry(registry), m_meshManager(meshManager) {
    }

    // 1) Import: 颇老 -> ModelAsset(府家胶父 积己)
    Result<ModelAsset> ImportModel(
        const std::string& path,
        const ImportOptions& importOpt);

    // 2) Instantiate: ModelAsset -> World俊 浚萍萍 积己
    Result<EntityId> InstantiateModel(
        World& world,
        const ModelAsset& asset,
        const SpawnModelOptions& spawnOpt);

private:
    ImportRegistry& m_registry;
    MeshManager& m_meshManager;
};
