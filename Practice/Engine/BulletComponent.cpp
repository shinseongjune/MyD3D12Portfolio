#include "BulletComponent.h"
#include "SceneContext.h"
#include "PhysicsSystem.h"
#include "StatsComponent.h"
#if defined(_DEBUG)
#include <Windows.h>
#endif

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

	lifeTime -= ctx.dt;
	if (lifeTime <= 0.0f)
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
    float dist = speed * ctx.dt;

    PhysicsSystem::RaycastHit hit{};

    uint32_t mask = 0xFFFFFFFFu;

    if (ctx.physics.Raycast(ctx.world, tr.position, dirN, dist, hit, mask, false))
    {
        if (!owner.IsValid() || hit.entity != owner)
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
                    OutputDebugStringA("test hit");
                }
            }

            DestroyBullet(ctx);
            return;
        }
    }

    // 이동
    XMVECTOR newPosV = XMVectorAdd(posV, XMVectorScale(dirV, dist));
    XMStoreFloat3(&tr.position, newPosV);
}

void BulletComponent::DestroyBullet(SceneContext& ctx)
{
	ctx.world.RequestDestroy(Entity());
}
