#pragma once
#include "Behaviour.h"
#include <vector>
#include "MeshHandle.h"
#include "TextureHandle.h"

class StatsComponent : public Behaviour
{
public:
	void Update(SceneContext& ctx) override;

	float CurrentHp() { return m_hp; }
	void SetHp(float hp) { m_hp = hp; }

	void TakeDamage(float damage);

	
	void MakeDeadEffect(SceneContext& ctx);

	void SetQuadMesh(MeshHandle quad) { m_quad = quad; }
	void SetDieAnims(std::vector<TextureHandle> anims) { m_anims = anims; }

private:
	float m_hp = 100.0f;
	bool m_dead = false;
	bool m_deadEffectSpawned = false;
	MeshHandle m_quad;
	std::vector<TextureHandle> m_anims;

};