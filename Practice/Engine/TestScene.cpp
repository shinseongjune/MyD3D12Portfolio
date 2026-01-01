#include "TestScene.h"
#include "DebugDraw.h"
#include "World.h"
#include "Input.h"
#include "MeshCPUData.h"
#include "TextureLoader_WIC.h"
#if defined(_DEBUG)
#include "PrimitiveMeshes.h"
#endif

using namespace DirectX;

void TestScene::OnLoad(World& world)
{
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

    // 2) 텍스처 로드 (Assets/Texture/Alien-Animal-Base-Color.jpg)
    auto texRes = LoadTextureRGBA8_WIC("Assets/Texture/Alien-Animal-Base-Color.jpg",
        ImageColorSpace::SRGB, /*flipY=*/false);
    TextureHandle hTex{};
    if (texRes.IsOk())
    {
        hTex = m_textureManager.Create(std::move(texRes.value));
        m_ownedTextures.push_back(hTex);
    }
    else
    {
        LOG_ERROR("Failed to load texture: %s", texRes.error->message.c_str());
        // hTex invalid -> 기본 텍스처(slot0)로 렌더됨
    }

    // 2.5) Material 연결 (중요!)
    {
        // 모델이 submesh별로 materialIndex가 다를 수 있으니 슬롯을 넉넉히 만든다.
        uint32_t maxMatIndex = 0;

        for (const auto& m : asset.meshes)
        {
            for (const auto& sm : m.submeshes)
            {
                if (sm.materialIndex > maxMatIndex)
                    maxMatIndex = sm.materialIndex;
            }
        }

        MaterialComponent mat{};
        mat.slots.resize(maxMatIndex + 1);

        // 지금은 "한 장의 텍스처"를 모든 슬롯에 공통으로 적용
        for (auto& s : mat.slots)
        {
            s.color = { 1,1,1,1 };
            s.albedo = hTex.IsValid() ? hTex : TextureHandle{ 0 };
        }

        world.AddMaterial(root, mat);
    }

    // =========================
    // 3. 위치/회전/스케일 조정
    // =========================
	world.SetLocalPosition(root, { 0.f, 0.f, 0.f });
	world.SetLocalRotation(root, { 0.f, 0.f, 0.f, 1.f });
	world.SetLocalScale(root, { 1.f, 1.f, 1.f });


#if defined(_DEBUG)
    // 디버그용: primitive mesh 생성 테스트
    {
        MeshCPUData boxMesh = PrimitiveMeshes::MakeUnitBox();
        MeshHandle boxHandle = m_meshManager.Create(boxMesh);

        EntityId boxEntity = world.CreateEntity("DebugBox");
        m_spawned.push_back(boxEntity);
        world.AddTransform(boxEntity);
        world.AddMesh(boxEntity, MeshComponent{ boxHandle });
        world.SetLocalPosition(boxEntity, { 0.f, 1.f, 0.f });

        MeshCPUData sphereMesh = PrimitiveMeshes::MakeUnitSphereUV(8, 16);
        MeshHandle sphereHandle = m_meshManager.Create(sphereMesh);

		EntityId sphereEntity = world.CreateEntity("DebugSphere");
        m_spawned.push_back(sphereEntity);
        world.AddTransform(sphereEntity);
        world.AddMesh(sphereEntity, MeshComponent{ sphereHandle });
		world.SetLocalPosition(sphereEntity, { 2.f, 1.f, 0.f });

		world.GetTransform(sphereEntity).parent = boxEntity;
		world.GetTransform(boxEntity).children.push_back(sphereEntity);
        world.GetTransform(boxEntity).parent = root;
		world.GetTransform(root).children.push_back(boxEntity);
    }
#endif

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
    // 텍스처 먼저 정리(월드 엔티티 파괴와 독립)
    for (auto& h : m_ownedTextures)
    {
        if (h.IsValid())
            m_textureManager.Destroy(h);
    }
    m_ownedTextures.clear();

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

#if defined(_DEBUG)
    static float time = 0.0f;
    time += deltaTime;

    const float amplitude = 20.0f;              // 좌우폭
    const float frequencyHz = 0.2f;            // 초당 0.2회 왕복(느리게)
    const float omega = 2.0f * 3.14159265f * frequencyHz;

	float moveSpeed = amplitude * sin(omega * time);
	EntityId root = world.FindByName("AlienAnimal");
    world.SetLocalPosition(root, { moveSpeed, 0, 0 });

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
#endif
}