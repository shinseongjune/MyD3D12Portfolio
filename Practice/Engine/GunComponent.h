#pragma once
#include <DirectXMath.h>
#include "Behaviour.h"
#include "MeshHandle.h"
#include "TextureHandle.h"

class GunComponent : public Behaviour
{
public:
	DirectX::XMFLOAT3 muzzleOffset{ 0.0f, 0.0f, 1.0f };
	float fireInterval = 0.2f;
	float fireCooldown = 0.0f;

	void Update(SceneContext& ctx) override;

	void Fire(SceneContext& ctx);

private:
	MeshHandle m_quad;
	TextureHandle m_bulletTex;

public:
	void SetHandles(const MeshHandle quad, const TextureHandle bulletTex) { m_quad = quad; m_bulletTex = bulletTex; }

};