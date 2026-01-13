#include "PlayScene.h"

#include <DirectXMath.h>
#include <memory>
#include "World.h"
#include "SceneContext.h"
#include "FlightRigComponent.h"
#include "CameraRigComponent.h"
#include "BillboardComponent.h"
#include "GunComponent.h"


void PlayScene::OnLoad(SceneContext& ctx)
{
    SetSkybox(ctx);
    SetDirectionalLight(ctx);
	m_quad = CreateQuadMesh(ctx);
	m_bulletTex = CreateBulletTexture(ctx);


    // Import models
    Result<ModelAsset> res_model_spacefighter = ImportModel(ctx, "Assets/Model/space_fighter.obj");
    if (!res_model_spacefighter.IsOk()) return;

    Result<ModelAsset> res_model_starcruiser = ImportModel(ctx, "Assets/Model/star_cruiser.obj");
    if (!res_model_starcruiser.IsOk()) return;

    // Load textures
    TextureHandle texh_spacefighter = LoadTexture(ctx, "Assets/Texture/space_fighter_diffuse.png");
    if (!texh_spacefighter.IsValid()) return;

    TextureHandle texh_starcruiser = LoadTexture(ctx, "Assets/Texture/star_cruiser_diffuse.png");
    if (!texh_starcruiser.IsValid()) return;

    // Materials
    MaterialComponent mat_spacefighter = CreateMaterial(ctx, texh_spacefighter);
    MaterialComponent mat_starcruiser  = CreateMaterial(ctx, texh_starcruiser);

    // Spawn player
    SpawnModelOptions spawnOpt{};
    spawnOpt.name = "Player";
    auto spawned_spacefighter = ctx.SpawnModel(res_model_spacefighter.value, spawnOpt);
    if (!spawned_spacefighter.IsOk())
    {
        LOG_ERROR("Failed to Spawn space_fighter: %s", spawned_spacefighter.error->message.c_str());
        return;
    }

    EntityId fighter = spawned_spacefighter.value;
    ctx.world.AddMaterial(fighter, mat_spacefighter);
    ctx.world.SetLocalPosition(fighter, { 0.0f, 0.0f, 0.0f });
    m_player = fighter;

    // Attach flight rig as a Behaviour (script component)
    {
        auto rig = std::make_unique<FlightRigComponent>();
        ctx.world.AddScript(m_player, std::move(rig));
    }
    // Attach gun
    {
        auto gun = std::make_unique<GunComponent>();
        gun->SetHandles(m_quad, m_bulletTex);
        ctx.world.AddScript(m_player, std::move(gun));
    }

    // Camera
    {
        EntityId cam = ctx.Instantiate("MainCamera");
        ctx.world.AddTransform(cam);

        CameraComponent cc{};
        ctx.world.AddCamera(cam, cc);

        ctx.world.SetLocalPosition(cam, { 0.f, 0.f, -6.f });
        cc.active = true;
        m_cam = cam;

        // Attach camera rig as a Behaviour so it updates in ScriptSystem order.
        auto camRig = std::make_unique<CameraRigComponent>();
        camRig->SetCamera(m_cam);
        camRig->SetTarget(fighter);
        camRig->SetFollowEnabled(true);
        ctx.world.AddScript(m_cam, std::move(camRig));
    }
}

void PlayScene::OnUnload(SceneContext& ctx)
{
    (void)ctx;
}

void PlayScene::OnUpdate(SceneContext& ctx)
{
    BuildShooterCommands(ctx.input, m_cmds);

    ExecuteCommand(ctx);
}

FlightRigComponent* PlayScene::GetFlightRig(SceneContext& ctx)
{
    if (!ctx.world.HasScript(m_player)) return nullptr;

    auto& sc = ctx.world.GetScript(m_player);

    // Scripts added this frame live in pendingAdd until World::FlushScripts() (EndFrame).
    for (auto& p : sc.pendingAdd)
    {
        if (!p.ptr || !p.enabled) continue;
        if (auto* rig = dynamic_cast<FlightRigComponent*>(p.ptr.get()))
            return rig;
    }

    for (auto& s : sc.scripts)
    {
        if (!s.ptr || !s.enabled) continue;
        if (auto* rig = dynamic_cast<FlightRigComponent*>(s.ptr.get()))
            return rig;
    }
    return nullptr;
}

CameraRigComponent* PlayScene::GetCameraRig(SceneContext& ctx)
{
    if (!ctx.world.IsAlive(m_cam) || !ctx.world.HasScript(m_cam)) return nullptr;

    auto& sc = ctx.world.GetScript(m_cam);

    for (auto& p : sc.pendingAdd)
    {
        if (!p.ptr || !p.enabled) continue;
        if (auto* rig = dynamic_cast<CameraRigComponent*>(p.ptr.get()))
            return rig;
    }

    for (auto& s : sc.scripts)
    {
        if (!s.ptr || !s.enabled) continue;
        if (auto* rig = dynamic_cast<CameraRigComponent*>(s.ptr.get()))
            return rig;
    }
    return nullptr;
}

