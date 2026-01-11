#include "PlayScene.h"

void PlayScene::OnLoad(SceneContext& ctx)
{
	SetSkybox(ctx);
	SetDirectionalLight(ctx);

	// 기본 모델 임포트
	Result<ModelAsset> res_model_spacefighter = ImportModel(ctx, "Assets/Model/space_fighter.obj");
	if (!res_model_spacefighter.IsOk()) return;

	Result<ModelAsset> res_model_starcruiser = ImportModel(ctx, "Assets/Model/star_cruiser.obj");
	if (!res_model_starcruiser.IsOk()) return;

	// 기본 텍스쳐 로드
	TextureHandle texh_spacefighter = LoadTexture(ctx, "Assets/Texture/space_fighter_diffuse.png");
	if (!texh_spacefighter.IsValid()) return;

	TextureHandle texh_starcruiser = LoadTexture(ctx, "Assets/Texture/star_cruiser_diffuse.png");
	if (!texh_starcruiser.IsValid()) return;

	// 머티리얼 생성
	MaterialComponent mat_spacefighter = CreateMaterial(ctx, texh_spacefighter);

	MaterialComponent mat_starcruiser = CreateMaterial(ctx, texh_starcruiser);

	// 테스트 인스턴스 생성
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

	// Camera
	{
		EntityId cam = ctx.Instantiate("MainCamera");

		ctx.world.AddTransform(cam);
		ctx.world.AddCamera(cam);

		ctx.world.SetLocalPosition(cam, { 0.f, 0.f, -6.f });
		ctx.world.GetCamera(cam).active = true;
		m_cam = cam;

		m_cameraRig.SetCamera(m_cam);
		m_cameraRig.SetTarget(fighter);
		m_cameraRig.SetFollowEnabled(true);
	}
}

void PlayScene::OnUnload(SceneContext& ctx)
{
}

void PlayScene::OnUpdate(SceneContext& ctx)
{
	BuildShooterCommands(ctx.input, m_cmds);

	ExecuteCommand(ctx);
	m_cameraRig.Update(ctx.world, ctx.dt);
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
	{
		ctx.SetSkybox(skyRes.value);
	}
	else
	{
		ctx.ClearSkybox();
	}
}

void PlayScene::SetDirectionalLight(SceneContext& ctx)
{
	EntityId light = ctx.Instantiate("MainDirectionalLight");
	ctx.world.AddTransform(light);
	
	LightComponent s{};
	s.type = LightType::Directional;
	s.color = { 1,0.87f,0.87f };
	s.intensity = 8.0f;
	ctx.world.AddLight(light, s);
	
	ctx.world.SetLocalRotationEuler(light, {
		DirectX::XMConvertToRadians(10.f),  // pitch down
		DirectX::XMConvertToRadians(-90.f),   // yaw
		0.f
		});

	EntityId light2 = ctx.Instantiate("SubDirectionalLight");
	ctx.world.AddTransform(light2);

	LightComponent s2{};
	s2.type = LightType::Directional;
	s2.color = { 0.8f,0.5f,0.6f };
	s2.intensity = 3.5f;
	ctx.world.AddLight(light2, s2);

	ctx.world.SetLocalRotationEuler(light2, {
		DirectX::XMConvertToRadians(0.f),  // pitch down
		DirectX::XMConvertToRadians(90.f),   // yaw
		0.f
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
	{
		LOG_ERROR("Failed to import %s: %s", path, imported.error->message.c_str());
	}
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
	}
#endif
	return tex;
}

MaterialComponent PlayScene::CreateMaterial(SceneContext& ctx, const TextureHandle& tex)
{
	MaterialComponent mat{};
	mat.slots.resize(256);
	for (auto& s : mat.slots)
	{
		s.color = { 1,1,1,1 };
		s.albedo = tex.IsValid() ? tex : TextureHandle{ 0 };
	}

	return mat;
}

void PlayScene::ExecuteCommand(SceneContext& ctx)
{
	for (const auto& cmd : m_cmds)
	{
		switch (cmd.action)
		{
		case ShooterAction::Move:
			break;
		case ShooterAction::FireGun:
			break;
		case ShooterAction::FireMissile:
			break;
		case ShooterAction::CameraLook:
			m_cameraRig.OnLook(cmd.camX, cmd.camY);
			break;
		}
	}
}
