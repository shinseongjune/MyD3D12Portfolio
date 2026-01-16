#include "PlayerBooster.h"
#include <string>
#include <format>
#include <memory>
#include "SceneContext.h"
#include "FlightRigComponent.h"
#include "BillboardComponent.h"
#include "EffectComponent.h"

void PlayerBooster::Update(SceneContext& ctx)
{
    auto flightRig = ctx.world.GetScriptAs<FlightRigComponent>(Entity());
    if (!flightRig) return;

    float speed01 = (flightRig->maxSpeed > 1e-6f) ? (flightRig->currentSpeed / flightRig->maxSpeed) : 0.f;
    float spacing = std::max(0.15f, m_emitSpacing * (1.0f - 0.3f * speed01)); // 빠를수록 약간 촘촘

    // 현재 트랜스폼
    const auto& tr = ctx.world.GetTransform(Entity());
    DirectX::XMFLOAT3 currPos = tr.position;
    DirectX::XMFLOAT4 currRot = tr.rotation;

    if (!m_hasPrev)
    {
        m_prevPos = currPos;
        m_prevRot = currRot;
        m_hasPrev = true;
        return;
    }

    using namespace DirectX;
    XMVECTOR p0 = XMLoadFloat3(&m_prevPos);
    XMVECTOR p1 = XMLoadFloat3(&currPos);
    XMVECTOR seg = XMVectorSubtract(p1, p0);

    float segLen = XMVectorGetX(XMVector3Length(seg));
    if (segLen < 1e-6f)
    {
        m_prevPos = currPos;
        m_prevRot = currRot;
        return;
    }

    // 프레임 동안 이동한 거리 + 누적 carry
    float dist = segLen;
    float remaining = dist;

    // 첫 방출까지 필요한 거리 (carry가 있으면 더 빨리 터짐)
    float need = spacing - m_emitCarry;

    // 필요 거리보다 이번 프레임 이동거리가 적으면 carry만 누적
    if (remaining < need)
    {
        m_emitCarry += remaining;
        m_prevPos = currPos;
        m_prevRot = currRot;
        return;
    }

    // 방출 1회 이상 가능: 경로 위에 여러 번 찍기
    // “현재 프레임 경로”에서 방출될 위치의 alpha(0~1)를 계산해서 그 지점에 생성
    float traveled = need;
    while (traveled <= dist + 1e-6f)
    {
        float alpha = traveled / dist; // prev->curr 사이 비율

        // 위치 보간
        XMVECTOR posV = XMVectorLerp(p0, p1, alpha);

        // 회전도 보간(오프셋 회전에 필요)
        XMVECTOR q0 = XMLoadFloat4(&m_prevRot);
        XMVECTOR q1 = XMLoadFloat4(&currRot);
        XMVECTOR q = XMQuaternionNormalize(XMQuaternionSlerp(q0, q1, alpha));

        MakeFlamesAt(ctx, posV, q);

        traveled += spacing;
    }

    // 마지막 overshoot만 carry로 저장
    m_emitCarry = std::fmod(dist - (traveled - spacing), spacing);
    if (m_emitCarry < 0.f) m_emitCarry = 0.f;

    m_prevPos = currPos;
    m_prevRot = currRot;
}

void PlayerBooster::MakeBoosterFlame(SceneContext& ctx, DirectX::FXMVECTOR basePosV, DirectX::FXMVECTOR qEntity, DirectX::XMFLOAT3 localOffset)
{
    using namespace DirectX;
    if (!m_quad.IsValid()) return;
    if (m_boosterAnims.empty()) return;

    static int count = 0;
    std::string name = std::format("boost{}Of", count++);
    EntityId effect = ctx.Instantiate(name + ctx.world.GetName(Entity()));

    ctx.world.AddTransform(effect);

    // 로컬 offset을 기체 회전으로 회전시켜서 월드로
    XMVECTOR offsetV = XMLoadFloat3(&localOffset);
    XMVECTOR rotatedOffsetV = XMVector3Rotate(offsetV, qEntity);

    XMVECTOR flamePosV = XMVectorAdd(basePosV, rotatedOffsetV);

    XMFLOAT3 flamePos;
    XMStoreFloat3(&flamePos, flamePosV);
    ctx.world.SetLocalPosition(effect, flamePos);
    ctx.world.SetLocalScale(effect, { 0.18f, 0.18f, 0.18f });

    ctx.world.AddMesh(effect, MeshComponent{ m_quad });

    MaterialComponent flameMat;
    auto& mat = flameMat.Primary();
    mat.albedo = m_boosterAnims[0];
    mat.transparent = true;
    mat.unlit = true;
    ctx.world.AddMaterial(effect, flameMat);

    ctx.world.AddScript(effect, std::make_unique<BillboardComponent>(BillboardMode::Spherical));

    auto flameEffect = std::make_unique<EffectComponent>();
    flameEffect->SetAnims(m_boosterAnims);
    flameEffect->SetLifeTime(0.23f);
    flameEffect->SetFrameDuration(0.023f);
    ctx.world.AddScript(effect, std::move(flameEffect));
}

void PlayerBooster::MakeFlamesAt(SceneContext& ctx, DirectX::FXMVECTOR basePosV, DirectX::FXMVECTOR qEntity)
{
    MakeBoosterFlame(ctx, basePosV, qEntity, m_leftBoosterOffset);
    MakeBoosterFlame(ctx, basePosV, qEntity, m_rightBoosterOffset);
}