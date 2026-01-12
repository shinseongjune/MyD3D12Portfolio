#pragma once
#include "EntityId.h"
#include <DirectXMath.h>
#include <random>

class World;

class CameraRig
{
public:
    void SetFollowEnabled(bool on) { m_followEnabled = on; }
    bool IsFollowEnabled() const { return m_followEnabled; }

    void SetCamera(EntityId cam) { m_cam = cam; }
    void SetTarget(EntityId target);

    // Orbit radius (meters)
    void SetOrbitRadius(float r) { m_orbitRadius = (r < 0.1f) ? 0.1f : r; }

    // Look-at offset in aircraft local frame (right, up, forward)
    void SetLookAtOffset(DirectX::XMFLOAT3 off) { m_lookAtOffset = off; }

    // Roll follow: 0 = horizon(worldUp), 1 = follow aircraft up
    void SetRollFollow(float a) { m_rollFollow = a; }

    // Input: dx,dy (e.g. mouse delta or stick)
    void OnLook(float dx, float dy);

    // Shake trigger
    void Impulse(float duration, float strength);

    // Per-frame update
    void Update(World& world, float dt);

private:
    static float Clamp(float v, float lo, float hi)
    {
        return (v < lo) ? lo : (v > hi) ? hi : v;
    }

    static DirectX::XMVECTOR SafeNormalize(DirectX::FXMVECTOR v, DirectX::FXMVECTOR fallback)
    {
        float len = DirectX::XMVectorGetX(DirectX::XMVector3Length(v));
        if (len < 1e-6f) return fallback;
        return DirectX::XMVectorScale(v, 1.0f / len);
    }

    static DirectX::XMVECTOR SlerpSafe(DirectX::FXMVECTOR a, DirectX::FXMVECTOR b, float t)
    {
        if (DirectX::XMVectorGetX(DirectX::XMVector4Dot(a, b)) < 0.0f)
            return DirectX::XMQuaternionSlerp(a, DirectX::XMVectorNegate(b), t);
        return DirectX::XMQuaternionSlerp(a, b, t);
    }

    // Look-at quaternion with stable up + continuity (NO smoothing, just continuity)
    DirectX::XMVECTOR BuildLookAtQuaternionStable(
        DirectX::FXMVECTOR camPos,
        DirectX::FXMVECTOR lookAtPos,
        DirectX::FXMVECTOR preferredUp,
        DirectX::FXMVECTOR fallbackRight);

private:
    bool m_followEnabled = true;
    EntityId m_cam = EntityId::Invalid();
    EntityId m_target = EntityId::Invalid();

    // orbit angles (NO smoothing)
    float m_orbitSensitivity = 0.02f; // rad per input unit
    float m_orbitYaw = 0.0f;
    float m_orbitPitch = 0.0f;

    // yaw/pitch limit
    float m_orbitYawLimit = DirectX::XMConvertToRadians(180.f);
    float m_orbitPitchLimit = DirectX::XMConvertToRadians(45.f);

    // radius
    float m_orbitRadius = 4.5f;

    // Return-to-center: delay 후 즉시 0으로 스냅(스무딩 없음)
    float m_returnDelay = 0.8f;
    float m_remainingTime = 0.0f;
    bool  m_hadLookInputThisFrame = false;

    // Look-at offset (aircraft local)
    DirectX::XMFLOAT3 m_lookAtOffset{ 0.f, 1.f, 0.f };

    // Roll follow
    float m_rollFollow = 1.0f;

    // shake: 프레임-노이즈 없는 sin/cos 기반(스무딩 없음)
    float m_shakeTimeLeft = 0.0f;
    float m_shakeDuration = 0.0f;
    float m_shakeStrength = 0.0f;
    float m_shakePhaseX = 0.0f;
    float m_shakePhaseY = 0.0f;
    float m_shakeFreqX = 18.0f; // rad/sec 근처
    float m_shakeFreqY = 22.0f;

    std::mt19937 m_rng{ 12345 };

    // stable look-at basis continuity
    DirectX::XMFLOAT3 m_prevRight{ 1,0,0 };
    DirectX::XMFLOAT3 m_prevUp{ 0,1,0 };
    bool m_hasPrevBasis = false;

    // qTarget continuity (avoid q/-q flip)
    DirectX::XMFLOAT4 m_prevTargetQ{ 0,0,0,1 };
    bool m_hasPrevTargetQ = false;
};
