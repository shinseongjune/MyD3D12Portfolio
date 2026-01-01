#pragma once
#include <vector>
#include "Scene.h"
#include "EntityId.h"
#include "MeshManager.h"
#include "TextureManager.h"
#include "AssetPipeline.h"

class TestScene final : public Scene
{
public:
    TestScene(MeshManager& mm, TextureManager& tm, AssetPipeline& ap) : m_meshManager(mm), m_textureManager(tm), m_assetPipeline(ap) {}

    void OnLoad(World& world) override;
    void OnUnload(World& world) override;

    void OnUpdate(World& world, float deltaTime) override;

private:
    std::vector<EntityId> m_spawned;
    std::vector<TextureHandle> m_ownedTextures;

    MeshManager& m_meshManager;
	TextureManager& m_textureManager;
    AssetPipeline& m_assetPipeline;
};