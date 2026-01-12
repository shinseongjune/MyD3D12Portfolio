#include "FlightRigComponent.h"
#include "FlightRigSystem.h"
#include "SceneContext.h"
#include "World.h"
#include "DebugDraw.h"

using namespace DirectX;

void FlightRigComponent::Start(SceneContext& ctx)
{
    // Give a small initial airspeed so controls feel responsive immediately.
    // This is only applied if we are essentially stationary.
    if (!ctx.world.HasTransform(Entity())) return;
    auto& tr = ctx.world.GetTransform(Entity());

    // Initialize basis (forward/up) from current transform rotation.
    {
        XMVECTOR q = XMLoadFloat4(&tr.rotation);
        q = XMQuaternionNormalize(q);

        XMVECTOR fwd = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), q);
        XMVECTOR up  = XMVector3Rotate(XMVectorSet(0, 1, 0, 0), q);

        fwd = XMVector3Normalize(fwd);
        // Orthonormalize up against fwd in case rotation isn't perfectly clean.
        up = XMVectorSubtract(up, XMVectorScale(fwd, XMVectorGetX(XMVector3Dot(up, fwd))));
        up = XMVector3Normalize(up);

        XMStoreFloat3(&forwardDir, fwd);
        XMStoreFloat3(&upDir, up);
    }

    // Give a small initial airspeed so controls feel responsive immediately.
    const XMVECTOR v = XMLoadFloat3(&velocity);
    const float speed = XMVectorGetX(XMVector3Length(v));
    if (speed < 0.01f && initialSpeed > 0.0f)
    {
        XMVECTOR fwd = XMLoadFloat3(&forwardDir);
        XMVECTOR vel = XMVectorScale(fwd, initialSpeed);
        XMStoreFloat3(&velocity, vel);
    }

    rollAngle = 0.0f;

    throttle01 = std::max(0.0f, std::min(1.0f, initialThrottle01));
}

void FlightRigComponent::Update(SceneContext& ctx)
{
    if (!ctx.world.HasTransform(Entity()))
        return;

    auto& tr = ctx.world.GetTransform(Entity());
    FlightRigSystem::Step(tr, *this, m_input, ctx.dt);

    // Debug: draw current velocity direction as a red line.
    {
        DirectX::XMVECTOR v = DirectX::XMLoadFloat3(&velocity);
        float speed = DirectX::XMVectorGetX(DirectX::XMVector3Length(v));
        if (speed > 0.1f)
        {
            DirectX::XMVECTOR dir = DirectX::XMVector3Normalize(v);
            constexpr float kLen = 6.0f;
            DirectX::XMVECTOR aV = DirectX::XMLoadFloat3(&tr.position);
            DirectX::XMVECTOR bV = DirectX::XMVectorAdd(aV, DirectX::XMVectorScale(dir, kLen));

            DirectX::XMFLOAT3 a, b;
            DirectX::XMStoreFloat3(&a, aV);
            DirectX::XMStoreFloat3(&b, bV);
            DebugDraw::Line(a, b, { 1,0,0,1 });
        }
    }

    // Consume input so if the scene stops feeding it, we naturally return to neutral.
    m_input = {};
}
