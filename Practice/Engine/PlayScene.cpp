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

static float MoveToward(float cur, float target, float maxDelta)
{
    float d = target - cur;
    if (d > maxDelta) return cur + maxDelta;
    if (d < -maxDelta) return cur - maxDelta;
    return target;
}

static float ApplyRate(float cur, float target, float riseRate, float fallRate, float dt)
{
    float rate = (std::abs(target) > std::abs(cur)) ? riseRate : fallRate;
    return MoveToward(cur, target, rate * dt);
}

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

    {
        auto boundaryWarning = ctx.LoadSoundScoped("Assets/Audio/boundaryWarning.mp3");
        if (boundaryWarning.IsOk())
            m_boundaryWarningSound = boundaryWarning.value;
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

    m_minimapTex = LoadTexture(ctx, "Assets/Texture/minimap.png");
    m_minimap_enemymarkerTex = LoadTexture(ctx, "Assets/Texture/minimap_enemyMarker.png");

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
    {
        auto& s = mat_spacefighter.Primary();
        s.metallic = 0.9f;
        s.roughness = 0.3f;
        s.emissive = 0.0f;
    }
    MaterialComponent mat_starcruiser  = CreateMaterial(ctx, texh_starcruiser);
    {
        auto& s = mat_starcruiser.Primary();
        s.metallic = 0.95f;
        s.roughness = 0.35f;
        s.emissive = 0.0f;
    }

    MaterialComponent mat_map = CreateMaterial(ctx, texh_map);
    {
        auto& s = mat_map.Primary();
        s.metallic = 0.0f;
        s.roughness = 0.85f;
        s.emissive = 0.0f;
    }

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
            col.layer = 1 << 0;
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
            stats->SetHp(5000);
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

        // warning audio entity (player child 역할)
        {
            m_warningAudio = ctx.Instantiate("WarningAudio");
            ctx.world.AddTransform(m_warningAudio);
            ctx.world.AddAudioSource(m_warningAudio, AudioSourceComponent{});

            ctx.world.SetParent(m_warningAudio, m_player);
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

    // minimap
    {
        // background
        {
            m_minimapBg = ctx.Instantiate("MinimapBG");
            UIElementComponent bg;
            bg.texture = m_minimapTex;
            bg.sizePx = { 256,256 };
            bg.anchor = { 1.0f, 1.0f };          // 화면 우하단 기준
            bg.pivot = { 1.0f, 1.0f };          // 자기 우하단을 anchor에 붙임
            bg.anchoredPosPx = { -20, -20 };     // 화면 끝에서 살짝 띄움
            bg.z = 100;                          // HUD 위로
            bg.color = { 1,1,1,1 };
            ctx.world.AddUIElement(m_minimapBg, bg);
        }

        // enemy pool
        {
            const int K = 64;
            m_minimapEnemyMarkers.clear();
            m_minimapEnemyMarkers.reserve(K);

            for (int i = 0; i < K; i++)
            {
                EntityId m = ctx.Instantiate(std::format("MinimapEnemyMarker{}", i).c_str());
                UIElementComponent ui;
                ui.uiParent = m_minimapBg;                 // BG의 자식
                ui.texture = m_minimap_enemymarkerTex;
                ui.sizePx = { 12,12 };                     // 빨간 점 크기
                ui.anchor = { 0.5f, 0.5f };                // 부모(미니맵) 중앙
                ui.pivot = { 0.5f, 0.5f };                // 점의 중앙 정렬
                ui.anchoredPosPx = { 0,0 };
                ui.z = 101;                                // BG 위
                ui.enabled = false;
                ctx.world.AddUIElement(m, ui);

                m_minimapEnemyMarkers.push_back(m);
            }
        }
    }

    // keys
    {
        texh_black_w = LoadTexture(ctx, "Assets/Texture/PlayScene/black_w.png");
        texh_black_s = LoadTexture(ctx, "Assets/Texture/PlayScene/black_s.png");
        texh_black_a = LoadTexture(ctx, "Assets/Texture/PlayScene/black_a.png");
        texh_black_d = LoadTexture(ctx, "Assets/Texture/PlayScene/black_d.png");
        texh_black_8 = LoadTexture(ctx, "Assets/Texture/PlayScene/black_8.png");
        texh_black_2 = LoadTexture(ctx, "Assets/Texture/PlayScene/black_2.png");
        texh_black_4 = LoadTexture(ctx, "Assets/Texture/PlayScene/black_4.png");
        texh_black_6 = LoadTexture(ctx, "Assets/Texture/PlayScene/black_6.png");
        texh_black_up = LoadTexture(ctx, "Assets/Texture/PlayScene/black_up.png");
        texh_black_down = LoadTexture(ctx, "Assets/Texture/PlayScene/black_down.png");
        texh_black_left = LoadTexture(ctx, "Assets/Texture/PlayScene/black_left.png");
        texh_black_right = LoadTexture(ctx, "Assets/Texture/PlayScene/black_right.png");
        texh_black_space = LoadTexture(ctx, "Assets/Texture/PlayScene/black_space.png");
        texh_black_ctrl = LoadTexture(ctx, "Assets/Texture/PlayScene/black_ctrl.png");

        texh_white_w = LoadTexture(ctx, "Assets/Texture/PlayScene/white_w.png");
        texh_white_s = LoadTexture(ctx, "Assets/Texture/PlayScene/white_s.png");
        texh_white_a = LoadTexture(ctx, "Assets/Texture/PlayScene/white_a.png");
        texh_white_d = LoadTexture(ctx, "Assets/Texture/PlayScene/white_d.png");
        texh_white_8 = LoadTexture(ctx, "Assets/Texture/PlayScene/white_8.png");
        texh_white_2 = LoadTexture(ctx, "Assets/Texture/PlayScene/white_2.png");
        texh_white_4 = LoadTexture(ctx, "Assets/Texture/PlayScene/white_4.png");
        texh_white_6 = LoadTexture(ctx, "Assets/Texture/PlayScene/white_6.png");
        texh_white_up = LoadTexture(ctx, "Assets/Texture/PlayScene/white_up.png");
        texh_white_down = LoadTexture(ctx, "Assets/Texture/PlayScene/white_down.png");
        texh_white_left = LoadTexture(ctx, "Assets/Texture/PlayScene/white_left.png");
        texh_white_right = LoadTexture(ctx, "Assets/Texture/PlayScene/white_right.png");
        texh_white_space = LoadTexture(ctx, "Assets/Texture/PlayScene/white_space.png");
        texh_white_ctrl = LoadTexture(ctx, "Assets/Texture/PlayScene/white_ctrl.png");

        {
            key_w = ctx.Instantiate("key_w");
            UIElementComponent image;
            image.texture = texh_white_w;
            image.sizePx = { 24, 24 };
            image.anchoredPosPx = { 10, 504 };
            ctx.world.AddUIElement(key_w, image);
        }

        {
            key_s = ctx.Instantiate("key_s");
            UIElementComponent image;
            image.texture = texh_white_s;
            image.sizePx = { 24, 24 };
            image.anchoredPosPx = { 34, 504 };
            ctx.world.AddUIElement(key_s, image);
        }

        {
            key_a = ctx.Instantiate("key_a");
            UIElementComponent image;
            image.texture = texh_white_a;
            image.sizePx = { 24, 24 };
            image.anchoredPosPx = { 10, 528 };
            ctx.world.AddUIElement(key_a, image);
        }

        {
            key_d = ctx.Instantiate("key_d");
            UIElementComponent image;
            image.texture = texh_white_d;
            image.sizePx = { 24, 24 };
            image.anchoredPosPx = { 34, 528 };
            ctx.world.AddUIElement(key_d, image);
        }

        {
            key_8 = ctx.Instantiate("key_8");
            UIElementComponent image;
            image.texture = texh_white_8;
            image.sizePx = { 24, 24 };
            image.anchoredPosPx = { 10, 552 };
            ctx.world.AddUIElement(key_8, image);
        }

        {
            key_2 = ctx.Instantiate("key_2");
            UIElementComponent image;
            image.texture = texh_white_2;
            image.sizePx = { 24, 24 };
            image.anchoredPosPx = { 34, 552 };
            ctx.world.AddUIElement(key_2, image);
        }

        {
            key_4 = ctx.Instantiate("key_4");
            UIElementComponent image;
            image.texture = texh_white_4;
            image.sizePx = { 24, 24 };
            image.anchoredPosPx = { 10, 576 };
            ctx.world.AddUIElement(key_4, image);
        }

        {
            key_6 = ctx.Instantiate("key_6");
            UIElementComponent image;
            image.texture = texh_white_6;
            image.sizePx = { 24, 24 };
            image.anchoredPosPx = { 34, 576 };
            ctx.world.AddUIElement(key_6, image);
        }

        {
            key_up = ctx.Instantiate("key_up");
            UIElementComponent image;
            image.texture = texh_white_up;
            image.sizePx = { 24, 24 };
            image.anchoredPosPx = { 140, 528 };
            ctx.world.AddUIElement(key_up, image);
        }

        {
            key_down = ctx.Instantiate("key_down");
            UIElementComponent image;
            image.texture = texh_white_down;
            image.sizePx = { 24, 24 };
            image.anchoredPosPx = { 164, 528 };
            ctx.world.AddUIElement(key_down, image);
        }

        {
            key_left = ctx.Instantiate("key_left");
            UIElementComponent image;
            image.texture = texh_white_left;
            image.sizePx = { 24, 24 };
            image.anchoredPosPx = { 188, 528 };
            ctx.world.AddUIElement(key_left, image);
        }

        {
            key_right = ctx.Instantiate("key_right");
            UIElementComponent image;
            image.texture = texh_white_right;
            image.sizePx = { 24, 24 };
            image.anchoredPosPx = { 212, 528 };
            ctx.world.AddUIElement(key_right, image);
        }

        {
            key_space = ctx.Instantiate("key_space");
            UIElementComponent image;
            image.texture = texh_white_space;
            image.sizePx = { 96, 24 };
            image.anchoredPosPx = { 140, 552 };
            ctx.world.AddUIElement(key_space, image);
        }

        {
            key_ctrl = ctx.Instantiate("key_ctrl");
            UIElementComponent image;
            image.texture = texh_white_ctrl;
            image.sizePx = { 48, 24 };
            image.anchoredPosPx = { 140, 576 };
            ctx.world.AddUIElement(key_ctrl, image);
        }
    }
}

void PlayScene::OnUnload(SceneContext& ctx)
{
}

void PlayScene::OnUpdate(SceneContext& ctx)
{
    // 플레이어가 이미 죽었거나 삭제 예정이면 바로 Lose 처리
    if (!m_player.IsValid() || !ctx.world.IsAlive(m_player) || !ctx.world.HasTransform(m_player))
    {
        GameManager::GetInstance().SetState(GameManager::State::Lose);
    }

    m_enemyHud.SetEnabled(GameManager::GetInstance().CurrentState() == GameManager::State::Playing);
    if (GameManager::GetInstance().CurrentState() == GameManager::State::Win || GameManager::GetInstance().CurrentState() == GameManager::State::Lose)
    {
        ctx.audio.StopEntity(m_warningAudio);
        ctx.RequestLoadScene(SceneId::Result);
        return;
    }

    ctx.DrawWText(58, 504, L"속도변경", 18.0f, { 1, 1, 1, 1 });
    ctx.DrawWText(58, 528, L"좌우회전", 18.0f, { 1, 1, 1, 1 });
    ctx.DrawWText(58, 552, L"상하회전", 18.0f, { 1, 1, 1, 1 });
    ctx.DrawWText(58, 576, L"뱅크회전", 18.0f, { 1, 1, 1, 1 });
    ctx.DrawWText(240, 528, L"카메라조작", 18.0f, { 1, 1, 1, 1 });
    ctx.DrawWText(240, 552, L"기총발사", 18.0f, { 1, 1, 1, 1 });
    ctx.DrawWText(190, 576, L"미사일발사", 18.0f, { 1, 1, 1, 1 });

    DisplayDefaultHUD(ctx);

    DetectBoundary(ctx);

    // 전투 지역 이탈 경고음
    UpdateBoundaryWarningAudio(ctx);

    // 경계를 벗어난 경우
    if (m_outOfPlay)
    {
        ctx.DrawWText(300, 350, L"전투 지역을 이탈하고 있습니다!", 18, { 1, 0, 0, 1 });
    }

    // 땅이 가까운 경우
    if (m_groundWarning)
    {
        ctx.DrawWText(300, 380, L"지면이 너무 가깝습니다!", 18, { 1, 0, 0 ,1 });
    }

    m_enemyHud.Update(ctx, m_cam, m_player, m_waveManager.GetCurrentEnemies());

    PlayerWindUpdate(ctx, m_player, m_windSounds);

    m_waveManager.Update(ctx, ctx.dt);
    UpdateMinimap(ctx);

    BuildShooterCommands(ctx.input, m_cmds);

    ExecuteCommand(ctx);

    // keys
    {
        // w
        {
            if (ctx.input.IsKeyPressed(Key::W))
            {
                auto& image = ctx.world.GetUIElement(key_w);
                image.texture = texh_black_w;
            }
            else if (ctx.input.IsKeyReleased(Key::W))
            {
                auto& image = ctx.world.GetUIElement(key_w);
                image.texture = texh_white_w;
            }
        }

        // s
        {
            if (ctx.input.IsKeyPressed(Key::S))
            {
                auto& image = ctx.world.GetUIElement(key_s);
                image.texture = texh_black_s;
            }
            else if (ctx.input.IsKeyReleased(Key::S))
            {
                auto& image = ctx.world.GetUIElement(key_s);
                image.texture = texh_white_s;
            }
        }

        // a
        {
            if (ctx.input.IsKeyPressed(Key::A))
            {
                auto& image = ctx.world.GetUIElement(key_a);
                image.texture = texh_black_a;
            }
            else if (ctx.input.IsKeyReleased(Key::A))
            {
                auto& image = ctx.world.GetUIElement(key_a);
                image.texture = texh_white_a;
            }
        }

        // d
        {
            if (ctx.input.IsKeyPressed(Key::D))
            {
                auto& image = ctx.world.GetUIElement(key_d);
                image.texture = texh_black_d;
            }
            else if (ctx.input.IsKeyReleased(Key::D))
            {
                auto& image = ctx.world.GetUIElement(key_d);
                image.texture = texh_white_d;
            }
        }

        // 8
        {
            if (ctx.input.IsKeyPressed(Key::NumPad8))
            {
                auto& image = ctx.world.GetUIElement(key_8);
                image.texture = texh_black_8;
            }
            else if (ctx.input.IsKeyReleased(Key::NumPad8))
            {
                auto& image = ctx.world.GetUIElement(key_8);
                image.texture = texh_white_8;
            }
        }

        // 2
        {
            if (ctx.input.IsKeyPressed(Key::NumPad2))
            {
                auto& image = ctx.world.GetUIElement(key_2);
                image.texture = texh_black_2;
            }
            else if (ctx.input.IsKeyReleased(Key::NumPad2))
            {
                auto& image = ctx.world.GetUIElement(key_2);
                image.texture = texh_white_2;
            }
        }

        // 4
        {
            if (ctx.input.IsKeyPressed(Key::NumPad4))
            {
                auto& image = ctx.world.GetUIElement(key_4);
                image.texture = texh_black_4;
            }
            else if (ctx.input.IsKeyReleased(Key::NumPad4))
            {
                auto& image = ctx.world.GetUIElement(key_4);
                image.texture = texh_white_4;
            }
        }

        // 6
        {
            if (ctx.input.IsKeyPressed(Key::NumPad6))
            {
                auto& image = ctx.world.GetUIElement(key_6);
                image.texture = texh_black_6;
            }
            else if (ctx.input.IsKeyReleased(Key::NumPad6))
            {
                auto& image = ctx.world.GetUIElement(key_6);
                image.texture = texh_white_6;
            }
        }

        // up
        {
            if (ctx.input.IsKeyPressed(Key::Up))
            {
                auto& image = ctx.world.GetUIElement(key_up);
                image.texture = texh_black_up;
            }
            else if (ctx.input.IsKeyReleased(Key::Up))
            {
                auto& image = ctx.world.GetUIElement(key_up);
                image.texture = texh_white_up;
            }
        }

        // down
        {
            if (ctx.input.IsKeyPressed(Key::Down))
            {
                auto& image = ctx.world.GetUIElement(key_down);
                image.texture = texh_black_down;
            }
            else if (ctx.input.IsKeyReleased(Key::Down))
            {
                auto& image = ctx.world.GetUIElement(key_down);
                image.texture = texh_white_down;
            }
        }

        // left
        {
            if (ctx.input.IsKeyPressed(Key::Left))
            {
                auto& image = ctx.world.GetUIElement(key_left);
                image.texture = texh_black_left;
            }
            else if (ctx.input.IsKeyReleased(Key::Left))
            {
                auto& image = ctx.world.GetUIElement(key_left);
                image.texture = texh_white_left;
            }
        }

        // right
        {
            if (ctx.input.IsKeyPressed(Key::Right))
            {
                auto& image = ctx.world.GetUIElement(key_right);
                image.texture = texh_black_right;
            }
            else if (ctx.input.IsKeyReleased(Key::Right))
            {
                auto& image = ctx.world.GetUIElement(key_right);
                image.texture = texh_white_right;
            }
        }

        // space
        {
            if (ctx.input.IsKeyPressed(Key::Space))
            {
                auto& image = ctx.world.GetUIElement(key_space);
                image.texture = texh_black_space;
            }
            else if (ctx.input.IsKeyReleased(Key::Space))
            {
                auto& image = ctx.world.GetUIElement(key_space);
                image.texture = texh_white_space;
            }
        }

        // ctrl
        {
            if (ctx.input.IsKeyPressed(Key::Ctrl))
            {
                auto& image = ctx.world.GetUIElement(key_ctrl);
                image.texture = texh_black_ctrl;
            }
            else if (ctx.input.IsKeyReleased(Key::Ctrl))
            {
                auto& image = ctx.world.GetUIElement(key_ctrl);
                image.texture = texh_white_ctrl;
            }
        }
    }
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

    if (!rig) return;

    // (A) 이번 프레임 목표값(디지털)
    FlightInput target{}; // 기본 0 (Move 없으면 자연스럽게 중립으로 복귀)

    for (auto& c : m_cmds)
    {
        if (c.action == ShooterAction::Move)
        {
            target.pitch = c.pitch;
            target.roll = c.roll;
            target.yaw = c.yaw;
            target.throttleDelta = c.throttle; // W/S: 레버 업/다운(Delta)
            target.airbrake = c.airbrake;
            break;
        }
    }

    // (B) 스무딩 (레이트 제한)
    m_playerSmoothed.pitch = ApplyRate(m_playerSmoothed.pitch, target.pitch, m_inputRiseRate, m_inputFallRate, ctx.dt);
    m_playerSmoothed.roll = ApplyRate(m_playerSmoothed.roll, target.roll, m_inputRiseRate, m_inputFallRate, ctx.dt);
    m_playerSmoothed.yaw = ApplyRate(m_playerSmoothed.yaw, target.yaw, m_inputRiseRate, m_inputFallRate, ctx.dt);

    m_playerSmoothed.throttleDelta =
        ApplyRate(m_playerSmoothed.throttleDelta, target.throttleDelta, m_throttleRiseRate, m_throttleFallRate, ctx.dt);

    m_playerSmoothed.airbrake = target.airbrake;

    // (C) rig에 공급
    rig->SetInput(m_playerSmoothed);

    // (D) 나머지 명령 실행
    for (const auto& cmd : m_cmds)
    {
        switch (cmd.action)
        {
        case ShooterAction::Move:
            break;

        case ShooterAction::FireGun:
            if (gun) gun->Fire(ctx);
            break;

        case ShooterAction::FireMissile:
            if (missile) missile->Fire(ctx);
            break;

        case ShooterAction::CameraLook:
            if (camRig) camRig->OnLook(cmd.camX, cmd.camY);
            break;
        }
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
    s.color = { 1, 0.95f, 0.95f };
    s.intensity = 8.0f;
    ctx.world.AddLight(light, s);

    ctx.world.SetLocalRotationEuler(light, {
        DirectX::XMConvertToRadians(3.0f),
        DirectX::XMConvertToRadians(-90.0f),
        0.0f
    });

    // Fill light
    //EntityId light2 = ctx.Instantiate("SubDirectionalLight");
    //ctx.world.AddTransform(light2);
    //
    //LightComponent s2{};
    //s2.type = LightType::Directional;
    //s2.color = { 0.8f, 0.5f, 0.6f };
    //s2.intensity = 1.5f;
    //ctx.world.AddLight(light2, s2);
    //
    //ctx.world.SetLocalRotationEuler(light2, {
    //    DirectX::XMConvertToRadians(0.0f),
    //    DirectX::XMConvertToRadians(90.0f),
    //    0.0f
    //});
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

    m_outOfPlay = (d2 > playR2);
    m_groundWarning = p.y < m_groundBoundary + 30;
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

void PlayScene::DisplayDefaultHUD(SceneContext& ctx)
{
    if (!m_player.IsValid()) return;
    if (!ctx.world.IsAlive(m_player)) return;

    auto stats = ctx.world.GetScriptAs<StatsComponent>(m_player);
    if (!stats) return;

    auto buf = std::format(L"HP : {}", stats->CurrentHp());
    ctx.DrawWText(40, 430, buf, 24, {0.2f, 1, 0.35f, 1});

    auto missileLauncher = ctx.world.GetScriptAs<MissileLauncherComponent>(m_player);
    buf = std::format(L"missile : {}", missileLauncher->m_missileCount);
    ctx.DrawWText(40, 460, buf, 24, {0.2f, 1, 0.35f, 1});

    buf = std::format(L"웨이브 {}", m_waveManager.GetCurrentWaveIndex() + 1);
    ctx.DrawWText(40, 40, buf, 24, { 1, 1, 1, 1 });

    buf = std::format(L"남은 적 수 : {} / {}", m_waveManager.GetCurrentEnemiesCount(), m_waveManager.GetCurrentWaveEnemiesCound());
    ctx.DrawWText(40, 80, buf, 24, { 1, 1, 1, 1 });
}

void PlayScene::UpdateBoundaryWarningAudio(SceneContext& ctx)
{
    if (!ctx.world.IsAlive(m_warningAudio) || !ctx.world.HasAudioSource(m_warningAudio))
        return;

    const bool warningNow = (m_outOfPlay || m_groundWarning);

    auto& src = ctx.world.GetAudioSource(m_warningAudio);
    src.bus = AudioBus::SFX;
    src.loop = false;

    if (!warningNow)
    {
        if (m_prevWarning)
        {
            ctx.audio.StopEntity(m_warningAudio);
        }
        m_prevWarning = false;
        return;
    }

    // ON 상태
    src.clip = m_boundaryWarningSound;

    // 아직 재생 중이면 겹치지 않게 리턴
    if (ctx.audio.IsEntityPlaying(m_warningAudio))
    {
        m_prevWarning = true;
        return;
    }

    // 재생 중이 아니면 1회 재생 요청
    ctx.audio.PlayFromEntity(m_warningAudio);
    m_prevWarning = true;
}

void PlayScene::UpdateMinimap(SceneContext& ctx)
{
    if (!ctx.world.IsAlive(m_player) || !ctx.world.HasTransform(m_player))
        return;

    // 미니맵 크기/반지름(픽셀)
    const float mmW = ctx.world.GetUIElement(m_minimapBg).sizePx.x;
    const float mmH = ctx.world.GetUIElement(m_minimapBg).sizePx.y;
    const float R = 0.5f * std::min(mmW, mmH);

    const float marginPx = 10.0f;
    const float clampR = R - marginPx;

    // 미니맵이 커버하는 월드 반경
    const float rangeWorld = m_playBoundary;

    XMFLOAT3 ppos = ctx.world.GetLocalPosition(m_player);

    float playerYaw = 0.0f;
    if (ctx.world.HasTransform(m_player))
    {
        playerYaw = ctx.world.GetLocalRotationEuler(m_player).y;
    }

    const float c = cosf(playerYaw);
    const float s = sinf(playerYaw);

    // 1) 살아있는 적 목록 수집
    std::vector<EntityId> alive;
    alive.reserve(128);

    for (EntityId e : m_waveManager.GetCurrentEnemies())
    {
        if (!ctx.world.IsAlive(e) || !ctx.world.HasTransform(e))
            continue;

        if (auto* st = ctx.world.GetScriptAs<StatsComponent>(e))
        {
            if (st->CurrentHp() <= 0) continue;
        }
        alive.push_back(e);
    }

    // 가까운 순 정렬
    std::sort(alive.begin(), alive.end(),
        [&](EntityId a, EntityId b)
        {
            XMFLOAT3 A = ctx.world.GetLocalPosition(a);
            XMFLOAT3 B = ctx.world.GetLocalPosition(b);
            float dax = A.x - ppos.x, daz = A.z - ppos.z;
            float dbx = B.x - ppos.x, dbz = B.z - ppos.z;
            return (dax * dax + daz * daz) < (dbx * dbx + dbz * dbz);
        });

    const int K = (int)m_minimapEnemyMarkers.size();
    const int count = std::min((int)alive.size(), K);

    // 2) 보이는 슬롯 갱신
    for (int i = 0; i < count; i++)
    {
        EntityId marker = m_minimapEnemyMarkers[i];
        auto& ui = ctx.world.GetUIElement(marker);

        XMFLOAT3 epos = ctx.world.GetLocalPosition(alive[i]);

        float dx = epos.x - ppos.x;
        float dz = epos.z - ppos.z;

        // 로컬(플레이어 기준 회전)
        float lx = dx * c - dz * s;
        float lz = dx * s + dz * c;

        // 월드->픽셀
        float px = (lx / rangeWorld) * clampR;
        float py = (lz / rangeWorld) * clampR;

        // 원형 클램프
        float len = std::sqrt(px * px + py * py);
        if (len > clampR && len > 1e-5f)
        {
            px = px / len * clampR;
            py = py / len * clampR;
        }

        ui.anchoredPosPx = { px, -py };

        ui.enabled = true;
    }

    // 3) 남은 슬롯 숨김
    for (int i = count; i < K; i++)
    {
        auto& ui = ctx.world.GetUIElement(m_minimapEnemyMarkers[i]);
        ui.enabled = false;
    }
}
