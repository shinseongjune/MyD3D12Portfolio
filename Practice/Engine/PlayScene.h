#pragma once

#include "Scene.h"

#include <string>
#include "MaterialComponent.h"
#include "TextureHandle.h"
#include "ModelAsset.h"

class PlayScene : public Scene
{
public:
	// Scene을(를) 통해 상속됨
	void OnLoad(SceneContext& ctx) override;

	void OnUnload(SceneContext& ctx) override;

	void OnUpdate(SceneContext& ctx) override;

private:
	void SetSkybox(SceneContext& ctx);
	void SetDirectionalLight(SceneContext& ctx);
	Result<ModelAsset> ImportModel(SceneContext& ctx, const std::string& path);
	TextureHandle LoadTexture(SceneContext& ctx, const std::string& path);
	MaterialComponent CreateMaterial(SceneContext& ctx, const TextureHandle& tex);
};