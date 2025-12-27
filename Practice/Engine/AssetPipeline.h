#pragma once
#include <string>
#include "ImportRegistry.h"

#include "MeshManager.h"
#include "World.h"
#include "EntityId.h"

struct SpawnModelOptions
{
    std::string name = "ImportedModel";
    bool spawnAsSingleEntity = true; // true: 1 entity + 여러 submesh 렌더(향후)
    // false: mesh별 entity 생성(간단)
};

class AssetPipeline
{
public:
    AssetPipeline(ImportRegistry& registry, MeshManager& meshManager)
        : m_registry(registry), m_meshManager(meshManager) {
    }

    Result<EntityId> LoadModelIntoWorld(World& world,
        const std::string& path,
        const ImportOptions& importOpt,
        const SpawnModelOptions& spawnOpt);

private:
    ImportRegistry& m_registry;
    MeshManager& m_meshManager;
};
