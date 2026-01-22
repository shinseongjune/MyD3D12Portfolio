#pragma once
#include <DirectXMath.h>
#include <vector>
#include "Behaviour.h"
#include "MeshHandle.h"
#include "TextureHandle.h"
#include "ModelAsset.h"

class MissileLauncherComponent : public Behaviour
{
public:
	DirectX::XMFLOAT3 muzzleOffset{ 0.0f, -0.1f, 0.5f };
	float m_fireInterval = 0.3f;
	float m_fireCooldown = 0.0f;

	int m_missileCount = 10;

	void Update(SceneContext& ctx) override;

	void Fire(SceneContext& ctx);

	void SetHandles(const MeshHandle quad, const ModelAsset missileMesh, const TextureHandle missileTex) { m_quad = quad, m_missileMesh = missileMesh; m_missileTex = missileTex; }
	void SetMissileHitAnims(std::vector<TextureHandle> anims) { m_boomAnims = anims; }

private:
	MeshHandle m_quad;
	ModelAsset m_missileMesh;
	TextureHandle m_missileTex;
	std::vector<TextureHandle> m_boomAnims;

};