#include "EnemyAIComponent.h"
#define NOMINMAX
#include <cmath>
#include "SceneContext.h"
#include "World.h"
#include "TransformComponent.h"
#include "GunComponent.h"
#include "MissileLauncherComponent.h"
#include "MyRandom.h"

using namespace DirectX;

static XMFLOAT3 Add3(const XMFLOAT3& a, const XMFLOAT3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
static XMFLOAT3 Mul3(const XMFLOAT3& v, float s) { return { v.x * s, v.y * s, v.z * s }; }

float EnemyAIComponent::Clamp(float v, float lo, float hi) { return (v < lo) ? lo : (v > hi) ? hi : v; }
float EnemyAIComponent::Clamp01(float v) { return Clamp(v, 0.0f, 1.0f); }
float EnemyAIComponent::DegToRad(float deg) { return deg * (XM_PI / 180.0f); }
float EnemyAIComponent::ExpSmoothT(float rate, float dt) { return 1.0f - std::exp(-rate * dt); }

XMFLOAT3 EnemyAIComponent::Sub(const XMFLOAT3& a, const XMFLOAT3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
float EnemyAIComponent::Dot3(const XMFLOAT3& a, const XMFLOAT3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
XMFLOAT3 EnemyAIComponent::Cross3(const XMFLOAT3& a, const XMFLOAT3& b)
{
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
float EnemyAIComponent::Length3(const XMFLOAT3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

XMFLOAT3 EnemyAIComponent::Normalize3(const XMFLOAT3& v, const XMFLOAT3& fallback)
{
    float len = Length3(v);
    if (len < 1e-6f) return fallback;
    float inv = 1.0f / len;
    return { v.x * inv, v.y * inv, v.z * inv };
}

void EnemyAIComponent::Start(SceneContext& ctx)
{
    m_state = State::Chase;
    m_stateTime = 0.0f;
    m_attackTime = 0.0f;
    m_disengageTimeLeft = 0.0f;
    m_missileAiCooldown = 0.0f;
    m_smoothed = {};
}

DirectX::XMFLOAT3 EnemyAIComponent::PickRandomReturnPoint(const DirectX::XMFLOAT3& myPos) const
{
    // 1) 원판(수평) 안에서 랜덤 방향
    float ang = RandRange(0.0f, DirectX::XM_2PI);

    // 2) 반경은 [min,max]로. 균일 분포를 원하면 sqrt를 써야 함.
    float rMin = mapRadius * returnRadiusRatioMin;
    float rMax = mapRadius * returnRadiusRatioMax;

    float u = RandRange(0.0f, 1.0f);
    float r = std::sqrt((1.0f - u) * (rMin * rMin) + u * (rMax * rMax)); // 면적 균일

    XMFLOAT3 p = mapCenter;
    p.x += std::cos(ang) * r;
    p.z += std::sin(ang) * r;

    if (keepReturnAltitude)
        p.y = myPos.y; // 고도 유지
    p.y = std::max(p.y, groundY + groundSoftMargin);

    return p;
}

FlightInput EnemyAIComponent::ComputeSteeringToPoint(
    const XMFLOAT3& myPos,
    const XMFLOAT3& myFwd,
    const XMFLOAT3& myUp,
    const XMFLOAT3& targetPos,
    float throttleTarget01,
    bool airbrake) const
{
    FlightInput out{};
    out.airbrake = airbrake;

    // Build right = up x forward (LH in your engine)
    XMFLOAT3 fwd = Normalize3(myFwd, { 0,0,1 });
    XMFLOAT3 up = Normalize3(myUp, { 0,1,0 });
    XMFLOAT3 right = Normalize3(Cross3(up, fwd), { 1,0,0 });

    // Vector to target in world
    XMFLOAT3 toW = Normalize3(Sub(targetPos, myPos), fwd);

    // Project into local axes
    float x = Dot3(toW, right); // + right
    float y = Dot3(toW, up);    // + up
    float z = Dot3(toW, fwd);   // + forward

    // Roll: bank toward target horizontally
    // If target is to the right (x>0) => roll right (+)
    float roll = Clamp(x * rollGain, -1.0f, +1.0f);

    // Pitch: pull nose toward target vertically
    // NOTE: Sign might need flip depending on your control convention.
    // If y>0 (target above), we need nose up => pitch input should produce up.
    float pitch = Clamp(-y * pitchGain, -1.0f, +1.0f);

    // If target is behind (z < 0), prefer a harder turn: roll strong, and pitch a bit to avoid flat spin feel
    if (z < 0.0f)
    {
        roll = Clamp(roll * 1.25f, -1.0f, +1.0f);
        pitch = Clamp(pitch * 0.75f, -1.0f, +1.0f);
    }

    out.roll = roll;
    out.pitch = pitch;

    // Yaw: mostly unnecessary because autoYawFromRoll already exists in rig.
    // Keep very small yaw assist if desired:
    out.yaw = Clamp(x * 0.15f, -1.0f, +1.0f);

    // Throttle delta: push throttle01 toward a target [0..1]
    // throttleDelta integrates over dt in FlightRigSystem, so we output [-1..+1].
    // We'll compute "desired change direction only", actual smoothing happens outside.
    // (If you want proportional control, scale by (throttleTarget - rig.throttle01) in Update().)
    out.throttleDelta = 0.0f; // decided in Update() where we know current throttle.

    return out;
}

void EnemyAIComponent::Update(SceneContext& ctx)
{
    if (!ctx.world.HasTransform(Entity())) return;
    if (!ctx.world.HasTransform(target)) return;

    auto& myTr = ctx.world.GetTransform(Entity());
    auto& tgTr = ctx.world.GetTransform(target);

    const XMFLOAT3 myPos = myTr.position;
    const XMFLOAT3 tgPos = tgTr.position;

    auto* rig = ctx.world.GetScriptAs<FlightRigComponent>(Entity());

    const XMFLOAT3 myFwd = rig->forwardDir;
    const XMFLOAT3 myUp = rig->upDir;

    // Timers
    m_stateTime += ctx.dt;
    if (m_state == State::Attack) m_attackTime += ctx.dt;
    if (m_disengageTimeLeft > 0.0f) m_disengageTimeLeft -= ctx.dt;
    if (m_missileAiCooldown > 0.0f) m_missileAiCooldown -= ctx.dt;
    if (m_reengageCooldown > 0.0f) m_reengageCooldown -= ctx.dt;
    if (m_attackGateTime > 0.0f && m_state != State::Chase) m_attackGateTime = 0.0f;

    // Distances
    XMFLOAT3 toTargetV = Sub(tgPos, myPos);
    float dTarget = Length3(toTargetV);

    XMFLOAT3 toCenterV = Sub(mapCenter, myPos);
    float dCenter = Length3(toCenterV);

    // Heading / aim
    XMFLOAT3 toTargetDir = Normalize3(toTargetV, myFwd);
    XMFLOAT3 fwdN = Normalize3(myFwd, { 0,0,1 });
    float dotFT = Clamp(Dot3(fwdN, toTargetDir), -1.0f, +1.0f);
    float aimAngle = std::acos(dotFT); // radians

    const float aimConeGun = DegToRad(aimConeGunDeg);
    const float aimConeMissile = DegToRad(aimConeMissileDeg);

    // 0) Map boundary forces Disengage(Return)
    bool outsideSoft = (dCenter > mapRadius * boundaryEnterRatio);
    bool insideSafe = (dCenter < mapRadius * boundaryExitRatio);

    if (outsideSoft && m_state != State::Disengage)
    {
        m_state = State::Disengage;
        m_stateTime = 0.0f;
        m_disengageTimeLeft = disengageTime;

        m_returnPoint = PickRandomReturnPoint(myPos);
        m_hasReturnPoint = true;
    }

    // 1) State transitions (with minStateTime)
    if (m_stateTime >= minStateTime)
    {
        switch (m_state)
        {
        case State::Chase:
        {
            if (!outsideSoft)
            {
                // 1) "진입 조건"을 조금 더 엄격하게 + 일정 시간 유지되면 Attack으로
                bool gateOk =
                    (m_reengageCooldown <= 0.0f) &&
                    (dTarget < attackEnterDist) &&
                    (dotFT > attackEnterDot);

                if (gateOk) m_attackGateTime += ctx.dt;
                else        m_attackGateTime = std::max(0.0f, m_attackGateTime - ctx.dt * 2.0f);

                if (m_attackGateTime >= attackGateRequiredTime)
                {
                    m_attackGateTime = 0.0f;

                    m_state = State::Attack;
                    m_stateTime = 0.0f;
                    m_attackTime = 0.0f;
                }
            }
        } break;

        case State::Attack:
        {
            bool tooClose = (dTarget < nearDist);
            bool tooFar = (dTarget > attackExitDist);
            bool timeUp = (m_attackTime > attackMaxTime);

            if (outsideSoft || tooClose || tooFar || timeUp)
            {
                m_state = State::Disengage;
                m_stateTime = 0.0f;
                m_disengageTimeLeft = disengageTime;

                m_returnPoint = PickRandomReturnPoint(myPos);
                m_hasReturnPoint = true;

                m_reengageCooldown = reengageCooldownTime;
            }
        } break;

        case State::Disengage:
        {
            // If this disengage was due to boundary, prefer returning inside safe zone.
            // Otherwise, just wait a bit, then resume chase.
            bool done = (m_disengageTimeLeft <= 0.0f);
            if (outsideSoft)
            {
                // still outside => keep returning
            }
            else if (insideSafe && done)
            {
                m_state = State::Chase;
                m_stateTime = 0.0f;
            }
            else if (!outsideSoft && done)
            {
                m_state = State::Chase;
                m_stateTime = 0.0f;
                m_hasReturnPoint = false;
            }
        } break;
        }
    }

    // 2) Compute desired input by state
    FlightInput desired{};
    float throttleTarget01 = chaseThrottleTarget01;
    bool airbrake = false;

    XMFLOAT3 steerPoint = tgPos;

    if (m_state == State::Chase)
    {
        throttleTarget01 = chaseThrottleTarget01;
        steerPoint = tgPos;
        airbrake = false;
        desired = ComputeSteeringToPoint(myPos, myFwd, myUp, steerPoint, throttleTarget01, airbrake);
    }
    else if (m_state == State::Attack)
    {
        throttleTarget01 = attackThrottleTarget01;
        steerPoint = tgPos;

        // If too close, brake hard to avoid overshoot
        airbrake = (dTarget < nearDist * 1.15f);
        desired = ComputeSteeringToPoint(myPos, myFwd, myUp, steerPoint, throttleTarget01, airbrake);
    }
    else // Disengage
    {
        throttleTarget01 = disengageThrottleTarget01;

        // Return-to-map:
        if (!m_hasReturnPoint)
        {
            m_returnPoint = PickRandomReturnPoint(myPos);
            m_hasReturnPoint = true;
        }
        steerPoint = m_returnPoint;

        airbrake = false;
        desired = ComputeSteeringToPoint(myPos, myFwd, myUp, steerPoint, throttleTarget01, airbrake);

        // When returning, lean into strong roll a bit less (avoid spiraling near center)
        desired.roll *= 0.85f;
        desired.pitch *= 0.9f;
    }

    // 3) Decide throttleDelta based on current rig throttle01
    // Output in [-1..+1], FlightRigSystem integrates: rig.throttle01 += throttleDelta * dt
    float th = rig->throttle01;
    float thErr = throttleTarget01 - th;
    desired.throttleDelta = Clamp(thErr * 2.0f, -1.0f, +1.0f); // proportional control

    // --- Ground boundary override (after desired computed, before smoothing) ---
    float ySoft = groundY + groundSoftMargin;
    float yHard = groundY + groundHardMargin;

    if (myPos.y < ySoft)
    {
        // 회피 강도: yHard 이하에서는 강, 그 위는 완만
        float boost = groundPitchBoostSoft;
        if (myPos.y < yHard) boost = groundPitchBoostHard;

        // 1) 목표 고도를 위로 강제 (가장 안정적)
        //    steerPoint의 y를 끌어올려서 기존 조향 로직을 재사용
        //    (ComputeSteeringToPoint 다시 호출하는 게 깔끔)
        XMFLOAT3 lifted = steerPoint;
        lifted.y = groundY + groundRecoverHeight;

        // airbrake는 끄는 게 보통 안전(상승 회복이 우선)
        airbrake = false;

        FlightInput liftedDesired = ComputeSteeringToPoint(myPos, myFwd, myUp, lifted, throttleTarget01, airbrake);

        // 2) 추가 피치 가중(“무조건 상승” 느낌)
        liftedDesired.pitch = Clamp(liftedDesired.pitch + boost, -1.0f, +1.0f);

        // 3) 너무 낮으면 롤도 조금 죽여서 스파이럴로 파고드는 걸 방지
        if (myPos.y < yHard)
        {
            liftedDesired.roll *= 0.6f;
            liftedDesired.yaw *= 0.4f;
            // 스로틀은 유지/증가(회복 우선)
            liftedDesired.throttleDelta = Clamp((disengageThrottleTarget01 - rig->throttle01) * 2.5f, -1.0f, +1.0f);
        }

        desired = liftedDesired;
    }


    // 4) Smooth inputs (important for stability)
    float t = ExpSmoothT(inputResponse, ctx.dt);

    auto smoothScalar = [&](float prev, float cur)
        {
            return prev + (cur - prev) * t;
        };

    m_smoothed.pitch = smoothScalar(m_smoothed.pitch, desired.pitch);
    m_smoothed.roll = smoothScalar(m_smoothed.roll, desired.roll);
    m_smoothed.yaw = smoothScalar(m_smoothed.yaw, desired.yaw);

    m_smoothed.throttleDelta = smoothScalar(m_smoothed.throttleDelta, desired.throttleDelta);
    m_smoothed.airbrake = desired.airbrake; // bool은 즉시 반영

    // 5) Feed rig input
    rig->SetInput(m_smoothed);

    // 6) Fire weapons in Attack state when aimed
    if (m_state == State::Attack)
    {
        // Gun
        auto* gun = ctx.world.GetScriptAs<GunComponent>(Entity());
        bool gunOk = (gun != nullptr)
            && (dTarget <= gunRange)
            && (aimAngle <= aimConeGun)
            && (dotFT > 0.0f);

        if (gunOk)
        {
            // GunComponent has internal cooldown; calling each frame is ok if Fire() checks it.
            gun->Fire(ctx);
        }

        // Missile (optional)
        auto* missile = ctx.world.GetScriptAs<MissileLauncherComponent>(Entity());
        bool missileOk = (missile != nullptr)
            && (dTarget <= missileRange)
            && (aimAngle <= aimConeMissile)
            && (dotFT > 0.0f)
            && (m_missileAiCooldown <= 0.0f);

        if (missileOk)
        {
            // MissileLauncherComponent also has cooldown & count;
            // We additionally gate with AI-side cooldown to prevent spamming.
            missile->Fire(ctx);
            m_missileAiCooldown = missileMinTimeBetweenShots;
        }
    }
}
