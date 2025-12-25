#include "Application.h"
#include "IRenderer.h"
#include "D3D12Renderer.h"
#include "Time.h"
#include "TestScene.h"

#include <stdexcept>
#include <Windows.h>

Application::~Application() = default;

void Application::Initialize(HINSTANCE hInstance)
{
    // 1) 창 만들기
    if (!m_window.Create(hInstance, L"Engine", 1280, 720))
        throw std::runtime_error("Failed to create window.");

    // 2) time 초기화
    Time::Initialize();

    // 3) 렌더러 만들기(일단 NullRenderer)
    m_renderer = std::make_unique<D3D12Renderer>();
    m_renderer->Initialize(m_window.GetHwnd(), m_window.GetWidth(), m_window.GetHeight());

    // 4) 월드 초기화
    // m_world.Initialize(); // 나중에 필요하면 추가

    // 4-1) 첫 Scene 로드(테스트)
    m_sceneManager.Load(m_world, std::make_unique<TestScene>());

    m_lastW = m_window.GetWidth();
    m_lastH = m_window.GetHeight();

    m_running = true;
}

void Application::Run()
{
    while (m_running)
    {
        // 메시지 처리 (WM_QUIT면 false)
        if (!m_window.PumpMessages())
        {
            m_running = false;
            break;
        }

        // 리사이즈 감지 (WM_SIZE에서 width/height 갱신됨)
        const uint32_t w = m_window.GetWidth();
        const uint32_t h = m_window.GetHeight();
        if ((w != m_lastW || h != m_lastH) && w != 0 && h != 0)
        {
            m_renderer->Resize(w, h);
            m_lastW = w;
            m_lastH = h;
        }

        // time 갱신
        Time::Tick();
        const double dt = Time::DeltaTime();

        // 월드 갱신
        m_world.UpdateTransforms();

        // 드로우 리스트 생성
        m_renderSystem.Build(m_world, m_renderItems);

        // 드로우 리스트 렌더링
        m_renderer->Render(m_renderItems);

        // 임시: CPU 100% 방지(나중엔 Time/FPS 제어로 대체)
        Sleep(1);
    }
}

void Application::Shutdown()
{
    // Scene 정리
    m_sceneManager.Load(m_world, nullptr);

    if (m_renderer)
    {
        m_renderer->Shutdown();
        m_renderer.reset();
    }

    m_window.Destroy();
    m_running = false;
}
