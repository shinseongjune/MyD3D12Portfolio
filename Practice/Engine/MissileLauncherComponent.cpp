#include "MissileLauncherComponent.h"
#include "SceneContext.h"
#include "MissileComponent.h"

void MissileLauncherComponent::Update(SceneContext& ctx)
{
	m_fireCooldown -= ctx.dt;
	if (m_fireCooldown <= 0) m_fireCooldown = 0;
}

void MissileLauncherComponent::Fire(SceneContext& ctx)
{
	using namespace DirectX;

	if (m_fireCooldown > 0 || m_missileCount <= 0)
	{
		return;
	}

	TransformComponent& tr = ctx.world.GetTransform(Entity());
	XMVECTOR q = XMLoadFloat4(&tr.rotation);

	XMVECTOR off = XMLoadFloat3(&muzzleOffset);
	XMVECTOR offW = XMVector3Rotate(off, q);

	XMVECTOR pos = XMLoadFloat3(&tr.position);
	XMVECTOR muzzlePos = XMVectorAdd(pos, offW);

	XMFLOAT3 out;
	XMStoreFloat3(&out, muzzlePos);

	static int missileCount = 0;
	EntityId e = ctx.SpawnModel(m_missileMesh, SpawnModelOptions{std::string("missile") + std::to_string(missileCount++)}).value;

	MaterialComponent matCom{ XMFLOAT4(1,1,1,1), m_missileTex };
	ctx.world.AddMaterial(e, matCom);
	ctx.world.SetLocalPosition(e, out);
	ctx.world.SetLocalRotation(e, ctx.world.GetLocalRotation(Entity()));

	auto bullet = std::make_unique<MissileComponent>();
	bullet->SetOwner(Entity());
	bullet->SetQuadMesh(m_quad);
	bullet->SetBoomAnims(m_boomAnims);
	ctx.world.AddScript(e, std::move(bullet));

	m_fireCooldown = m_fireInterval;
	m_missileCount--;
}
