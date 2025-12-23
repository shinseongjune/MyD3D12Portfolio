#include "Application.h"
#include "IRenderer.h"
#include "D3D12Renderer.h"
#include "Time.h"

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

    m_lastW = m_window.GetWidth();
    m_lastH = m_window.GetHeight();

    m_running = true;


    // ==== 테스트 엔티티 생성 ====
    EntityId player = m_world.CreateEntity("Player");
    EntityId mouth = m_world.CreateEntity("MouthSocket");

    m_world.EnsureTransform(player);
    m_world.EnsureTransform(mouth);

    // mouth를 player의 자식(소켓)으로
    m_world.SetParent(mouth, player);

    // player는 원점 근처, mouth는 머리/입 위치로 오프셋
    m_world.SetLocalPosition(player, DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f });
    m_world.SetLocalPosition(mouth, DirectX::XMFLOAT3{ 0.3f, 1.2f, 0.0f });
    m_world.SetLocalScale(mouth, DirectX::XMFLOAT3{ 0.3f, 0.3f, 0.3f });
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


        // ==== 테스트: player를 좌우로 흔들기 ====
        static double t = 0.0;
        t += dt;

        EntityId player = m_world.FindByName("Player");
        if (player.IsValid())
        {
            auto p = m_world.GetLocalPosition(player);
            p.x = (float)(sinf((float)t) * 5.0f); // -2~2
            m_world.SetLocalPosition(player, p);
        }

		EntityId mouth = m_world.FindByName("MouthSocket");
        static double acc = 0;
        acc += dt;
        if (acc > 0.5)
        {
            acc = 0;
            auto lp = m_world.GetLocalPosition(mouth);
            wchar_t buf[128];
            swprintf_s(buf, L"mouth local: %.3f %.3f %.3f\n", lp.x, lp.y, lp.z);
            //OutputDebugStringW(buf);
        }

        auto pw = m_world.GetWorldPosition(player);
        auto mw = m_world.GetWorldPosition(mouth);
        wchar_t buf[256];
        swprintf_s(buf, L"player.x=%.3f mouth.x=%.3f\nplayer.x - mouth.x = %.3f\n", pw.x, mw.x, pw.x - mw.x);
        OutputDebugStringW(buf);

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
    if (m_renderer)
    {
        m_renderer->Shutdown();
        m_renderer.reset();
    }

    m_window.Destroy();
    m_running = false;
}
