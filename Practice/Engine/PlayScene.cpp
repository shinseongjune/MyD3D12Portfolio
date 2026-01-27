#include "PlayScene.h"
#include <DirectXMath.h>
#include <memory>
#include <format>
#include "World.h"
#include "SceneContext.h"
#include "FlightRigComponent.h"
#include "CameraRigComponent.h"
#include "GunComponent.h"
#include "MyRandom.h"
#include "StatsComponent.h"
#include "PlayerBooster.h"
#include "PhysicsSystem.h"
#include <DirectXMath.h>
#include "MissileLauncherComponent.h"
#include "PrimitiveMeshes.h"
using namespace DirectX;

static XMFLOAT3 Add(const XMFLOAT3& a, const XMFLOAT3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

static XMFLOAT3 Mul(const XMFLOAT3& v, float s)
{
    return { v.x * s, v.y * s, v.z * s };
}

static XMFLOAT3 NormalizeXZ(const XMFLOAT3& v)
{
    float x = v.x, z = v.z;
    float len = std::sqrt(x * x + z * z);
    if (len < 1e-6f) return { 0,0,1 };
    return { x / len, 0.0f, z / len };
}

// yaw: +Y축 기준 회전, (0이면 +Z를 바라본다)
static float ComputeYawToCenter_XZ(const XMFLOAT3& point, const XMFLOAT3& center)
{
    XMFLOAT3 toC{ center.x - point.x, center.y - point.y, center.z - point.z };
    XMFLOAT3 dir = NormalizeXZ(toC);
    // +Z 기준 yaw: atan2(x, z)
    return std::atan2(dir.x, dir.z);
}

void PlayScene::OnLoad(SceneContext& ctx)
{
    GameManager::GetInstance().SetState(GameManager::State::Playing);

    ctx.physics.SetGravityEnabled(false);
    SetSkybox(ctx);
    SetDirectionalLight(ctx);
	m_quad = CreateQuadMesh(ctx);
	m_bulletTex = CreateBulletTexture(ctx);
    BuildBoundariesAndSpawnPoints(ctx, 500, 5, 8, 0.95f, m_groundBoundary);
    m_enemyHud.Initialize(ctx, 64);

    // bgm
    {
        auto bgm = ctx.LoadSoundScoped("Assets/Audio/play_bgm.mp3");
        if (bgm.IsOk())
            ctx.PlayBGM(bgm.value, 1.0f);
    }

    // sfx
    {
        auto gunFire1 = ctx.LoadSoundScoped("Assets/Audio/gun1.mp3");
        auto gunFire2 = ctx.LoadSoundScoped("Assets/Audio/gun2.mp3");
        if (gunFire1.IsOk())
            m_gunFireSounds.push_back(gunFire1.value);
        if (gunFire2.IsOk())
            m_gunFireSounds.push_back(gunFire2.value);
    }

    {
        for (int i = 0; i < 3; i++) {
            auto bulletHit = ctx.LoadSoundScoped(std::format("Assets/Audio/bulletHit{}.mp3", i + 1));
            if (bulletHit.IsOk())
                m_bulletHitSounds.push_back(bulletHit.value);
        }
    }

    {
        auto missileFire = ctx.LoadSoundScoped("Assets/Audio/missile_launch.mp3");
        if (missileFire.IsOk())
            m_missileLaunchSound = missileFire.value;
    }

    {
        auto missileBoom = ctx.LoadSoundScoped("Assets/Audio/missile_boom.mp3");
        if (missileBoom.IsOk())
            m_missileBoomSound = missileBoom.value;
    }

    {
        for (int i = 0; i < 4; i++)
        {
            auto death = ctx.LoadSoundScoped(std::format("Assets/Audio/death{}.mp3", i + 1));
            if (death.IsOk())
                m_deathSounds.push_back(death.value);
        }
    }

    {
        for (int i = 0; i < 4; i++)
        {
            auto wind = ctx.LoadSoundScoped(std::format("Assets/Audio/wind{}.mp3", i + 1));
            if (wind.IsOk())
                m_windSounds.push_back(wind.value);
        }
    }

    // Import models
    Result<ModelAsset> res_model_spacefighter = ImportModel(ctx, "Assets/Model/space_fighter.obj");

    Result<ModelAsset> res_model_starcruiser = ImportModel(ctx, "Assets/Model/star_cruiser.obj");

    Result<ModelAsset> res_model_missile = ImportModel(ctx, "Assets/Model/missile.obj");
    m_missile = res_model_missile.value;

    Result<ModelAsset> res_model_map = ImportModel(ctx, "Assets/Model/map.obj");

    // Load textures
    TextureHandle texh_spacefighter = LoadTexture(ctx, "Assets/Texture/space_fighter_diffuse.png");

    TextureHandle texh_starcruiser = LoadTexture(ctx, "Assets/Texture/star_cruiser_diffuse.png");

    m_missileTex = LoadTexture(ctx, "Assets/Texture/missile.png");

    TextureHandle texh_map = LoadTexture(ctx, "Assets/Texture/map.png");

    // booster anims
    {
        for (int i = 0; i < 11; i++) 
        {
            std::string path = std::format("Assets/Texture/Booster/booster-{:02}.png", i);

            m_boosterAnims.push_back(LoadTexture(ctx, path));
        }
    }

    // death anims
    {
        for (int i = 1; i <= 3; i++)
        {
            std::vector<TextureHandle> deathAnims;

            for (int j = 0; j < 12; j++) 
            {
                std::string path = std::format("Assets/Texture/Death/death{}-{:02}.png", i, j);

                deathAnims.push_back(LoadTexture(ctx, path));
            }

            m_deathAnimsContainer.push_back(deathAnims);
        }
    }

    // bullet hit anims
    {
        for (int i = 0; i < 9; i++) 
        {
            std::string path = std::format("Assets/Texture/BulletHit/bullet_hit-{}.png", i);

            m_bulletHitAnims.push_back(LoadTexture(ctx, path));
        }
    }

    // missile boom anims
    {
        for (int i = 0; i < 6; i++) 
        {
            std::string path = std::format("Assets/Texture/MissileBoom/missile_boom-{:02}.png", i);

            m_missleHitAnims.push_back(LoadTexture(ctx, path));
        }
    }

    // missile booster anims
    {
        for (int i = 0; i < 12; i++)
        {
            std::string path = std::format("Assets/Texture/MissileBooster/missile_booster-{:02}.png", i);

            m_missleBoosterAnims.push_back(LoadTexture(ctx, path));
        }
    }

    // Materials
    MaterialComponent mat_spacefighter = CreateMaterial(ctx, texh_spacefighter);
    MaterialComponent mat_starcruiser  = CreateMaterial(ctx, texh_starcruiser);

    MaterialComponent mat_map = CreateMaterial(ctx, texh_map);

    // Spawn map
    {
        auto spawned_map = ctx.SpawnModel(res_model_map.value, SpawnModelOptions{ "Map" });
        auto map = spawned_map.value;
        ctx.world.AddMaterial(map, mat_map);
        ctx.world.SetLocalPosition(map, { 0.0f, m_groundBoundary, 0.0f });
    }

    // Spawn player
    {
        SpawnModelOptions spawnOpt{};
        spawnOpt.name = "Player";
        auto spawned_spacefighter = ctx.SpawnModel(res_model_spacefighter.value, spawnOpt);
#if defined(_DEBUG)
        if (!spawned_spacefighter.IsOk())
        {
            LOG_ERROR("Failed to Spawn space_fighter: %s", spawned_spacefighter.error->message.c_str());
            return;
        }
#endif
        m_player = spawned_spacefighter.value;
        ctx.world.AddMaterial(m_player, mat_spacefighter);
        ctx.world.SetLocalPosition(m_player, { 0.0f, 0.0f, 0.0f });

        // Attach flight rig as a Behaviour (script component)
        {
            auto rig = std::make_unique<FlightRigComponent>();
            ctx.world.AddScript(m_player, std::move(rig));
        }
        // Attach gun
        {
            auto gun = std::make_unique<GunComponent>();
            gun->SetHandles(m_quad, m_bulletTex);
            gun->SetBulletHitAnims(m_bulletHitAnims);
            gun->SetGunFireSounds(m_gunFireSounds);
            gun->SetBulletHitSounds(m_bulletHitSounds);
            ctx.world.AddScript(m_player, std::move(gun));
        }
        // Attach collider & rigidbody
        {
            ColliderComponent col;
            col.shapeType = ShapeType::Sphere;
            col.localCenter = { 0, -1.0f, 0.3f };
            col.sphere.radius = 2.0f;
            col.layer = 1;
            PhysicsMaterial pm;
            pm.restitution = 0.5f;
            col.material = pm;
            ctx.world.AddCollider(m_player, col);

            RigidBodyComponent rb;
            rb.type = BodyType::Dynamic;
            rb.useGravity = false;
            ctx.world.AddRigidBody(m_player, rb);
        }
        // Attach Stats
        {
            auto stats = std::make_unique<StatsComponent>();
            stats->SetHp(10000);
            stats->SetQuadMesh(m_quad);
            stats->SetDieAnims(m_deathAnimsContainer[0]);
            stats->SetDieSounds(m_deathSounds);
            ctx.world.AddScript(m_player, std::move(stats));
        }
        // Attach Booster
        {
            auto booster = std::make_unique<PlayerBooster>();
            booster->SetQuadMesh(m_quad);
            booster->SetBoosterAnims(m_boosterAnims);
            ctx.world.AddScript(m_player, std::move(booster));
        }
        // Attach Missile Launcher
        {
            auto missileLauncher = std::make_unique<MissileLauncherComponent>();
            missileLauncher->SetHandles(m_quad, m_missile, m_missileTex);
            missileLauncher->SetMissileHitAnims(m_missleHitAnims);
            missileLauncher->SetLaunchSound(m_missileLaunchSound);
            missileLauncher->SetBoomSound(m_missileBoomSound);
            ctx.world.AddScript(m_player, std::move(missileLauncher));
        }
        // audio source
        {
            ctx.world.AddAudioSource(m_player, AudioSourceComponent{});
        }
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
        camRig->SetTarget(m_player);
        camRig->SetFollowEnabled(true);
        ctx.world.AddScript(m_cam, std::move(camRig));
    }

    // wave setting
    {
        m_waveManager.model_cruiser = res_model_starcruiser.value;
        m_waveManager.mat_cruiser = mat_starcruiser;
        m_waveManager.m_quad = m_quad;
        m_waveManager.m_deathAnimsContainer = m_deathAnimsContainer;
        m_waveManager.m_spawns = m_spawnPositions;
        m_waveManager.SetDieSounds(m_deathSounds);
        m_waveManager.SetGunAssets(m_bulletTex, m_bulletHitAnims, m_gunFireSounds, m_bulletHitSounds);
        m_waveManager.SetMissileAssets(m_missile, m_missileTex, m_missleHitAnims, m_missileLaunchSound, m_missileBoomSound);
        m_waveManager.SetAIData(m_player, m_playBoundary, m_groundBoundary + 150);
        m_waveManager.Start(m_spawnPositions);
    }

#if defined(_DEBUG)
    // spawn pos test
    {
        for (auto e : m_spawnPositions)
        {
            static int testInt = 0;
            std::string name = std::string("test") + std::to_string(testInt++);
            auto t = ctx.SpawnModel(res_model_spacefighter.value, SpawnModelOptions{ name.c_str()});
            ctx.world.SetLocalPosition(t.value, ctx.world.GetLocalPosition(e));

            auto& tr = ctx.world.GetPendingTransform(t.value);
            ctx.world.FlushStructuralChanges();
            auto& real = ctx.world.GetTransform(t.value);
        }
    }
#endif
}

void PlayScene::OnUnload(SceneContext& ctx)
{
}

void PlayScene::OnUpdate(SceneContext& ctx)
{
    m_enemyHud.SetEnabled(GameManager::GetInstance().CurrentState() == GameManager::State::Playing);
    if (GameManager::GetInstance().CurrentState() == GameManager::State::Win || GameManager::GetInstance().CurrentState() == GameManager::State::Lose)
    {
        ctx.RequestLoadScene(SceneId::Result);
        return;
    }

    DetectBoundary(ctx);

    // 경계를 벗어난 경우
    if (m_outOfPlay)
    {
        //TODO: 경고음
        //TODO: 경고 ui
    }

    m_enemyHud.Update(ctx, m_cam, m_player, m_waveManager.GetCurrentEnemies());

    PlayerWindUpdate(ctx, m_player, m_windSounds);

    m_waveManager.Update(ctx, ctx.dt);

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

MissileLauncherComponent* PlayScene::GetMissileLauncher(SceneContext& ctx)
{
    if (!ctx.world.HasScript(m_player)) return nullptr;

    auto& sc = ctx.world.GetScript(m_player);

    // Scripts added this frame live in pendingAdd until World::FlushScripts() (EndFrame).
    for (auto& p : sc.pendingAdd)
    {
        if (!p.ptr || !p.enabled) continue;
        if (auto* gun = dynamic_cast<MissileLauncherComponent*>(p.ptr.get()))
            return gun;
    }

    for (auto& s : sc.scripts)
    {
        if (!s.ptr || !s.enabled) continue;
        if (auto* gun = dynamic_cast<MissileLauncherComponent*>(s.ptr.get()))
            return gun;
    }
    return nullptr;
}

void PlayScene::SetBoundaryRadius(SceneContext& ctx, float radius)
{
    m_playBoundary = radius;
}

void PlayScene::ExecuteCommand(SceneContext& ctx)
{
    FlightRigComponent* rig = GetFlightRig(ctx);
    CameraRigComponent* camRig = GetCameraRig(ctx);
    GunComponent* gun = GetGun(ctx);
    MissileLauncherComponent* missile = GetMissileLauncher(ctx);

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
            missile->Fire(ctx);
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
    auto tex = ctx.LoadTextureScoped(path);
    return tex.value;
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

void PlayScene::BuildBoundariesAndSpawnPoints(
    SceneContext& ctx,
    float playRadius,
    int latDiv,     // θ 분할 개수(극쪽 포함 여부는 아래 코드 참고)
    int lonDiv,     // φ 분할 개수
    float spawnRadiusRatio, // 예: 0.95f ~ 1.05f (playRadius 근처 링/쉘)
    float groundY
)
{
    // 1) Boundary spheres
    SetBoundaryRadius(ctx, playRadius);

    // 2) 중심점
    XMFLOAT3 center{ 0.f, 0.f, 0.f };

    float R = playRadius * spawnRadiusRatio;

    m_spawnPositions.clear();
    m_spawnPositions.reserve(latDiv * lonDiv);

    // 스폰 y 범위
    float minY = groundY + 150.0f;

    // 천정 제외 (여유값)
    float ceilingMargin = 50.0f;
    float maxY = R - ceilingMargin;

    // 안전 클램프
    minY = std::max(minY, -R + 1.0f);
    maxY = std::min(maxY, R - 1.0f);

    // y -> theta 변환
    float thetaMin = std::acos(maxY / R); // y가 클수록 theta는 작아짐
    float thetaMax = std::acos(minY / R);

    // 위도/경도 분할:
    // θ: (0..π). 0/π는 극점이라 lon을 돌려도 같은 점이므로,
    // 극을 피하려면 0과 π를 제외하고 (1..latDiv-1)만 쓰는 게 실용적.
    // 아래는 "극점 제외" 버전.
    for (int i = 1; i < latDiv; ++i)
    {
        float t = (float)i / (float)(latDiv - 1); // [0,1]
        float theta = thetaMin + t * (thetaMax - thetaMin);

        float sinT = std::sin(theta);
        float cosT = std::cos(theta);

        for (int j = 0; j < lonDiv; ++j)
        {
            float u = (float)j / (float)lonDiv; // [0,1)
            float phi = u * XM_2PI;

            float x = R * sinT * std::cos(phi);
            float y = R * cosT;
            float z = R * sinT * std::sin(phi);

            XMFLOAT3 pos = Add(center, { x, y, z });

            std::string name = std::string("SpawnPoint") + std::to_string(spawnPosNumber++);
            EntityId sp = ctx.Instantiate(name.c_str());
            ctx.world.AddTransform(sp);

            ctx.world.SetLocalPosition(sp, pos);

            // "중앙을 향하도록" (y rotation만)
            float yaw = ComputeYawToCenter_XZ(pos, center);

            ctx.world.SetLocalRotationEuler(sp, { 0.f, yaw, 0.f });

            auto test = ctx.world.GetPendingTransform(sp);

            m_spawnPositions.push_back(sp);
        }
    }
}

void PlayScene::DetectBoundary(SceneContext& ctx)
{
    if (!ctx.world.IsAlive(m_player) || !ctx.world.HasTransform(m_player))
        return;

    const XMFLOAT3 p = ctx.world.GetLocalPosition(m_player);

    // 0) Ground boundary: below ground => Lose
    if (p.y < m_groundBoundary)
    {
        GameManager::GetInstance().SetState(GameManager::State::Lose);
        return;
    }

    const float playR = m_playBoundary;
    const float limitR = playR * 1.2f;

    const float d2 = p.x * p.x + p.y * p.y + p.z * p.z;
    const float playR2 = playR * playR;
    const float limitR2 = limitR * limitR;

    if (d2 > limitR2)
    {
        GameManager::GetInstance().SetState(GameManager::State::Lose);
        return;
    }

    m_outOfPlay = (d2 > playR2) || p.y < m_groundBoundary + 150;
}


void PlayScene::PlayerWindUpdate(SceneContext& ctx, EntityId player, const std::vector<SoundHandle>& windClips)
{
    if (!ctx.world.HasAudioSource(player)) return;

    AudioSourceComponent& src = ctx.world.GetAudioSource(player);

    src.bus = AudioBus::SFX;
    src.loop = false;

    // 아직 재생중이면 아무것도 안 함
    if (src.playingInstanceId != 0 && ctx.audio.IsInstanceAlive(src.playingInstanceId))
        return;

    // 재생 끝났거나 아직 시작 안 했음 → 다음 클립 랜덤 선택
    src.playingInstanceId = 0;

    if (windClips.empty()) return;

    const int n = (int)windClips.size();
    const int idx = RandRangeInt(0, n - 1);
    src.clip = windClips[idx];

    ctx.audio.PlayFromEntity(player);
}