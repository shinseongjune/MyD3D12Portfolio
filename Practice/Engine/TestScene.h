#pragma once
#include <vector>
#include "Scene.h"
#include "EntityId.h"
#include "MeshManager.h"

class TestScene final : public Scene
{
public:
    TestScene(MeshManager& mm) : m_meshManager(mm) {}

    void OnLoad(World& world) override;
    void OnUnload(World& world) override;

    void OnUpdate(World& world, float deltaTime) override;

private:
    std::vector<EntityId> m_spawned;

    MeshManager& m_meshManager;
};