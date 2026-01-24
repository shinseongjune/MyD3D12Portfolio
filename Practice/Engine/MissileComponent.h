#pragma once
#include <DirectXMath.h>
#include "Behaviour.h"
#include <vector>
#include "TextureHandle.h"
#include "MeshHandle.h"
#include "SoundHandle.h"

class MissileComponent : public Behaviour
{
public:
	void Start(SceneContext& ctx) override;
	void Update(SceneContext& ctx) override;

	void SetOwner(const EntityId& ownerId) { owner = ownerId; }
	void SetQuadMesh(MeshHandle quad) { m_quad = quad; }
	void SetBoomAnims(std::vector<TextureHandle> anims) { m_boomAnims = anims; }
	void SetBoomSound(SoundHandle sound) { m_boomSound = sound; }

private:
	float m_lifeTime = 10.0f; // sec
	float m_speed = 45.0f;    // m/s
	DirectX::XMFLOAT3 direction = { 0.0f, 0.0f, 1.0f };
	EntityId owner = EntityId::Invalid();

	MeshHandle m_quad;
	std::vector<TextureHandle> m_boomAnims;
	SoundHandle m_boomSound;

	float m_seekRadius = 800.0f;     // 탐지 반경
	float m_seekHalfFovDeg = 60.0f;   // 전방 120도 = 반각 60도
	float m_turnRateDeg = 180.0f;    // 초당 최대 회전(도). 180이면 1초에 반바퀴

	void Step(SceneContext& ctx);

	void DestroyMissile(SceneContext& ctx);

	void MakeBoomEffect(SceneContext& ctx);

};