#pragma once
#include <DirectXMath.h>
#include <vector>
#include "Behaviour.h"
#include "MeshHandle.h"
#include "TextureHandle.h"
#include "SoundHandle.h"

class GunComponent : public Behaviour
{
public:
	DirectX::XMFLOAT3 muzzleOffset{ 0.0f, 0.0f, 0.5f };
	float m_fireInterval = 0.125f;
	float m_fireCooldown = 0.0f;

	void Update(SceneContext& ctx) override;

	void Fire(SceneContext& ctx);

	void SetHandles(const MeshHandle quad, const TextureHandle bulletTex) { m_quad = quad; m_bulletTex = bulletTex; }
	void SetBulletHitAnims(std::vector<TextureHandle> anims) { m_bulletHitAnims = anims; }
	void SetGunFireSounds(std::vector<SoundHandle> sounds) { m_gunFireSounds = sounds; }
	void SetBulletHitSounds(std::vector<SoundHandle> sounds) { m_bulletHitSounds = sounds; }

private:
	MeshHandle m_quad;
	TextureHandle m_bulletTex;
	std::vector<TextureHandle> m_bulletHitAnims;
	std::vector<SoundHandle> m_gunFireSounds;
	std::vector<SoundHandle> m_bulletHitSounds;

};