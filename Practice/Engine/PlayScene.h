#pragma once

#include "Scene.h"

class PlayScene : public Scene
{
public:

private:


	// Scene을(를) 통해 상속됨
	void OnLoad(SceneContext& ctx) override;

	void OnUnload(SceneContext& ctx) override;

	void OnUpdate(SceneContext& ctx) override;

};