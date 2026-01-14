#include "EffectComponent.h"
#include "SceneContext.h"

void EffectComponent::Start(SceneContext& ctx)
{
	m_currentIndex = 0;
	m_currentDuration = m_frameDuration;
}

void EffectComponent::Update(SceneContext& ctx)
{
	if (m_anims.empty())
	{
		ctx.world.RequestDestroy(Entity());
		return;
	}

	m_lifeTime -= ctx.dt;
	if (m_lifeTime <= 0.0f)
	{
		ctx.world.RequestDestroy(Entity());
		return;
	}

	m_currentDuration -= ctx.dt;

	while (m_currentDuration <= 0.0f)
	{
		m_currentIndex = (m_currentIndex + 1) % (int)m_anims.size();

		auto& mat = ctx.world.GetMaterial(Entity());
		mat.Primary().albedo = m_anims[m_currentIndex];

		m_currentDuration += m_frameDuration;
	}
}
