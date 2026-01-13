#pragma once
#include <DirectXMath.h>
#include "Behaviour.h"

struct TransformComponent;

class BulletComponent : public Behaviour
{
public:
	void Start(SceneContext& ctx) override;
	void Update(SceneContext& ctx) override;

	void SetOwner(const EntityId& ownerId) { owner = ownerId; }

private:
	float lifeTime = 5.0f; // sec
	float speed = 200.0f;  // m/s
	DirectX::XMFLOAT3 direction = { 0.0f, 0.0f, 1.0f };
	EntityId owner = EntityId::Invalid();

	void Step(SceneContext& ctx);

	void DestroyBullet(SceneContext& ctx);

};