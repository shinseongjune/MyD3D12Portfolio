#pragma once
#include "Behaviour.h"
#include <vector>

struct TextureHandle;

/// <summary>
/// MeshComponent, MaterialComponent, BillboardComponent ÇÊ¼ö
/// </summary>
class EffectComponent : public Behaviour
{
public:
	void Start(SceneContext& ctx) override;
	void Update(SceneContext& ctx) override;
	void SetLifeTime(float lifeTime) { m_lifeTime = lifeTime; }
	void SetFrameDuration(float duration) { m_frameDuration = duration; }
	void SetAnims(const std::vector<TextureHandle>& anims) { m_anims = anims; }

private:
	float m_lifeTime = 1.0f;
	std::vector<TextureHandle> m_anims;
	int m_currentIndex = 0;
	float m_frameDuration = 0.2f;
	float m_currentDuration = 0.0f;

};