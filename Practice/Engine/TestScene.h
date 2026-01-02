#pragma once
#include <vector>
#include "Scene.h"
#include "EntityId.h"
#include "TextureHandle.h"

class TestScene final : public Scene
{
public:
    void OnLoad(SceneContext& ctx) override;
    void OnUnload(SceneContext& ctx) override;

    void OnUpdate(SceneContext& ctx) override;

private:

};