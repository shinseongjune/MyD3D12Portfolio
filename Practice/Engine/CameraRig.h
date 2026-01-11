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
    void SetTarget(EntityId target) { m_target = target; }

    void SetBaseOffset(DirectX::XMFLOAT3 off) { m_baseOffset = off; }
    void SetLookAtOffset(DirectX::XMFLOAT3 off) { m_lookAtOffset = off; }

    // Input event (CameraLook command)
    void OnLook(float dx, float dy);

    // Per-frame update (actually positions/rotates camera)
    void Update(World& world, float dt);

    // Shake trigger
    void Impulse(float duration, float strength);

private:
    bool m_followEnabled = true;
    EntityId m_cam = EntityId::Invalid();
    EntityId m_target = EntityId::Invalid();

    // orbit
    DirectX::XMFLOAT3 m_baseOffset = { 0.f, 2.f, -6.f };
    DirectX::XMFLOAT3 m_lookAtOffset = { 0.f, 1.f, 0.f };

    float m_yaw = 0.f;
    float m_pitch = 0.f;

    bool  m_hadLookInputThisFrame = false;
    float m_sensitivity = 0.025f;
    float m_returnSpeed = 4.0f;
    float m_remainingTime = 0.f;
    const float m_returnDelay = 0.8f;

    // shake
    DirectX::XMFLOAT3 m_shakeOffset = { 0,0,0 };
    DirectX::XMFLOAT3 m_shakeTarget = { 0,0,0 };
    float m_shakeTimeLeft = 0.f;
    float m_shakeStrength = 0.f;
    float m_shakeLerp = 20.f;
    float m_shakeEps = 0.05f;

    std::mt19937 m_rng{ 12345 };

private:
    static float Clamp(float v, float lo, float hi);
    static float Lerp(float a, float b, float t);
    static DirectX::XMFLOAT3 Lerp3(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float t);
    static float Smooth01(float k, float dt); // 1-exp(-k*dt)
};
