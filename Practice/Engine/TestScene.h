#pragma once
#include <vector>
#include "Scene.h"
#include "EntityId.h"
#include "MeshManager.h"
#include "AssetPipeline.h"

class TestScene final : public Scene
{
public:
    TestScene(MeshManager& mm, AssetPipeline& ap) : m_meshManager(mm), m_assetPipeline(ap) {}

    void OnLoad(World& world) override;
    void OnUnload(World& world) override;

    void OnUpdate(World& world, float deltaTime) override;

private:
    std::vector<EntityId> m_spawned;

    MeshManager& m_meshManager;
    AssetPipeline& m_assetPipeline;
};