#pragma once
struct SceneContext;

class ScriptSystem
{
public:
    void Update(SceneContext& ctx);
    void FixedUpdate(SceneContext& ctx);
};
