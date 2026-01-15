#pragma once
#include "Behaviour.h"
#include <DirectXMath.h>
#include <vector>
#include "MeshHandle.h"
#include "TextureHandle.h"

class PlayerBooster : public Behaviour
{
public:
	void Update(SceneContext& ctx) override;

	void SetQuadMesh(MeshHandle quad) { m_quad = quad; }
	void SetBoosterAnims(std::vector<TextureHandle> anims) { m_boosterAnims = anims; }
	void MakeBoosterFlame(SceneContext& ctx, DirectX::FXMVECTOR basePosV, DirectX::FXMVECTOR qEntity, DirectX::XMFLOAT3 localOffset);
	void MakeFlamesAt(SceneContext& ctx, DirectX::FXMVECTOR basePosV, DirectX::FXMVECTOR qEntity);

private:
	DirectX::XMFLOAT3 m_rightBoosterOffset{ 0.43f, 0.2f, -0.75f };
	DirectX::XMFLOAT3 m_leftBoosterOffset{ -0.43f, 0.2f, -0.75f };
	MeshHandle m_quad;
	std::vector<TextureHandle> m_boosterAnims;

	DirectX::XMFLOAT3 m_prevPos{};
	DirectX::XMFLOAT4 m_prevRot{ 0,0,0,1 };
	bool m_hasPrev = false;

	float m_emitSpacing = 0.6f;   // 불꽃 간격(월드 단위)
	float m_emitCarry = 0.0f;     // 지난 프레임에서 남은 거리 누적

};