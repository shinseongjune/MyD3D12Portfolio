#include "StatsComponent.h"
#include "MyRandom.h"
#include "SceneContext.h"
#include "MeshComponent.h"
#include "MaterialComponent.h"
#include "EffectComponent.h"

void StatsComponent::Update(SceneContext& ctx)
{
	if (m_deadEffectSpawned) return;

	if (m_dead)
	{
		ctx.world.RequestDestroy(Entity());
		MakeDeadEffect(ctx);
		m_deadEffectSpawned = true;
	}
}

void StatsComponent::TakeDamage(float damage)
{
	if (m_dead || m_hp <= 0) return;

	m_hp -= damage;
	if (m_hp <= 0)
	{
		m_hp = 0;
		m_dead = true;
	}
}

void StatsComponent::MakeDeadEffect(SceneContext& ctx)
{
	if (!m_quad.IsValid()) return;
	if (m_anims.empty()) return;

	EntityId effect = ctx.Instantiate("deathof_" + ctx.world.GetName(Entity()));
	
	ctx.world.AddTransform(effect);
	ctx.world.SetLocalPosition(effect, ctx.world.GetLocalPosition(Entity()));
	float scale = RandRange(0.5f, 1.5f);
	ctx.world.SetLocalScale(effect, { scale, scale, scale });

	ctx.world.AddMesh(effect, MeshComponent{ m_quad });
	
	MaterialComponent dieMat;
	dieMat.Primary().albedo = m_anims[0];
	ctx.world.AddMaterial(effect, dieMat);
	
	auto dieEffect = std::make_unique<EffectComponent>();
	dieEffect->SetAnims(m_anims);
	dieEffect->SetLifeTime(RandRange(1, 1.5f));
	dieEffect->SetFrameDuration(RandRange(0.1f, 0.23f));
	ctx.world.AddScript(effect, std::move(dieEffect));
}
