#include "PhysicsTestScene.h"
#include "SceneContext.h"
#include "World.h"
#include "Input.h"
#include "MaterialComponent.h"
#include "MeshCPUData.h"
#include "ColliderComponent.h"
#include "RigidBodyComponent.h"
#include "PrimitiveMeshes.h"
#include "DebugDraw.h"
#include "PhysicsSystem.h"
#include <string>

using namespace DirectX;

static void DrawCross(const DirectX::XMFLOAT3& p, float s, const DirectX::XMFLOAT4& c)
{
    using namespace DirectX;
    DebugDraw::Line({ p.x - s, p.y, p.z }, { p.x + s, p.y, p.z }, c);
    DebugDraw::Line({ p.x, p.y - s, p.z }, { p.x, p.y + s, p.z }, c);
    DebugDraw::Line({ p.x, p.y, p.z - s }, { p.x, p.y, p.z + s }, c);
}

EntityId PhysicsTestScene::CreateCameraIfMissing(SceneContext& ctx)
{
    EntityId cam = ctx.world.FindActiveCamera();
    if (ctx.world.IsAlive(cam))
        return cam;

    cam = ctx.Instantiate("MainCamera");
    ctx.world.AddTransform(cam);
    ctx.world.AddCamera(cam);

    auto& cc = ctx.world.GetCamera(cam);
    cc.active = true;

    // 적당히 멀리
    ctx.world.SetLocalPosition(cam, { 0.0f, 2.0f, -6.0f });
    ctx.world.SetLocalRotation(cam, { 0.0f, 0.0f, 0.0f, 1.0f });
    ctx.world.SetLocalScale(cam, { 1.0f, 1.0f, 1.0f });

    return cam;
}

EntityId PhysicsTestScene::CreateGround(SceneContext& ctx)
{
    EntityId e = ctx.Instantiate("Ground");
    ctx.world.AddTransform(e);

    // 렌더링용
    ctx.world.AddMesh(e, MeshComponent{ m_boxMesh });
    ctx.world.AddMaterial(e, MaterialComponent{ XMFLOAT4{0.7f, 0.7f, 0.7f, 1.0f}, TextureHandle{0} });

    // 위치/스케일: 바닥 판
    ctx.world.SetLocalPosition(e, { 0.0f, -0.5f, 0.0f });
    ctx.world.SetLocalRotation(e, { 0.0f, 0.0f, 0.0f, 1.0f });
    ctx.world.SetLocalScale(e, { 20.0f, 1.0f, 20.0f });

    // 물리: Static + Box collider
    {
        RigidBodyComponent rb{};
        rb.type = BodyType::Static;
        rb.mass = 0.0f;
        rb.RecalcInvMass();
        ctx.world.AddRigidBody(e, rb);

        ColliderComponent col{};
        col.shapeType = ShapeType::Box;
        col.isTrigger = false;
        col.localCenter = { 0,0,0 };

        // unit box(half=0.5) + Transform scale로 실제 크기 결정
        col.box.halfExtents = { 0.5f, 0.5f, 0.5f };

        col.material.restitution = 0.0f;
        col.material.friction = 0.6f;

        ctx.world.AddCollider(e, col);
    }

    return e;
}

EntityId PhysicsTestScene::CreateBall(SceneContext& ctx, const XMFLOAT3& pos)
{
    EntityId e = ctx.Instantiate("Ball");
    ctx.world.AddTransform(e);

    // 렌더링용
    ctx.world.AddMesh(e, MeshComponent{ m_sphereMesh });
    ctx.world.AddMaterial(e, MaterialComponent{ XMFLOAT4{0.2f, 0.8f, 1.0f, 1.0f}, TextureHandle{0} });

    ctx.world.SetLocalPosition(e, pos);
    ctx.world.SetLocalRotation(e, { 0.0f, 0.0f, 0.0f, 1.0f });
    ctx.world.SetLocalScale(e, { 1.0f, 1.0f, 1.0f });

    // 물리: Dynamic + Sphere collider
    {
        RigidBodyComponent rb{};
        rb.type = BodyType::Dynamic;
        rb.mass = 1.0f;
        rb.useGravity = true;
        rb.gravityScale = 1.0f;
        rb.linearDamping = 0.01f;
        rb.velocity = { 0,0,3 };
        rb.RecalcInvMass();
        ctx.world.AddRigidBody(e, rb);

        ColliderComponent col{};
        col.shapeType = ShapeType::Sphere;
        col.isTrigger = false;
        col.localCenter = { 0,0,0 };

        // unit sphere(반지름 0.5) 기준
        col.sphere.radius = 0.5f;

        // 통통 튀는지 확인하려고 약간 반발
        col.material.restitution = 0.1f;
        col.material.friction = 0.3f;

        ctx.world.AddCollider(e, col);
    }

    return e;
}

void PhysicsTestScene::ResetWorld(SceneContext& ctx)
{
    // 공들 삭제
    for (EntityId e : m_balls)
    {
        if (ctx.world.IsAlive(e))
            ctx.world.RequestDestroy(e);
    }
    m_balls.clear();

    // 새 공 1개
    m_ball = CreateBall(ctx, { 0.0f, 3.0f, 0.0f });
    m_balls.push_back(m_ball);

    // 중력 기본 ON
    m_gravityOn = true;
    ctx.physics.SetGravityEnabled(true);
}

