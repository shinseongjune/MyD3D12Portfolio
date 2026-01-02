#pragma once

struct SceneContext;

class Scene
{
public:
    virtual ~Scene() = default;
    virtual void OnLoad(SceneContext& ctx) = 0;
    virtual void OnUnload(SceneContext& ctx) = 0;

    virtual void OnUpdate(SceneContext& ctx) = 0;

	virtual void OnFixedUpdate(SceneContext& ctx) {}
};
