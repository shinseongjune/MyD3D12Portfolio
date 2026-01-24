#pragma once
#include "Behaviour.h"
#include <vector>
#include "MeshHandle.h"
#include "TextureHandle.h"
#include "SoundHandle.h"

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
	void SetDieSounds(std::vector<SoundHandle> sounds) { m_deathSounds = sounds; }

private:
	float m_hp = 100.0f;
	bool m_dead = false;
	bool m_deadEffectSpawned = false;
	MeshHandle m_quad;
	std::vector<TextureHandle> m_anims;
	std::vector<SoundHandle> m_deathSounds;

};