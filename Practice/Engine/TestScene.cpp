#include "TestScene.h"
#include "DebugDraw.h"
#include "World.h"
#include "Input.h"
#include "MeshCPUData.h"

using namespace DirectX;

static MeshCPUData CreateCubeMesh()
{
    MeshCPUData mesh;

    mesh.positions = {
        {-0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f},
        {-0.5f,-0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f},
    };

    mesh.indices = {
        0,1,2, 0,2,3,
        4,6,5, 4,7,6,
        4,5,1, 4,1,0,
        3,2,6, 3,6,7,
        1,5,6, 1,6,2,
        4,0,3, 4,3,7
    };

    return mesh;
}

void TestScene::OnLoad(World& world)
{
    //// 1) 큐브 메쉬 등록
    //MeshResource cube = CreateCubeMesh();
    //MeshHandle cubeHandle = m_meshManager.Create(cube);
    //
    //// 2) 엔티티 생성
    //EntityId e = world.CreateEntity("Cube");
    //
    //world.AddTransform(e);
    //world.AddMesh(e, MeshComponent{ cubeHandle });

    // =========================
    // 1. Import / Spawn 옵션
    // =========================
    ImportOptions importOpt{};
    importOpt.triangulate = true;
    importOpt.generateNormalsIfMissing = true;
    importOpt.flipV = true;          // 텍스처 뒤집힘 방지(지금은 의미 거의 없음)
    importOpt.uniformScale = 1.0f;   // 필요하면 0.01f 같은 값으로 조절

    SpawnModelOptions spawnOpt{};
    spawnOpt.name = "AlienAnimal";

    // 1) Import
    auto imported = m_assetPipeline.ImportModel("Assets/Alien Animal.obj", importOpt);
    if (!imported.IsOk())
    {
        LOG_ERROR("Failed to import: %s", imported.error->message.c_str());
        return;
    }
    ModelAsset asset = std::move(imported.value);

    // 2) Instantiate
    auto spawned = m_assetPipeline.InstantiateModel(world, asset, spawnOpt);
    if (!spawned.IsOk())
    {
        LOG_ERROR("Failed to instantiate: %s", spawned.error->message.c_str());
        return;
    }
    EntityId root = spawned.value;

    // =========================
    // 3. 위치/회전/스케일 조정
    // =========================
	world.SetLocalPosition(root, { 0.f, 0.f, 0.f });
	world.SetLocalRotation(root, { 0.f, 0.f, 0.f, 1.f });
	world.SetLocalScale(root, { 1.f, 1.f, 1.f });

    // Camera
    {
        EntityId cam = world.CreateEntity("MainCamera");
        m_spawned.push_back(cam);

        world.AddTransform(cam);
        world.AddCamera(cam);

        world.SetLocalPosition(cam, { 0.f, 0.f, -6.f });
        world.GetCamera(cam).active = true;
    }
}

void TestScene::OnUnload(World& world)
{
    // 역순 파괴(부모-자식 관계가 있을 수 있어 안전)
    for (auto it = m_spawned.rbegin(); it != m_spawned.rend(); ++it)
        world.RequestDestroy(*it);
    m_spawned.clear();
}

void TestScene::OnUpdate(World& world, float deltaTime)
{
    EntityId cam = world.FindActiveCamera();
    if (!world.IsAlive(cam)) return;

    auto& t = world.GetTransform(cam);

    const float speed = 3.0f * deltaTime;

    XMFLOAT3 delta{ 0,0,0 };
    if (Input::IsKeyDown(Key::W)) delta.z += speed;
    if (Input::IsKeyDown(Key::S)) delta.z -= speed;
    if (Input::IsKeyDown(Key::A)) delta.x -= speed;
    if (Input::IsKeyDown(Key::D)) delta.x += speed;
    if (Input::IsKeyDown(Key::Q)) delta.y -= speed;
    if (Input::IsKeyDown(Key::E)) delta.y += speed;
	world.TranslateLocal(cam, delta);

    // Debug
    DebugDraw::Line(
        XMFLOAT3{ 0,0,0 },
        XMFLOAT3{ 1,0,0 },
        XMFLOAT4{ 1,0,0,1 }); // X축

    DebugDraw::Line(
        XMFLOAT3{ 0,0,0 },
        XMFLOAT3{ 0,1,0 },
        XMFLOAT4{ 0,1,0,1 }); // Y축

    DebugDraw::Line(
        XMFLOAT3{ 0,0,0 },
        XMFLOAT3{ 0,0,1 },
        XMFLOAT4{ 0,0,1,1 }); // Z축
}