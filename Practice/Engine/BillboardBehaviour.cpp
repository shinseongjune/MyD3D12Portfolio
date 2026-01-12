#include "BillboardBehaviour.h"
#include "SceneContext.h"
#include "World.h"
#include "TransformComponent.h"

using namespace DirectX;

static XMFLOAT3 GetWorldPos(const TransformComponent& t)
{
    // row-vector convention: translation is _41,_42,_43
    return { t.world._41, t.world._42, t.world._43 };
}

void BillboardBehaviour::Update(SceneContext& ctx)
{
    auto& w = ctx.world;

    EntityId self = Entity();
    if (!w.HasTransform(self)) return;

    EntityId cam = w.FindActiveCamera();
    if (!w.IsAlive(cam) || !w.HasTransform(cam)) return;

    // transforms are updated after scripts in Application loop,
    // but cam/world matrix should be valid from previous frame.
    // (필요하면 w.UpdateTransformNow(cam) 같은 걸 써도 됨)
    const auto& selfTr = w.GetTransform(self);
    const auto& camTr = w.GetTransform(cam);

    XMFLOAT3 pSelf = GetWorldPos(selfTr);
    XMFLOAT3 pCam = GetWorldPos(camTr);

    XMVECTOR P = XMLoadFloat3(&pSelf);
    XMVECTOR CP = XMLoadFloat3(&pCam);

    XMVECTOR forward = CP - P;

    if (mode == BillboardMode::CylindricalY)
        forward = XMVectorSetY(forward, 0.f);

    if (XMVectorGetX(XMVector3LengthSq(forward)) < 1e-8f)
        return;

    forward = XMVector3Normalize(forward);

    XMVECTOR up = XMVectorSet(0, 1, 0, 0);
    XMVECTOR right = XMVector3Cross(up, forward);

    // 거의 평행이면 fallback
    if (XMVectorGetX(XMVector3LengthSq(right)) < 1e-8f)
    {
        up = XMVectorSet(0, 0, 1, 0);
        right = XMVector3Cross(up, forward);
    }

    right = XMVector3Normalize(right);
    up = XMVector3Cross(forward, right);

    // row-vector 관례: 축을 "행(row)"에 넣는 방식이 직관적으로 맞음
    XMMATRIX R(
        right,
        up,
        forward,
        XMVectorSet(0, 0, 0, 1)
    );

    XMVECTOR q = XMQuaternionRotationMatrix(R);

    auto& tr = w.GetTransform(self);
    XMStoreFloat4(&tr.rotation, q);
    tr.dirty = true; // World.UpdateTransforms()에서 월드행렬 갱신되게
}
