#include "PlayScene.h"

void PlayScene::OnLoad(SceneContext& ctx)
{
	// Skybox 임포트
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


	// 기본 모델 임포트
	ImportOptions importOpt{};
	importOpt.triangulate = true;
	importOpt.generateNormalsIfMissing = true;
	importOpt.flipV = true;
	importOpt.uniformScale = 1.0f;

	auto imported_spacefighter = ctx.ImportModel("Assets/Model/space_fighter.obj", importOpt);
	if (!imported_spacefighter.IsOk())
	{
		LOG_ERROR("Failed to import space_fighter.obj: %s", imported_spacefighter.error->message.c_str());
		return;
	}

	auto imported_starcruiser = ctx.ImportModel("Assets/Model/star_cruiser.obj", importOpt);
	if (!imported_starcruiser.IsOk())
	{
		LOG_ERROR("Failed to import star_cruiser.obj: %s", imported_starcruiser.error->message.c_str());
		return;
	}

	// 기본 텍스쳐 로드
	auto tex_spacefighter_res = LoadTextureRGBA8_WIC("Assets/Texture/space_fighter_diffuse.png",
		ImageColorSpace::SRGB, /*flipY=*/false);
	TextureHandle tex_spacefighter{};
	if (tex_spacefighter_res.IsOk())
	{
		tex_spacefighter = ctx.textures.Create(std::move(tex_spacefighter_res.value));
	}
	else
	{
		LOG_ERROR("Failed to load texture: %s", tex_spacefighter_res.error->message.c_str());
	}

	auto tex_starcruiser_res = LoadTextureRGBA8_WIC("Assets/Texture/star_cruiser_diffuse.png",
		ImageColorSpace::SRGB, /*flipY=*/false);
	TextureHandle tex_starcruiser{};
	if (tex_starcruiser_res.IsOk())
	{
		tex_starcruiser = ctx.textures.Create(std::move(tex_starcruiser_res.value));
	}
	else
	{
		LOG_ERROR("Failed to load texture: %s", tex_starcruiser_res.error->message.c_str());
	}

	// 머티리얼 설정
	MaterialComponent mat_spacefighter{};
	mat_spacefighter.slots.resize(256);
	for (auto& s : mat_spacefighter.slots)
	{
		s.color = { 1,1,1,1 };
		s.albedo = tex_spacefighter.IsValid() ? tex_spacefighter : TextureHandle{ 0 };
	}
	MaterialComponent mat_starcruiser{};
	mat_starcruiser.slots.resize(256);
	for (auto& s : mat_starcruiser.slots)
	{
		s.color = { 1,1,1,1 };
		s.albedo = tex_starcruiser.IsValid() ? tex_starcruiser : TextureHandle{ 0 };
	}

	// 테스트 인스턴스 생성
	SpawnModelOptions spawnOpt{};
	spawnOpt.name = "SpaceFighter";
	auto spawned_spacefighter = ctx.SpawnModel(imported_spacefighter.value, spawnOpt);
	if (!spawned_spacefighter.IsOk())
	{
		LOG_ERROR("Failed to Spawn space_fighter: %s", spawned_spacefighter.error->message.c_str());
		return;
	}
	EntityId fighter = spawned_spacefighter.value;
	ctx.world.AddMaterial(fighter, mat_spacefighter);
	ctx.world.SetLocalPosition(fighter, { 0.0f, 0.0f, 0.0f });
	ctx.world.SetLocalRotationEuler(fighter, { 0.0f, 45.0f, 0.0f });

	spawnOpt.name = "StarCruiser";
	auto spawned_starcruiser = ctx.SpawnModel(imported_starcruiser.value, spawnOpt);
	if (!spawned_starcruiser.IsOk())
	{
		LOG_ERROR("Failed to Spawn star_cruiser: %s", spawned_starcruiser.error->message.c_str());
		return;
	}
	EntityId cruiser = spawned_starcruiser.value;
	ctx.world.AddMaterial(cruiser, mat_starcruiser);
	ctx.world.SetLocalPosition(cruiser, { 0.0f, 0.0f, 3.0f });

	// Camera
	{
		EntityId cam = ctx.Instantiate("MainCamera");

		ctx.world.AddTransform(cam);
		ctx.world.AddCamera(cam);

		ctx.world.SetLocalPosition(cam, { 0.f, 0.f, -6.f });
		ctx.world.GetCamera(cam).active = true;
	}
}

void PlayScene::OnUnload(SceneContext& ctx)
{
}

void PlayScene::OnUpdate(SceneContext& ctx)
{
	// test camera control
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
}
