#pragma once
#include <memory>
#include <vector>
#include <Windows.h>

#include "Win32Window.h"
#include "World.h"
#include "IRenderer.h"
#include "RenderItem.h"
#include "RenderSystem.h"

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

public:
    Application() = default;
    ~Application();

    void Initialize(HINSTANCE hInstance);
    void Run();
    void Shutdown();
};
