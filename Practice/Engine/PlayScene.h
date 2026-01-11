#pragma once
#include "Scene.h"
#include <vector>
#include <string>
#include "MaterialComponent.h"
#include "TextureHandle.h"
#include "ModelAsset.h"
#include "CameraRig.h"

enum class ShooterAction
{
    Move,        // 이동
    FireGun,     // 발사
	FireMissile, // 미사일 발사
	CameraLook,  // 카메라 시점 이동
};

struct ShooterCommand
{
    ShooterAction action;
	float throttle = 0.f; // 이동 Z축
    float yaw      = 0.f; // 이동 X축(선회 Y축)
	float roll     = 0.f; // 선회 Z축
	float pitch    = 0.f; // 선회 X축
	float camX	   = 0.f; // 카메라 좌우
	float camY     = 0.f; // 카메라 상하
};

static void BuildShooterCommands(const Input& input, std::vector<ShooterCommand>& out)
{
    out.clear();

    // 1) 이동 축 (-1/0/1)
    float throttle = 0;
    if (input.IsKeyDown(Key::W))	throttle += 1.f;
    if (input.IsKeyDown(Key::S))	throttle -= 1.f;

    // 2) 선회
	float pitch = 0.f, yaw = 0.f, roll = 0.f;
    if (input.IsKeyDown(Key::A))	yaw -= 1.f;
    if (input.IsKeyDown(Key::D))	yaw += 1.f;
	if (input.IsKeyDown(Key::Num4)) roll -= 1.f;
	if (input.IsKeyDown(Key::Num6)) roll += 1.f;
	if (input.IsKeyDown(Key::Num2)) pitch -= 1.f;
	if (input.IsKeyDown(Key::Num8)) pitch += 1.f;

    if (throttle != 0 || pitch != 0 || yaw != 0 || roll != 0)
        out.push_back({ ShooterAction::Move, throttle, yaw, roll, pitch, 0, 0 });

    // 2) 발사
    if (input.IsKeyDown(Key::Space))
        out.push_back({ ShooterAction::FireGun });

	if (input.IsKeyPressed(Key::Ctrl))
		out.push_back({ ShooterAction::FireMissile });

	// 3) 카메라 조작
	float camX = 0.f, camZ = 0.f;
	if (input.IsKeyDown(Key::Left))  camX -= 1.f;
	if (input.IsKeyDown(Key::Right)) camX += 1.f;
	if (input.IsKeyDown(Key::Down))  camZ -= 1.f;
	if (input.IsKeyDown(Key::Up))	 camZ += 1.f;

	if (camX != 0 || camZ != 0)
		out.push_back({ ShooterAction::CameraLook, 0, 0, 0, 0, camX, camZ });
}

class PlayScene : public Scene
{
public:
	// Scene을(를) 통해 상속됨
	void OnLoad(SceneContext& ctx) override;

	void OnUnload(SceneContext& ctx) override;

	void OnUpdate(SceneContext& ctx) override;

private:
	std::vector<ShooterCommand> m_cmds;

	EntityId m_cam;
	EntityId m_player;

	CameraRig m_cameraRig;
	float camX = 0.f, camY = 0.f;

private:
	void SetSkybox(SceneContext& ctx);
	void SetDirectionalLight(SceneContext& ctx);
	Result<ModelAsset> ImportModel(SceneContext& ctx, const std::string& path);
	TextureHandle LoadTexture(SceneContext& ctx, const std::string& path);
	MaterialComponent CreateMaterial(SceneContext& ctx, const TextureHandle& tex);
	void ExecuteCommand(SceneContext& ctx);
	
};