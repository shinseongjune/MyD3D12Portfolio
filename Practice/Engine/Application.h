#pragma once
#include <memory>
#include <vector>
#include <Windows.h>
#include "Win32Window.h"
#include "World.h"
#include "IRenderer.h"
#include "RenderItem.h"
#include "RenderSystem.h"
#include "SceneManager.h"
#include "MeshManager.h"
#include "TextureManager.h"
#include "ImportRegistry.h"
#include "AssetPipeline.h"
#include "Input.h"
#include "PhysicsSystem.h"
#include "SoundManager.h"
#include "AudioSystem.h"
#include "UIHudSystem.h"
#include "UIDrawItem.h"

class Application
{
private:
    Win32Window m_window;
    World m_world;
    bool m_running = false;
    double m_dt = 0;

	// fixed deltatime 관련
    double m_accum = 0.0;
    double m_fixedDt = 1.0 / 60.0;   // 60Hz
    double m_maxAccum = 0.25;        // spiral of death 방지(250ms)

    uint32_t m_lastW = 0;
    uint32_t m_lastH = 0;

    std::unique_ptr<IRenderer> m_renderer;
    RenderSystem m_renderSystem;
    std::vector<RenderItem> m_renderItems;

	MeshManager m_meshManager;
	TextureManager m_textureManager;
    ImportRegistry m_registry;
    AssetPipeline m_pipeline;
    SoundManager m_soundManager;
    AudioSystem  m_audioSystem;
    Input m_input;
    SceneManager m_sceneManager;

    PhysicsSystem m_physics;

    std::vector<UIDrawItem> m_uiItems;
    UIHudSystem m_uiHud;

public:
    Application() : m_pipeline(m_registry, m_meshManager), m_sceneManager(m_world, m_pipeline, m_meshManager, m_textureManager, m_soundManager, m_audioSystem, m_input, m_physics) { }

    ~Application();

    void Initialize(HINSTANCE hInstance);
    void Run();
    void Shutdown();
	RenderCamera BuildRenderCamera() const;

private:
    void Resize();
    void BeginFrame();                       // input, time
	void UpdateScene(const double dt);       // Scene.OnUpdate
    void TickFixed(const double dt);
	void UpdateTransforms();                 // World.UpdateTransforms
	void UpdateSystems();                    // RenderSystem.Build 같은 것
	void RenderFrame();                      // Renderer.Render
    void EndFrame();                         // FlushDestroy
};
