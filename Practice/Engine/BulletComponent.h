#pragma once
#include <DirectXMath.h>
#include "Behaviour.h"
#include <vector>
#include "TextureHandle.h"
#include "MeshHandle.h"
#include "SoundHandle.h"

class BulletComponent : public Behaviour
{
public:
	void Start(SceneContext& ctx) override;
	void Update(SceneContext& ctx) override;

	void SetOwner(const EntityId& ownerId) { owner = ownerId; }
	void SetQuadMesh(MeshHandle quad) { m_quad = quad; }
	void SetBulletHitAnims(std::vector<TextureHandle> anims) { m_bulletHitAnims = anims; }
	void SetBulletHitSounds(std::vector<SoundHandle> sounds) { m_bulletHitSounds = sounds; }

private:
	float m_lifeTime = 5.0f; // sec
	float m_speed = 200.0f;  // m/s
	DirectX::XMFLOAT3 direction = { 0.0f, 0.0f, 1.0f };
	EntityId owner = EntityId::Invalid();
	uint32_t m_ownerLayerMask = 0;

	MeshHandle m_quad;
	std::vector<TextureHandle> m_bulletHitAnims;
	std::vector<SoundHandle> m_bulletHitSounds;

	void Step(SceneContext& ctx);

	void DestroyBullet(SceneContext& ctx);

	void MakeHitEffect(SceneContext& ctx);

};