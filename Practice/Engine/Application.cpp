#include "Application.h"
#include "IRenderer.h"
#include "D3D12Renderer.h"
#include "Time.h"
#include "TestScene.h"
#include "RenderCamera.h"
#include "Input.h"
#include "DebugDraw.h"
#include "ObjImporter_Minimal.h"
#include <DirectXMath.h>
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

    auto* d3d = static_cast<D3D12Renderer*>(m_renderer.get());
    d3d->SetMeshManager(&m_meshManager);

    // 4) 월드 초기화
    // m_world.Initialize(); // 나중에 필요하면 추가

    // 4-1) 첫 Scene 로드(테스트)
    m_registry.Register(std::make_unique<ObjImporter_Minimal>());

    m_sceneManager.Load(m_world, std::make_unique<TestScene>(m_meshManager, m_pipeline));

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

        // world 프레임 갱신
        m_world.BeginFrame();

		// DebugDraw 갱신
        DebugDraw::BeginFrame();

		// Input 갱신
        Input::Update();

		// Scene 업데이트
        if (auto* scene = m_sceneManager.Current())
            scene->OnUpdate(m_world, (float)dt);

        // 월드 갱신
        m_world.UpdateTransforms();

        // 드로우 리스트 생성
        m_renderSystem.Build(m_world, m_renderItems);

        // 드로우 리스트 렌더링
        RenderCamera cam{};
        cam = BuildRenderCamera();
        m_renderer->Render(m_renderItems, cam);

		// 지연 파괴된 Entity 실제 삭제
		m_world.FlushDestroy();

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

RenderCamera Application::BuildRenderCamera() const
{
    using namespace DirectX;

    RenderCamera out{};

    // 1) 활성 카메라 찾기 (정책: 첫 CameraComponent 가진 엔티티)
    EntityId camEnt = m_world.FindActiveCamera();
    if (!m_world.IsAlive(camEnt) || !m_world.HasTransform(camEnt) || !m_world.HasCamera(camEnt))
    {
        // 폴백: 임시 카메라
        XMMATRIX V = XMMatrixLookAtLH(
            XMVectorSet(0.f, 0.f, -6.f, 1.f),
            XMVectorSet(0.f, 0.8f, 0.f, 1.f),
            XMVectorSet(0.f, 1.f, 0.f, 0.f));

        float aspect = float(m_window.GetWidth()) / float(m_window.GetHeight());
        XMMATRIX P = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.f), aspect, 0.1f, 1000.f);

        XMStoreFloat4x4(&out.view, V);
        XMStoreFloat4x4(&out.proj, P);
        return out;
    }

    const auto& camT = m_world.GetTransform(camEnt);
    const auto& camC = m_world.GetCamera(camEnt);

    // 2) 카메라 엔티티의 world matrix로부터 View 만들기
    // camT.world: 카메라의 "카메라->월드" 변환(= 카메라의 pose)
    // View는 그 역행렬(월드->카메라)
    XMMATRIX camWorld = XMLoadFloat4x4(&camT.world);
    XMMATRIX V = XMMatrixInverse(nullptr, camWorld);

    // 3) Projection 만들기
    float aspect = float(m_window.GetWidth()) / float(m_window.GetHeight());
    float fovY = camC.FovYRadians(); // 네 필드명에 맞춰 수정
    XMMATRIX P = XMMatrixPerspectiveFovLH(fovY, aspect, camC.nearZ, camC.farZ);

	// view/proj 저장
    XMStoreFloat4x4(&out.view, V);
    XMStoreFloat4x4(&out.proj, P);
    return out;
}

