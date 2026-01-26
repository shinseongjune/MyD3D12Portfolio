#pragma once
#include "Scene.h"
#include <vector>
#include <string>
#include "MaterialComponent.h"
#include "TextureHandle.h"
#include "ModelAsset.h"
#include "WaveManager.h"
#include "EnemyIndicatorHUD.h"

class FlightRigComponent;
class CameraRigComponent;
class GunComponent;
class MissileLauncherComponent;

enum class ShooterAction
{
    Move,
    FireGun,
    FireMissile,
    CameraLook,
};

struct ShooterCommand
{
    ShooterAction action;

    // Flight controls
    float throttle = 0.0f; // forward/back input (-1..+1)
    float yaw      = 0.0f; // -1..+1
    float roll     = 0.0f; // -1..+1
    float pitch    = 0.0f; // -1..+1
	bool airbrake  = false;

    // Camera look
    float camX = 0.0f;
    float camY = 0.0f;
};

static void BuildShooterCommands(const Input& input, std::vector<ShooterCommand>& out)
{
    out.clear();

    // 1) throttle (-1/0/1)
    float throttle = 0.0f;
    if (input.IsKeyDown(Key::W)) throttle += 1.0f;
    if (input.IsKeyDown(Key::S)) throttle -= 1.0f;

    // 2) attitude
    float pitch = 0.0f, yaw = 0.0f, roll = 0.0f;
    // Left/right steering (flip per latest request)
    if (input.IsKeyDown(Key::A))    yaw -= 1.0f;
    if (input.IsKeyDown(Key::D))    yaw += 1.0f;
    // Aircraft attitude: prefer numpad 8/4/6/2, with top-row digit fallback.
    // Bank left/right (flip per latest request)
    if (input.IsKeyDown(Key::NumPad4) || input.IsKeyDown(Key::Num4)) roll += 1.0f;
    if (input.IsKeyDown(Key::NumPad6) || input.IsKeyDown(Key::Num6)) roll -= 1.0f;
    // Pitch: 8 raises nose, 2 lowers nose (flip per latest request)
    if (input.IsKeyDown(Key::NumPad8) || input.IsKeyDown(Key::Num8)) pitch -= 1.0f;
    if (input.IsKeyDown(Key::NumPad2) || input.IsKeyDown(Key::Num2)) pitch += 1.0f;
	bool airbrake = input.IsKeyDown(Key::Shift);

    if (throttle != 0.0f || pitch != 0.0f || yaw != 0.0f || roll != 0.0f || airbrake)
        out.push_back({ ShooterAction::Move, throttle, yaw, roll, pitch, airbrake, 0.0f, 0.0f });

    // 3) fire
    if (input.IsKeyDown(Key::Space))
        out.push_back({ ShooterAction::FireGun });

    if (input.IsKeyPressed(Key::Ctrl))
        out.push_back({ ShooterAction::FireMissile });

    // 4) camera
    float camX = 0.0f, camY = 0.0f;
    if (input.IsKeyDown(Key::Left))  camX -= 1.0f;
    if (input.IsKeyDown(Key::Right)) camX += 1.0f;
    if (input.IsKeyDown(Key::Down))  camY -= 1.0f;
    if (input.IsKeyDown(Key::Up))    camY += 1.0f;

    if (camX != 0.0f || camY != 0.0f)
        out.push_back({ ShooterAction::CameraLook, 0, 0, 0, 0, false, camX, camY });
}

class PlayScene : public Scene
{
public:
    void OnLoad(SceneContext& ctx) override;
    void OnUnload(SceneContext& ctx) override;
    void OnUpdate(SceneContext& ctx) override;

private:
    std::vector<ShooterCommand> m_cmds;

    // Camera follow lives as a Behaviour on the camera entity (CameraRigComponent).
    EntityId m_cam;
    EntityId m_player;

    MeshHandle m_quad;
    TextureHandle m_bulletTex;

    std::vector<SoundHandle> m_gunFireSounds;
    std::vector<SoundHandle> m_bulletHitSounds;

    ModelAsset m_missile;
    TextureHandle m_missileTex;

    SoundHandle m_missileLaunchSound;
    SoundHandle m_missileBoomSound;

    std::vector<TextureHandle> m_boosterAnims;
    std::vector<std::vector<TextureHandle>> m_deathAnimsContainer;
    std::vector<TextureHandle> m_bulletHitAnims;
    std::vector<TextureHandle> m_missleHitAnims;
    std::vector<TextureHandle> m_missleBoosterAnims;

    std::vector<SoundHandle> m_deathSounds;

    std::vector<SoundHandle> m_windSounds;

    WaveManager m_waveManager;
    EnemyIndicatorHUD m_enemyHud;

private:
    void SetSkybox(SceneContext& ctx);
    void SetDirectionalLight(SceneContext& ctx);
    Result<ModelAsset> ImportModel(SceneContext& ctx, const std::string& path);
    TextureHandle LoadTexture(SceneContext& ctx, const std::string& path);
    MaterialComponent CreateMaterial(SceneContext& ctx, const TextureHandle& tex);
	MeshHandle CreateQuadMesh(SceneContext& ctx);
	TextureHandle CreateBulletTexture(SceneContext& ctx);

    void ExecuteCommand(SceneContext& ctx);

    // Finds the attached FlightRigComponent (if any).
    FlightRigComponent* GetFlightRig(SceneContext& ctx);

    // Finds the attached CameraRigComponent (if any).
    CameraRigComponent* GetCameraRig(SceneContext& ctx);

    // Finds the attached GunComponent
    GunComponent* GetGun(SceneContext& ctx);

    MissileLauncherComponent* GetMissileLauncher(SceneContext& ctx);

    float m_playBoundary = 0;
    float m_groundBoundary = -150.0f;

    int spawnPosNumber = 0;
    std::vector<EntityId> m_spawnPositions;
    void SetBoundaryRadius(SceneContext& ctx, float radius);
    void BuildBoundariesAndSpawnPoints(SceneContext& ctx, float playRadius, int latDiv, int lonDiv, float spawnRadiusRatio, float groundY);
    
    bool m_outOfPlay = false;
    void DetectBoundary(SceneContext& ctx);

    void PlayerWindUpdate(SceneContext& ctx, EntityId player, const std::vector<SoundHandle>& windClips);

};
