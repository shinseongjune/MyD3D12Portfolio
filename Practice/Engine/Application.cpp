#include "Application.h"
#include "IRenderer.h"
#include "D3D12Renderer.h"
#include "Time.h"
#include "RenderCamera.h"
#include "Input.h"
#include "DebugDraw.h"
#include "ObjImporter_Minimal.h"
#include <DirectXMath.h>
#include <stdexcept>
#include <Windows.h>
#if defined(_DEBUG)
#include "TestScene.h"
#include "PhysicsTestScene.h"
#endif

Application::~Application() = default;

void Application::Initialize(HINSTANCE hInstance)
{
    // 1) 창 만들기
    if (!m_window.Create(hInstance, L"Engine", 1280, 720))
        throw std::runtime_error("Failed to create window.");

    // 2) time 초기화
    Time::Initialize();

    // 3) 렌더러 만들기
    m_renderer = std::make_unique<D3D12Renderer>();
    m_renderer->Initialize(m_window.GetHwnd(), m_window.GetWidth(), m_window.GetHeight());

    auto* d3d = static_cast<D3D12Renderer*>(m_renderer.get());
    d3d->SetMeshManager(&m_meshManager);
    d3d->SetTextureManager(&m_textureManager);

	// 4) RenderSystem 초기화
    m_audioSystem.Initialize();

    // 5) 첫 Scene 로드(테스트)
    m_registry.Register(std::make_unique<ObjImporter_Minimal>());

    m_sceneManager.Load(std::make_unique<PhysicsTestScene>());

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

        Resize();
        BeginFrame();
        UpdateScene(m_dt);
		TickFixed(m_dt);
        UpdateTransforms();
        UpdateSystems();
        RenderFrame();
        EndFrame();

        // 임시: CPU 100% 방지(나중엔 Time/FPS 제어로 대체)
        Sleep(1);
    }
}

void Application::Shutdown()
{
    // Scene 정리
    m_sceneManager.Load(nullptr);

	// 오디오 시스템 정리
    m_audioSystem.Shutdown();

	// 렌더러 정리
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

    // 2) camera pos/rot로 LookToLH 뷰 구성 (관례 꼬임 방지)
    XMFLOAT3 p = camT.position;
    XMFLOAT4 q = camT.rotation;

    XMVECTOR pos = XMVectorSet(p.x, p.y, p.z, 1.0f);
    XMVECTOR quat = XMVectorSet(q.x, q.y, q.z, q.w);

    // 엔진의 "카메라 기본 전방"을 +Z로 가정(LH)
    XMVECTOR fwd = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), quat);
    XMVECTOR up = XMVector3Rotate(XMVectorSet(0, 1, 0, 0), quat);

    XMMATRIX V = XMMatrixLookToLH(pos, fwd, up);

    // 3) Projection 만들기
    float aspect = float(m_window.GetWidth()) / float(m_window.GetHeight());
    float fovY = camC.FovYRadians(); // 네 필드명에 맞춰 수정
    XMMATRIX P = XMMatrixPerspectiveFovLH(fovY, aspect, camC.nearZ, camC.farZ);

	// view/proj 저장
    XMStoreFloat4x4(&out.view, V);
    XMStoreFloat4x4(&out.proj, P);
    return out;
}

void Application::Resize()
{
    // 리사이즈 감지 (WM_SIZE에서 width/height 갱신됨)
    const uint32_t w = m_window.GetWidth();
    const uint32_t h = m_window.GetHeight();
    if ((w != m_lastW || h != m_lastH) && w != 0 && h != 0)
    {
        m_renderer->Resize(w, h);
        m_lastW = w;
        m_lastH = h;
    }
}

void Application::BeginFrame()
{
    // time 갱신
    Time::Tick();
    m_dt = Time::DeltaTime();

    // world 프레임 갱신
    m_world.BeginFrame();

    // DebugDraw 갱신
    DebugDraw::BeginFrame();

    // Input 갱신
    m_input.Update();
}

void Application::UpdateScene(const double dt)
{
    // Scene 업데이트
    m_sceneManager.Update((float)dt);
}

void Application::TickFixed(const double dtIn)
{
    double dt = dtIn;
    if (dt > m_maxAccum) dt = m_maxAccum;
    m_accum += dt;

    while (m_accum >= m_fixedDt)
    {
        // 1) 사용자 고정 업데이트 (입력/힘/의도 적용 등)
        m_sceneManager.FixedUpdate((float)m_fixedDt);

        // 2) 엔진 내부 물리 스텝
        m_physics.Step(m_world, (float)m_fixedDt);

        m_accum -= m_fixedDt;
    }
}

void Application::UpdateTransforms()
{
    // 월드 갱신
    m_world.UpdateTransforms();
}

void Application::UpdateSystems()
{
	// 오디오 시스템 업데이트
    m_audioSystem.Update(m_world, m_soundManager);
    // 드로우 리스트 생성
    m_renderSystem.Build(m_world, m_renderItems);
    // UI render queue
    m_uiHud.Build(m_world, m_window.GetWidth(), m_window.GetHeight(), m_uiItems);
}

void Application::RenderFrame()
{
    // 드로우 리스트 렌더링
    RenderCamera cam{};
    cam = BuildRenderCamera();

    m_renderer->Render(m_renderItems, cam, m_uiItems);
}

void Application::EndFrame()
{
    // 지연 파괴된 Entity 실제 삭제
    m_world.FlushDestroy();
}