GunComponent* PlayScene::GetGun(SceneContext& ctx)
{
    if (!ctx.world.HasScript(m_player)) return nullptr;

    auto& sc = ctx.world.GetScript(m_player);

    // Scripts added this frame live in pendingAdd until World::FlushScripts() (EndFrame).
    for (auto& p : sc.pendingAdd)
    {
        if (!p.ptr || !p.enabled) continue;
        if (auto* gun = dynamic_cast<GunComponent*>(p.ptr.get()))
            return gun;
    }

    for (auto& s : sc.scripts)
    {
        if (!s.ptr || !s.enabled) continue;
        if (auto* gun = dynamic_cast<GunComponent*>(s.ptr.get()))
            return gun;
    }
    return nullptr;
}

void PlayScene::ExecuteCommand(SceneContext& ctx)
{
    FlightRigComponent* rig = GetFlightRig(ctx);
    CameraRigComponent* camRig = GetCameraRig(ctx);
    GunComponent* gun = GetGun(ctx);

    FlightInput flightIn{};
    bool hasMove = false;

    for (const auto& cmd : m_cmds)
    {
        switch (cmd.action)
        {
        case ShooterAction::Move:
            // Aggregate move input for this frame.
            flightIn.throttleDelta += cmd.throttle;
            flightIn.yaw += cmd.yaw;
            flightIn.roll += cmd.roll;
            flightIn.pitch += cmd.pitch;
            hasMove = true;
            break;
        case ShooterAction::FireGun:
            gun->Fire(ctx);
            break;
        case ShooterAction::FireMissile:
            break;
        case ShooterAction::CameraLook:
            if (camRig) camRig->OnLook(cmd.camX, cmd.camY);
            break;
        }
    }

    // Feed input to the rig only through ExecuteCommand (no direct Input usage in rig/system)
    if (rig)
    {
        if (hasMove)
            rig->SetInput(flightIn);
        else
            rig->ClearInput();
    }
}

void PlayScene::SetSkybox(SceneContext& ctx)
{
    auto skyRes = ctx.LoadCubemapScoped({
        "Assets/Skybox/Sky_01_right.png",
        "Assets/Skybox/Sky_01_left.png",
        "Assets/Skybox/Sky_01_top.png",
        "Assets/Skybox/Sky_01_bottom.png",
        "Assets/Skybox/Sky_01_front.png",
        "Assets/Skybox/Sky_01_back.png"
    });

    if (skyRes.IsOk())
        ctx.SetSkybox(skyRes.value);
    else
        ctx.ClearSkybox();
}

void PlayScene::SetDirectionalLight(SceneContext& ctx)
{
    // Main light
    EntityId light = ctx.Instantiate("MainDirectionalLight");
    ctx.world.AddTransform(light);

    LightComponent s{};
    s.type = LightType::Directional;
    s.color = { 1, 0.87f, 0.87f };
    s.intensity = 8.0f;
    ctx.world.AddLight(light, s);

    ctx.world.SetLocalRotationEuler(light, {
        DirectX::XMConvertToRadians(10.0f),
        DirectX::XMConvertToRadians(-90.0f),
        0.0f
    });

    // Fill light
    EntityId light2 = ctx.Instantiate("SubDirectionalLight");
    ctx.world.AddTransform(light2);

    LightComponent s2{};
    s2.type = LightType::Directional;
    s2.color = { 0.8f, 0.5f, 0.6f };
    s2.intensity = 3.5f;
    ctx.world.AddLight(light2, s2);

    ctx.world.SetLocalRotationEuler(light2, {
        DirectX::XMConvertToRadians(0.0f),
        DirectX::XMConvertToRadians(90.0f),
        0.0f
    });
}

Result<ModelAsset> PlayScene::ImportModel(SceneContext& ctx, const std::string& path)
{
    ImportOptions importOpt{};
    importOpt.triangulate = true;
    importOpt.generateNormalsIfMissing = true;
    importOpt.flipV = true;
    importOpt.uniformScale = 1.0f;

    auto imported = ctx.ImportModel(path, importOpt);
#if defined(_DEBUG)
    if (!imported.IsOk())
        LOG_ERROR("Failed to import %s: %s", path.c_str(), imported.error->message.c_str());
#endif
    return imported;
}

TextureHandle PlayScene::LoadTexture(SceneContext& ctx, const std::string& path)
{
    auto tex_res = LoadTextureRGBA8_WIC(path, ImageColorSpace::SRGB, /*flipY=*/false);
    TextureHandle tex{};
    if (tex_res.IsOk())
    {
        tex = ctx.textures.Create(std::move(tex_res.value));
    }
    else
    {
#if defined(_DEBUG)
        LOG_ERROR("Failed to load texture: %s", tex_res.error->message.c_str());
#endif
    }
    return tex;
}

MaterialComponent PlayScene::CreateMaterial(SceneContext& ctx, const TextureHandle& tex)
{
    (void)ctx;

    MaterialComponent mat{};
    mat.slots.resize(256);
    for (auto& s : mat.slots)
    {
        s.color = { 1,1,1,1 };
        s.albedo = tex.IsValid() ? tex : TextureHandle{ 0 };
    }
    return mat;
}

MeshHandle PlayScene::CreateQuadMesh(SceneContext& ctx)
{
    MeshHandle quad = ctx.meshes.Create(PrimitiveMeshes::MakeUnitQuad(/*flipV=*/false));
    ctx.scope.Track(quad);
    return quad;
}

TextureHandle PlayScene::CreateBulletTexture(SceneContext& ctx)
{
    auto texR = ctx.LoadTextureScoped("assets/texture/bullet.png");
    TextureHandle bulletTex = texR.IsOk() ? texR.value : TextureHandle{ 0 };
	return bulletTex;
}
