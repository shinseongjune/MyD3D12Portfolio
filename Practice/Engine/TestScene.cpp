#include "TestScene.h"
#include "EntityId.h"
#include "TextureHandle.h"
#include "DebugDraw.h"
#include "World.h"
#include "Input.h"
#include "MeshCPUData.h"
#include "TextureLoader_WIC.h"
#include "SceneContext.h"
#if defined(_DEBUG)
#include "PrimitiveMeshes.h"
#endif

using namespace DirectX;

void TestScene::OnLoad(SceneContext& ctx)
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

    // 2) Instantiate
	auto spawned = ctx.SpawnModel("Assets/Alien Animal.obj", importOpt, spawnOpt);
    if (!spawned.IsOk())
    {
        LOG_ERROR("Failed to Spawn: %s", spawned.error->message.c_str());
        return;
    }
    EntityId root = spawned.value;

    // 2) 텍스처 로드 (Assets/Texture/Alien-Animal-Base-Color.jpg)
    auto texRes = LoadTextureRGBA8_WIC("Assets/Texture/Alien-Animal-Base-Color.jpg",
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

    // 2.5) Material 연결 (중요!)
    {
        // 모델이 submesh별로 materialIndex가 다를 수 있으니 슬롯을 넉넉히 만든다.
        uint32_t maxMatIndex = 256;

        MaterialComponent mat{};
        mat.slots.resize(maxMatIndex);

        // 지금은 "한 장의 텍스처"를 모든 슬롯에 공통으로 적용
        for (auto& s : mat.slots)
        {
            s.color = { 1,1,1,1 };
            s.albedo = hTex.IsValid() ? hTex : TextureHandle{ 0 };
        }

        ctx.world.AddMaterial(root, mat);
    }

    // =========================
    // 3. 위치/회전/스케일 조정
    // =========================
	ctx.world.SetLocalPosition(root, { 0.f, 0.f, 0.f });
	ctx.world.SetLocalRotation(root, { 0.f, 0.f, 0.f, 1.f });
	ctx.world.SetLocalScale(root, { 1.f, 1.f, 1.f });


#if defined(_DEBUG)
    // 디버그용: primitive mesh 생성 테스트
    {
        MeshCPUData boxMesh = PrimitiveMeshes::MakeUnitBox();
        MeshHandle boxHandle = ctx.meshes.Create(boxMesh);

        EntityId boxEntity = ctx.Instantiate("DebugBox");
        ctx.world.AddTransform(boxEntity);
        ctx.world.AddMesh(boxEntity, MeshComponent{ boxHandle });
        ctx.world.SetLocalPosition(boxEntity, { 0.f, 1.f, 0.f });

        MeshCPUData sphereMesh = PrimitiveMeshes::MakeUnitSphereUV(8, 16);
        MeshHandle sphereHandle = ctx.meshes.Create(sphereMesh);

		EntityId sphereEntity = ctx.Instantiate("DebugSphere");
        ctx.world.AddTransform(sphereEntity);
        ctx.world.AddMesh(sphereEntity, MeshComponent{ sphereHandle });
		ctx.world.SetLocalPosition(sphereEntity, { 2.f, 1.f, 0.f });

		ctx.world.SetParent(sphereEntity, boxEntity);
		ctx.world.SetParent(boxEntity, root);
    }
#endif

    // Camera
    {
		EntityId cam = ctx.Instantiate("MainCamera");

        ctx.world.AddTransform(cam);
        ctx.world.AddCamera(cam);

        ctx.world.SetLocalPosition(cam, { 0.f, 0.f, -6.f });
        ctx.world.GetCamera(cam).active = true;
    }
}

void TestScene::OnUnload(SceneContext& ctx)
{
}

void TestScene::OnUpdate(SceneContext& ctx)
{
    EntityId cam = ctx.world.FindActiveCamera();
    if (!ctx.world.IsAlive(cam)) return;

    auto& t = ctx.world.GetTransform(cam);

    const float speed = 3.0f * ctx.dt;

    XMFLOAT3 delta{ 0,0,0 };
    if (ctx.input.IsKeyDown(Key::W)) delta.z += speed;
    if (ctx.input.IsKeyDown(Key::S)) delta.z -= speed;
    if (ctx.input.IsKeyDown(Key::A)) delta.x -= speed;
    if (ctx.input.IsKeyDown(Key::D)) delta.x += speed;
    if (ctx.input.IsKeyDown(Key::Q)) delta.y -= speed;
    if (ctx.input.IsKeyDown(Key::E)) delta.y += speed;
	ctx.world.TranslateLocal(cam, delta);
}