#pragma once
#include <vector>
#include "Scene.h"
#include "EntityId.h"

class TestScene final : public Scene
{
public:
    void OnLoad(World& world) override;
    void OnUnload(World& world) override;

private:
    std::vector<EntityId> m_spawned;
};
