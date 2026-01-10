#include "SceneManager.h"

#include "SceneContext.h"
#include "ScriptSystem.h"

void SceneManager::Load(std::unique_ptr<Scene> scene)
{
    if (m_current)
    {
        SceneContext ctx{ m_world, m_assets, m_meshes, m_textures, m_scope, m_input, m_physics, m_sounds, m_audio, m_textItems, 0.0f, &m_skybox };
        m_current->OnUnload(ctx);

        m_scope.Cleanup(m_world, m_meshes, m_textures, m_sounds);
        m_world.FlushDestroy();

		m_skybox = {}; // 씬 전환 시 스카이박스 초기화

        m_current.reset();
    }

    m_scope.Reset();
    m_current = std::move(scene);

    if (m_current)
    {
        SceneContext ctx{ m_world, m_assets, m_meshes, m_textures, m_scope, m_input, m_physics, m_sounds, m_audio, m_textItems, 0.0f, &m_skybox };
        m_current->OnLoad(ctx);
    }
}

void SceneManager::Update(float dt)
{
    if (!m_current) return;
    SceneContext ctx{ m_world, m_assets, m_meshes, m_textures, m_scope, m_input, m_physics, m_sounds, m_audio, m_textItems, dt, &m_skybox };
    m_current->OnUpdate(ctx);
    m_scripts.Update(ctx);
}

void SceneManager::FixedUpdate(float fixedDt)
{
    if (!m_current) return;
    SceneContext ctx{ m_world, m_assets, m_meshes, m_textures, m_scope, m_input, m_physics, m_sounds, m_audio, m_textItems, fixedDt, &m_skybox };
    m_current->OnFixedUpdate(ctx);
    m_scripts.FixedUpdate(ctx);
}
