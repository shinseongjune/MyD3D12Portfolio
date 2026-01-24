#include "MissileComponent.h"
#include <memory>
#include "SceneContext.h"
#include "MyRandom.h"
#include "BillboardComponent.h"
#include "EffectComponent.h"
#include "PhysicsSystem.h"
#include "StatsComponent.h"

static DirectX::XMVECTOR MakeLookRotation_ZForward(DirectX::XMVECTOR forwardN)
{
    using namespace DirectX;

    forwardN = XMVector3Normalize(forwardN);

    // 기본 up (Y+) 사용
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    // forward가 up과 거의 평행하면 다른 up을 사용 (특이점 방지)
    float d = XMVectorGetX(XMVector3Dot(forwardN, up));
    if (fabsf(d) > 0.98f)
        up = XMVectorSet(0, 0, 1, 0);

    // right = up x forward
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forwardN));
    XMVECTOR realUp = XMVector3Cross(forwardN, right);

    // 회전행렬(오쏘노멀)
    XMMATRIX rot{};
    rot.r[0] = XMVectorSetW(right, 0.0f);
    rot.r[1] = XMVectorSetW(realUp, 0.0f);
    rot.r[2] = XMVectorSetW(forwardN, 0.0f);
    rot.r[3] = XMVectorSet(0, 0, 0, 1);

    return XMQuaternionNormalize(XMQuaternionRotationMatrix(rot));
}

void MissileComponent::Start(SceneContext& ctx)
{
	if (owner.IsValid() && ctx.world.IsAlive(owner) && ctx.world.HasTransform(owner))
	{
		const TransformComponent& ownerTr = ctx.world.GetTransform(owner);
		XMVECTOR q = XMLoadFloat4(&ownerTr.rotation);
		XMVECTOR f = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), q);
		XMStoreFloat3(&direction, XMVector3Normalize(f));
	}
}

void MissileComponent::Update(SceneContext& ctx)
{
	Step(ctx);

	m_lifeTime -= ctx.dt;
	if (m_lifeTime <= 0.0f)
	{
        MakeBoomEffect(ctx);
        ctx.PlaySFX(m_boomSound);
		DestroyMissile(ctx);
	}
}

