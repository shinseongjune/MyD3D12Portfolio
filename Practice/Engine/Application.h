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

class Application
{
private:
    Win32Window m_window;
    World m_world;
    bool m_running = false;

    uint32_t m_lastW = 0;
    uint32_t m_lastH = 0;

    std::unique_ptr<IRenderer> m_renderer;
    RenderSystem m_renderSystem;
    std::vector<RenderItem> m_renderItems;

    SceneManager m_sceneManager;
	MeshManager m_meshManager;
	TextureManager m_textureManager;
    ImportRegistry m_registry;
    AssetPipeline m_pipeline;

public:
    Application() : m_pipeline(m_registry, m_meshManager) { }

    ~Application();

    void Initialize(HINSTANCE hInstance);
    void Run();
    void Shutdown();
	RenderCamera BuildRenderCamera() const;
};