void PhysicsTestScene::OnLoad(SceneContext& ctx)
{
    CreateCameraIfMissing(ctx);

#if defined(_DEBUG)
    // 테스트용 primitive mesh 준비
    if (!m_boxMesh.IsValid())
    {
        MeshCPUData box = PrimitiveMeshes::MakeUnitBox();
        m_boxMesh = ctx.meshes.Create(box);
    }
    if (!m_sphereMesh.IsValid())
    {
        MeshCPUData sph = PrimitiveMeshes::MakeUnitSphereUV(8, 16);
        m_sphereMesh = ctx.meshes.Create(sph);
    }
#else
    // 릴리즈 빌드에서도 쓸거면 primitive mesh를 엔진 공용으로 두거나, 모델 import로 대체
    // 여기서는 디버그 우선.
#endif

    m_ground = CreateGround(ctx);

    m_balls.clear();
    m_ball = CreateBall(ctx, { 0.0f, 3.0f, 0.0f });
    m_balls.push_back(m_ball);

    m_gravityOn = true;
    ctx.physics.SetGravityEnabled(true);

    // 사운드 테스트
    auto bgm = ctx.LoadSoundShared("Assets/Audio/bgm.mp3");
    if (bgm.IsOk())
        ctx.PlayBGM(bgm.value, 0.6f);

    // UI 테스트
    EntityId ui = ctx.Instantiate("HPBarBG");

    // 텍스쳐
    auto texRes = LoadTextureRGBA8_WIC("Assets/Texture/Alien-Animal_eye.jpg",
        ImageColorSpace::SRGB, /*flipY=*/false);
    TextureHandle hTex{};
    if (texRes.IsOk())
    {
        hTex = ctx.textures.Create(std::move(texRes.value));
    }
    else
    {
        LOG_ERROR("Failed to load texture: %s", texRes.error->message.c_str());
        // hTex invalid -> 기본 텍스처(slot0)로 렌더됨
    }

    UIElementComponent u{};
    u.anchor = { 0.0f, 0.0f };
    u.pivot = { 0.0f, 0.0f };
    u.anchoredPosPx = { 20.0f, 20.0f };
    u.sizePx = { 300.0f, 24.0f };

    u.texture = hTex;
    u.color = { 1,1,1,1 };
    u.z = 0.0f;

    ctx.world.AddUIElement(ui, u);
}

void PhysicsTestScene::OnUnload(SceneContext& ctx)
{
}

void PhysicsTestScene::OnUpdate(SceneContext& ctx)
{
    EntityId cam = ctx.world.FindActiveCamera();
    if (!ctx.world.IsAlive(cam)) return;

    const float speed = 3.0f * ctx.dt;

    XMFLOAT3 delta{ 0,0,0 };
    if (ctx.input.IsKeyDown(Key::W)) delta.z += speed;
    if (ctx.input.IsKeyDown(Key::S)) delta.z -= speed;
    if (ctx.input.IsKeyDown(Key::A)) delta.x -= speed;
    if (ctx.input.IsKeyDown(Key::D)) delta.x += speed;
    if (ctx.input.IsKeyDown(Key::Q)) delta.y -= speed;
    if (ctx.input.IsKeyDown(Key::E)) delta.y += speed;

    ctx.world.TranslateLocal(cam, delta);

    // --- 샌드박스 입력 ---
    if (ctx.input.IsKeyPressed(Key::Space))
    {
        // 카메라 앞쪽에 공 생성 (단순하게 z=0으로)
        EntityId cam = ctx.world.FindActiveCamera();
        XMFLOAT3 camPos = ctx.world.GetWorldPosition(cam);

        XMFLOAT3 spawnPos{ camPos.x, camPos.y + 1.0f, camPos.z + 4.0f };
        EntityId b = CreateBall(ctx, spawnPos);
        m_balls.push_back(b);
    }

    if (ctx.input.IsKeyPressed(Key::R))
    {
        ResetWorld(ctx);
    }

    if (ctx.input.IsKeyPressed(Key::G))
    {
        m_gravityOn = !m_gravityOn;
        ctx.physics.SetGravityEnabled(m_gravityOn);
    }

    // --- 충돌 이벤트에 따라 색 바꾸기 ---
    std::vector<CollisionEvent> evs;
    ctx.world.DrainCollisionEvents(evs);

    for (const auto& ev : evs)
    {
        // 테스트 목적: 공이 관련된 이벤트만 색 바꾸기
        auto applyColor = [&](EntityId e, const XMFLOAT4& col)
            {
                if (!ctx.world.IsAlive(e) || !ctx.world.HasMaterial(e)) return;
                auto& m = ctx.world.GetMaterial(e);
                m.Primary().color = col;
            };

        // Enter: 노랑, Stay: 주황, Exit: 하늘색(원래색)
        if (ev.type == CollisionEventType::Enter)
        {
            applyColor(ev.a, { 1,1,0,1 });
            applyColor(ev.b, { 1,1,0,1 });
        }
        else if (ev.type == CollisionEventType::Stay)
        {
            applyColor(ev.a, { 1,0.6f,0,1 });
            applyColor(ev.b, { 1,0.6f,0,1 });
        }
        else // Exit
        {
            applyColor(ev.a, { 0.2f,0.8f,1,1 });
            applyColor(ev.b, { 0.2f,0.8f,1,1 });
        }
    }

    ctx.DrawText(12.0f, 12.0f, L"한글 테스트", 18.0f);
    ctx.DrawText(12.0f, 36.0f, L"Hello DWrite", 18.0f, { 1,1,0,1 }, L"Segoe UI");
}
