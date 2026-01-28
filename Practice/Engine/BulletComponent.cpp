#include "BulletComponent.h"
#include <memory>
#include "SceneContext.h"
#include "PhysicsSystem.h"
#include "StatsComponent.h"
#include "MyRandom.h"
#include "EffectComponent.h"
#include "BillboardComponent.h"

void BulletComponent::Start(SceneContext& ctx)
{
	if (owner.IsValid() && ctx.world.IsAlive(owner) && ctx.world.HasTransform(owner))
	{
		const TransformComponent& ownerTr = ctx.world.GetTransform(owner);
		XMVECTOR q = XMLoadFloat4(&ownerTr.rotation);
		XMVECTOR f = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), q);
		XMStoreFloat3(&direction, XMVector3Normalize(f));
	}
}

void BulletComponent::Update(SceneContext& ctx)
{
	Step(ctx);

	m_lifeTime -= ctx.dt;
	if (m_lifeTime <= 0.0f)
	{
		DestroyBullet(ctx);
	}
}

void BulletComponent::Step(SceneContext& ctx)
{
    auto& tr = ctx.world.GetTransform(Entity());

    // dir normalize
    XMVECTOR dirV = XMLoadFloat3(&direction);
    dirV = XMVector3Normalize(dirV);
    DirectX::XMFLOAT3 dirN;
    XMStoreFloat3(&dirN, dirV);

    XMVECTOR posV = XMLoadFloat3(&tr.position);
    float dist = m_speed * ctx.dt;

    PhysicsSystem::RaycastHit hit{};

    uint32_t mask = 0xFFFFFFFFu;
    auto& ownerCol = ctx.world.GetCollider(owner);
    mask &= ~ownerCol.layer;

    if (ctx.physics.Raycast(ctx.world, tr.position, dirN, dist, hit, mask, false))
    {
        // 맞은 지점으로 스냅
        tr.position = hit.point;

        EntityId hitEntity = hit.entity;
        if (ctx.world.HasScript(hitEntity))
        {
            auto* stats = ctx.world.GetScriptAs<StatsComponent>(hitEntity);
            if (stats != nullptr)
            {
                stats->TakeDamage(20);
            }
        }
        MakeHitEffect(ctx);

        auto hitSound = m_bulletHitSounds[RandRangeInt(0, (int)m_bulletHitSounds.size() - 1)];
        ctx.PlaySFX(hitSound);

        DestroyBullet(ctx);
        return;
    }

    // 이동
    XMVECTOR newPosV = XMVectorAdd(posV, XMVectorScale(dirV, dist));
    XMStoreFloat3(&tr.position, newPosV);
}

void BulletComponent::DestroyBullet(SceneContext& ctx)
{
	ctx.world.RequestDestroy(Entity());
}

void BulletComponent::MakeHitEffect(SceneContext& ctx)
{
    if (!m_quad.IsValid()) return;
    if (m_bulletHitAnims.empty()) return;

    EntityId effect = ctx.Instantiate("hitof_" + ctx.world.GetName(Entity()));

    ctx.world.AddTransform(effect);
    ctx.world.SetLocalPosition(effect, ctx.world.GetLocalPosition(Entity()));
    float scale = RandRange(1.5f, 4.5f);
    ctx.world.SetLocalScale(effect, { scale, scale, scale });

    ctx.world.AddMesh(effect, MeshComponent{ m_quad });

    MaterialComponent dieMat;
    auto& mat = dieMat.Primary();
    mat.albedo = m_bulletHitAnims[0];
    mat.transparent = true;
    mat.unlit = true;
    ctx.world.AddMaterial(effect, dieMat);

    ctx.world.AddScript(effect, std::make_unique<BillboardComponent>(BillboardMode::Spherical));

    auto dieEffect = std::make_unique<EffectComponent>();
    dieEffect->SetAnims(m_bulletHitAnims);
    dieEffect->SetLifeTime(RandRange(0.4f, 0.8f));
    dieEffect->SetFrameDuration(RandRange(0.05f, 0.18f));
    ctx.world.AddScript(effect, std::move(dieEffect));
}
