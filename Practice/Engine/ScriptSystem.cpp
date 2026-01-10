#include "ScriptSystem.h"
#include "SceneContext.h"
#include "World.h"
#include "ScriptComponent.h"
#include "Behaviour.h"

static void EnsureAwakeStart(SceneContext& ctx, ScriptComponent& sc)
{
    for (auto& s : sc.scripts)
    {
        if (!s.ptr) continue;
        if (!s.enabled) continue;

        if (!s.awoken) { s.ptr->Awake(ctx); s.awoken = true; }
        if (!s.started) { s.ptr->Start(ctx); s.started = true; }
    }
}

void ScriptSystem::Update(SceneContext& ctx)
{
    auto& world = ctx.world;
    const auto& ents = world.GetScriptEntities();

    for (auto e : ents)
    {
        if (!world.IsAlive(e)) continue;
        auto& sc = world.GetScript(e);

        EnsureAwakeStart(ctx, sc);

        for (auto& s : sc.scripts)
        {
            if (!s.ptr || !s.enabled) continue;
            s.ptr->Update(ctx);
        }
    }
}

void ScriptSystem::FixedUpdate(SceneContext& ctx)
{
    auto& world = ctx.world;
    const auto& ents = world.GetScriptEntities();

    for (auto e : ents)
    {
        if (!world.IsAlive(e)) continue;
        auto& sc = world.GetScript(e);

        EnsureAwakeStart(ctx, sc);

        for (auto& s : sc.scripts)
        {
            if (!s.ptr || !s.enabled) continue;
            s.ptr->FixedUpdate(ctx);
        }
    }
}