void MissileComponent::Step(SceneContext& ctx)
{
    const auto& tr = ctx.world.GetTransform(Entity());
    XMVECTOR posV = XMLoadFloat3(&tr.position);

    // 1) 현재 dir 정규화
    XMVECTOR dirV = XMVector3Normalize(XMLoadFloat3(&direction));

    // 2) 타겟 탐색 + direction을 "조금씩" 조향
    {
        const float radius = m_seekRadius;
        const float cosHalfFov = cosf(DirectX::XMConvertToRadians(m_seekHalfFovDeg)); // 전방 판정용

        EntityId best{ EntityId::InvalidIndex, 0 };
        float bestDistSq = radius * radius;

        XMVECTOR selfPosV = XMLoadFloat3(&tr.position);

        const auto& candidates = ctx.world.GetColliderEntities();
        for (EntityId e : candidates)
        {
            if (!ctx.world.IsAlive(e)) continue;
            if (e == Entity()) continue;
            if (owner.IsValid() && e == owner) continue;

            if (!ctx.world.HasScript(e)) continue;
            auto* stats = ctx.world.GetScriptAs<StatsComponent>(e);
            if (!stats) continue;

            if (!ctx.world.HasTransform(e)) continue;

            XMVECTOR targetPosV = XMLoadFloat3(&ctx.world.GetTransform(e).position);
            XMVECTOR toV = XMVectorSubtract(targetPosV, selfPosV);

            // 거리 제한
            float distSq;
            XMStoreFloat(&distSq, XMVector3LengthSq(toV));
            if (distSq > bestDistSq) continue;

            // 전방 120도 제한 (dot >= cos(60도))
            XMVECTOR toN = XMVector3Normalize(toV);
            float dot;
            XMStoreFloat(&dot, XMVector3Dot(dirV, toN));
            if (dot < cosHalfFov) continue;

            best = e;
            bestDistSq = distSq;
        }

        if (best.IsValid())
        {
            XMVECTOR targetPosV = XMLoadFloat3(&ctx.world.GetTransform(best).position);
            XMVECTOR desiredV = XMVector3Normalize(XMVectorSubtract(targetPosV, selfPosV));

            // 3) 초당 최대 회전각만큼만 dir -> desired로 회전
            float maxRad = DirectX::XMConvertToRadians(m_turnRateDeg) * ctx.dt;

            float dot;
            XMStoreFloat(&dot, XMVector3Dot(dirV, desiredV));
            dot = std::clamp(dot, -1.0f, 1.0f);
            float angle = acosf(dot);

            if (angle > 1e-5f)
            {
                float t = (maxRad >= angle) ? 1.0f : (maxRad / angle);

                XMVECTOR newDir = XMVector3Normalize(XMVectorLerp(dirV, desiredV, t));
                dirV = newDir;
                XMStoreFloat3(&direction, dirV);
            }
        }
    }

    XMStoreFloat3(&direction, dirV);

    // 2) 회전 갱신 (진행방향 = forward)
    XMVECTOR q = MakeLookRotation_ZForward(dirV);
    XMFLOAT4 qf; XMStoreFloat4(&qf, q);
    ctx.world.SetLocalRotation(Entity(), qf);

    // 3) 레이캐스트/이동도 setter 사용
    XMFLOAT3 dirN; XMStoreFloat3(&dirN, dirV);
    float dist = m_speed * ctx.dt;

    PhysicsSystem::RaycastHit hit{};
    uint32_t mask = 0xFFFFFFFFu;

    if (ctx.physics.Raycast(ctx.world, tr.position, dirN, dist, hit, mask, false))
    {
        if (!owner.IsValid() || hit.entity != owner)
        {
            ctx.world.SetLocalPosition(Entity(), hit.point); // ★ setter 사용

            EntityId hitEntity = hit.entity;
            if (ctx.world.HasScript(hitEntity))
            {
                auto* stats = ctx.world.GetScriptAs<StatsComponent>(hitEntity);
                if (stats) stats->TakeDamage(100);
            }

            MakeBoomEffect(ctx);
            ctx.PlaySFX(m_boomSound);
            DestroyMissile(ctx);
            return;
        }
    }

    // 이동
    XMVECTOR newPosV = XMVectorAdd(posV, XMVectorScale(dirV, dist));
    XMFLOAT3 newPos; XMStoreFloat3(&newPos, newPosV);
    ctx.world.SetLocalPosition(Entity(), newPos);
}

void MissileComponent::DestroyMissile(SceneContext& ctx)
{
	ctx.world.RequestDestroy(Entity());
}

void MissileComponent::MakeBoomEffect(SceneContext& ctx)
{
    if (!m_quad.IsValid()) return;
    if (m_boomAnims.empty()) return;

    EntityId effect = ctx.Instantiate("hitof_" + ctx.world.GetName(Entity()));

    ctx.world.AddTransform(effect);
    ctx.world.SetLocalPosition(effect, ctx.world.GetLocalPosition(Entity()));
    float scale = RandRange(1.5f, 4.5f);
    ctx.world.SetLocalScale(effect, { scale, scale, scale });

    ctx.world.AddMesh(effect, MeshComponent{ m_quad });

    MaterialComponent dieMat;
    auto& mat = dieMat.Primary();
    mat.albedo = m_boomAnims[0];
    mat.transparent = true;
    mat.unlit = true;
    ctx.world.AddMaterial(effect, dieMat);

    ctx.world.AddScript(effect, std::make_unique<BillboardComponent>(BillboardMode::Spherical));

    auto dieEffect = std::make_unique<EffectComponent>();
    dieEffect->SetAnims(m_boomAnims);
    dieEffect->SetLifeTime(RandRange(0.4f, 0.8f));
    dieEffect->SetFrameDuration(RandRange(0.1f, 0.3f));
    ctx.world.AddScript(effect, std::move(dieEffect));
}