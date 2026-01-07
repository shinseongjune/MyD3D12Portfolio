#include "SceneManager.h"

#include "SceneContext.h"

void SceneManager::Load(std::unique_ptr<Scene> scene)
{
    if (m_current)
    {
        SceneContext ctx{ m_world, m_assets, m_meshes, m_textures, m_scope, m_input, m_physics, m_sounds, m_audio, m_textItems, 0.0f };
        m_current->OnUnload(ctx);

        m_scope.Cleanup(m_world, m_meshes, m_textures, m_sounds);
        m_world.FlushDestroy();

        m_current.reset();
    }

    m_scope.Reset();
    m_current = std::move(scene);

    if (m_current)
    {
        SceneContext ctx{ m_world, m_assets, m_meshes, m_textures, m_scope, m_input, m_physics, m_sounds, m_audio, m_textItems, 0.0f };
        m_current->OnLoad(ctx);
    }
}

void SceneManager::Update(float dt)
{
    if (!m_current) return;
    SceneContext ctx{ m_world, m_assets, m_meshes, m_textures, m_scope, m_input, m_physics, m_sounds, m_audio, m_textItems, dt };
    m_current->OnUpdate(ctx);
}

void SceneManager::FixedUpdate(float fixedDt)
{
    if (!m_current) return;
    SceneContext ctx{ m_world, m_assets, m_meshes, m_textures, m_scope, m_input, m_physics, m_sounds, m_audio, m_textItems, fixedDt };
    m_current->OnFixedUpdate(ctx);
}
