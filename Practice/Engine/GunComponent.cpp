#include "GunComponent.h"
#include <memory>
#include "SceneContext.h"
#include "BillboardComponent.h"
#include "BulletComponent.h"

void GunComponent::Update(SceneContext& ctx)
{
	m_fireCooldown -= ctx.dt;
	if (m_fireCooldown <= 0) m_fireCooldown = 0;
}

void GunComponent::Fire(SceneContext& ctx)
{
	using namespace DirectX;

	if (m_fireCooldown > 0) 
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

	static int bulletCount = 0;
	EntityId e = ctx.Instantiate("bulletSprite" + std::to_string(bulletCount++));
	ctx.world.AddTransform(e);
	ctx.world.AddMesh(e, MeshComponent{ m_quad });

	MaterialComponent matCom{ XMFLOAT4(1,1,1,1), m_bulletTex };
	auto& mat = matCom.Primary();
	mat.transparent = true;
	mat.unlit = true;
	ctx.world.AddMaterial(e, matCom);
	ctx.world.SetLocalPosition(e, out);

	ctx.world.AddScript(e, std::make_unique<BillboardComponent>(BillboardMode::Spherical));

	auto bullet = std::make_unique<BulletComponent>();
	bullet->SetOwner(Entity());
	bullet->SetQuadMesh(m_quad);
   	bullet->SetBulletHitAnims(m_bulletHitAnims);
	ctx.world.AddScript(e, std::move(bullet));

	m_fireCooldown = m_fireInterval;
}
