#pragma once
#include <DirectXMath.h>
#include "Behaviour.h"
#include "FlightRigComponent.h"

class GunComponent;
class MissileLauncherComponent;

// Very small, input-driving AI for an arcade aircraft.
// - States: Chase / Attack / Disengage (Return-to-map)
// - Feeds FlightInput into FlightRigComponent each frame.
// - Calls GunComponent::Fire(ctx), MissileLauncherComponent::Fire(ctx) when conditions are met.
class EnemyAIComponent : public Behaviour
{
public:
    enum class State
    {
        Chase,
        Attack,
        Disengage,
    };

    // ===== Setup =====
    EntityId target = {}; // player entity
    DirectX::XMFLOAT3 mapCenter{ 0,0,0 };
    float mapRadius = 250.0f;

    // ===== Tuning =====
    // Distances
    float attackEnterDist = 140.0f;
    float attackExitDist = 190.0f;
    float nearDist = 45.0f;   // too close => disengage / brake
    float gunRange = 160.0f;
    float missileRange = 240.0f;

    // Angles (degrees)
    float aimConeGunDeg = 8.0f;
    float aimConeMissileDeg = 6.0f;

    // State time hysteresis
    float minStateTime = 1.0f;       // seconds to stay in a state before switching again
    float attackMaxTime = 3.0f;      // after this, disengage briefly to avoid tail-sticking
    float disengageTime = 1.2f;      // time to run away/return

    // Map boundary hysteresis
    float boundaryEnterRatio = 0.95f; // if distCenter > R*ratio => force Disengage(Return)
    float boundaryExitRatio = 0.80f; // once distCenter < R*ratio => can leave Disengage(Return)

    // Input response / smoothing
    float inputResponse = 8.0f;     // higher = snappier, lower = smoother
    float rollGain = 2.2f;
    float pitchGain = 1.8f;

    // Throttle policy
    float chaseThrottleTarget01 = 0.95f;
    float attackThrottleTarget01 = 0.75f;
    float disengageThrottleTarget01 = 1.0f;

    // Missile usage policy
    int   missilesMinToUse = 1;      // only shoot if launcher has enough missiles (optional check if you expose count)
    float missileMinTimeBetweenShots = 1.2f; // AI-side limiter (launcher already has cooldown too)

public:
    void Start(SceneContext& ctx) override;
    void Update(SceneContext& ctx) override;

    State GetState() const { return m_state; }
    void  ForceState(State s) { m_state = s; m_stateTime = 0.0f; }

private:
    // Cached references (optional)
    FlightRigComponent* m_rig = nullptr;
    GunComponent* m_gun = nullptr;
    MissileLauncherComponent* m_missile = nullptr;

    State m_state = State::Chase;
    float m_stateTime = 0.0f;
    float m_attackTime = 0.0f;
    float m_disengageTimeLeft = 0.0f;

    FlightInput m_smoothed{}; // smoothed output input

    float m_missileAiCooldown = 0.0f;

public:
    // Ground boundary (min altitude)
    float groundY = -150.0f;          // 바닥 기준 높이
    float groundSoftMargin = 12.0f; // 이 높이 아래로 내려가면 서서히 끌어올림 시작
    float groundHardMargin = 4.0f;  // 이 높이 아래면 강제 회피(거의 무조건 상승)
    float groundRecoverHeight = 35.0f; // 회피 시 목표 y = groundY + 이 값

    // 얼마나 강하게 끌어올릴지
    float groundPitchBoostSoft = 0.35f; // 소프트 구간 추가 피치
    float groundPitchBoostHard = 0.85f; // 하드 구간 추가 피치

    // return point
private:
    DirectX::XMFLOAT3 m_returnPoint{ 0,0,0 };
    bool m_hasReturnPoint = false;
    DirectX::XMFLOAT3 PickRandomReturnPoint(const DirectX::XMFLOAT3& myPos) const;

public:
    float returnRadiusRatioMin = 0.15f; // mapRadius * 0.15
    float returnRadiusRatioMax = 0.55f; // mapRadius * 0.55
    bool  keepReturnAltitude = false;    // y 고정할지

private:
    // Helpers
    FlightInput ComputeSteeringToPoint(
        const DirectX::XMFLOAT3& myPos,
        const DirectX::XMFLOAT3& myFwd,
        const DirectX::XMFLOAT3& myUp,
        const DirectX::XMFLOAT3& targetPos,
        float throttleTarget01,
        bool airbrake) const;

    static float Clamp(float v, float lo, float hi);
    static float Clamp01(float v);
    static float DegToRad(float deg);
    static float ExpSmoothT(float rate, float dt);

    static DirectX::XMFLOAT3 Sub(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b);
    static float Dot3(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b);
    static DirectX::XMFLOAT3 Cross3(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b);
    static DirectX::XMFLOAT3 Normalize3(const DirectX::XMFLOAT3& v, const DirectX::XMFLOAT3& fallback);
    static float Length3(const DirectX::XMFLOAT3& v);
};
