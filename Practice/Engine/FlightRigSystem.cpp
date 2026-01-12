#include "FlightRigSystem.h"

#include <cmath>

using namespace DirectX;

static float Clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

static XMVECTOR Load3(const XMFLOAT3& v) { return XMLoadFloat3(&v); }
static void Store3(XMFLOAT3& out, FXMVECTOR v) { XMStoreFloat3(&out, v); }

static XMVECTOR SafeNormalize(FXMVECTOR v, FXMVECTOR fallback)
{
    XMVECTOR len = XMVector3Length(v);
    if (XMVectorGetX(len) < 1e-6f) return fallback;
    return XMVector3Normalize(v);
}

static float ExpSmoothingT(float rate, float dt)
{
    return 1.0f - expf(-rate * dt);
}

static float Clamp(float v, float lo, float hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

// Rotate vector "from" toward "to" by at most maxAngle (radians).
// Both vectors are expected to be unit (we normalize defensively).
static XMVECTOR RotateTowards(FXMVECTOR from, FXMVECTOR to, float maxAngle)
{
    XMVECTOR a = SafeNormalize(from, XMVectorSet(0,0,1,0));
    XMVECTOR b = SafeNormalize(to,   XMVectorSet(0,0,1,0));

    float dot = XMVectorGetX(XMVector3Dot(a, b));
    dot = Clamp(dot, -1.0f, 1.0f);
    float ang = acosf(dot);
    if (ang < 1e-6f) return b;
    float step = (maxAngle < ang) ? maxAngle : ang;

    XMVECTOR axis = XMVector3Cross(a, b);
    axis = SafeNormalize(axis, XMVectorSet(0,1,0,0));
    XMVECTOR dq = XMQuaternionRotationAxis(axis, step);
    XMVECTOR out = XMVector3Rotate(a, dq);
    return SafeNormalize(out, b);
}

// Engine uses row-vector convention, so basis vectors go in ROWS: Right, Up, Forward.
static XMVECTOR QuaternionFromBasis(FXMVECTOR right, FXMVECTOR up, FXMVECTOR fwd)
{
    XMMATRIX R;
    R.r[0] = XMVectorSelect(XMVectorSet(0,0,0,1), right, XMVectorSelectControl(1,1,1,0));
    R.r[1] = XMVectorSelect(XMVectorSet(0,0,0,1), up,    XMVectorSelectControl(1,1,1,0));
    R.r[2] = XMVectorSelect(XMVectorSet(0,0,0,1), fwd,   XMVectorSelectControl(1,1,1,0));
    R.r[3] = XMVectorSet(0,0,0,1);
    return XMQuaternionNormalize(XMQuaternionRotationMatrix(R));
}

void FlightRigSystem::Step(TransformComponent& tr, FlightRigComponent& rig, const FlightInput& in, float dt)
{
    if (dt <= 0.0f) return;

    // 0) throttle
    rig.throttle01 = Clamp01(rig.throttle01 + in.throttleDelta * dt);

    // 1) steering effectiveness (optional low-speed reduction; never fully disables)
    XMVECTOR vel = Load3(rig.velocity);
    float speed = XMVectorGetX(XMVector3Length(vel));
    float turnScale = 1.0f;
    if (rig.minTurnSpeed > 0.0f)
    {
        float s = speed / rig.minTurnSpeed;
        if (s < 0.25f) s = 0.25f;
        if (s > 1.0f)  s = 1.0f;
        turnScale = s;
    }

    // 2) Persistent basis (forward + up) advanced by LOCAL-axis rotations.
    //    This removes the "pole" singularity caused by tying yaw to worldUp.
    XMVECTOR fwd = SafeNormalize(Load3(rig.forwardDir), XMVectorSet(0,0,1,0));
    XMVECTOR up  = SafeNormalize(Load3(rig.upDir),      XMVectorSet(0,1,0,0));

    // Orthonormalize up against forward (Gram-Schmidt)
    up = XMVectorSubtract(up, XMVectorScale(fwd, XMVectorGetX(XMVector3Dot(up, fwd))));
    up = SafeNormalize(up, XMVectorSet(0,1,0,0));

    // LH convention in this engine: Right = Up x Forward
    XMVECTOR right = XMVector3Cross(up, fwd);
    right = SafeNormalize(right, XMVectorSet(1,0,0,0));
    up = XMVector3Cross(fwd, right);
    up = SafeNormalize(up, XMVectorSet(0,1,0,0));

    // 3) Inputs (yaw is subtle; roll-to-yaw coupling gives banked-turn feel)
    float yawInput   = in.yaw + (in.roll * rig.autoYawFromRoll);
    float pitchInput = in.pitch;
    float rollInput  = in.roll;

    const float dyaw   = (yawInput   * rig.turnRateYaw   * turnScale) * dt;
    const float dpitch = (pitchInput * rig.turnRatePitch * turnScale) * dt;
    const float droll  = (rollInput  * rig.turnRateRoll  * turnScale) * dt;

    // 3-1) Yaw about LOCAL up
    if (fabsf(dyaw) > 1e-7f)
    {
        XMVECTOR dq = XMQuaternionRotationAxis(up, dyaw);
        fwd   = XMVector3Rotate(fwd, dq);
        right = XMVector3Rotate(right, dq);
        fwd   = XMVector3Normalize(fwd);
        right = XMVector3Normalize(right);
    }

    // 3-2) Pitch about LOCAL right
    if (fabsf(dpitch) > 1e-7f)
    {
        XMVECTOR dq = XMQuaternionRotationAxis(right, dpitch);
        fwd = XMVector3Rotate(fwd, dq);
        up  = XMVector3Rotate(up, dq);
        fwd = XMVector3Normalize(fwd);
        up  = XMVector3Normalize(up);
    }

    // 3-3) Roll about LOCAL forward
    if (fabsf(droll) > 1e-7f)
    {
        XMVECTOR dq = XMQuaternionRotationAxis(fwd, droll);
        up    = XMVector3Rotate(up, dq);
        right = XMVector3Rotate(right, dq);
        up    = XMVector3Normalize(up);
        right = XMVector3Normalize(right);

        rig.rollAngle += droll;
        if (rig.rollAngle > XM_PI)  rig.rollAngle -= XM_2PI;
        if (rig.rollAngle < -XM_PI) rig.rollAngle += XM_2PI;
    }

    // 4) Final re-orthonormalization (prevents drift)
    right = XMVector3Cross(up, fwd);
    right = SafeNormalize(right, XMVectorSet(1,0,0,0));
    up = XMVector3Cross(fwd, right);
    up = SafeNormalize(up, XMVectorSet(0,1,0,0));
    fwd = SafeNormalize(fwd, XMVectorSet(0,0,1,0));

    // 5) Write rotation from basis
    XMVECTOR q = QuaternionFromBasis(right, up, fwd);
    XMStoreFloat4(&tr.rotation, q);

    Store3(rig.forwardDir, fwd);
    Store3(rig.upDir, up);

    // 6) Speed model: throttle -> target speed, smooth
    float targetSpeed = rig.minSpeed + (rig.maxSpeed - rig.minSpeed) * rig.throttle01;
    if (in.airbrake)
        targetSpeed *= 0.5f;

    const float response = in.airbrake ? 8.0f : 4.0f;
    float t = ExpSmoothingT(response, dt);
    float newSpeed = speed + (targetSpeed - speed) * t;

    if (newSpeed < rig.minSpeed) newSpeed = rig.minSpeed;
    if (newSpeed > rig.maxSpeed) newSpeed = rig.maxSpeed;

    // 7) Option-A velocity direction: bend velocity toward NOSE direction at a limited rate.
    //    This makes the flight path draw smooth arcs even with aggressive attitude changes.
    XMVECTOR vDir;
    if (speed > 0.25f)
        vDir = XMVector3Normalize(vel);
    else
        vDir = fwd;

    // Bank-to-turn feel: more bank => slightly higher velocity bending rate.
    // bankAmount = |roll| normalized to [0,1] using the aircraftRight vs worldUp relation.
    XMVECTOR worldUp = XMVectorSet(0,1,0,0);
    float upDot = fabsf(XMVectorGetX(XMVector3Dot(up, worldUp))); // 1 when level, 0 when 90deg bank
    float bank01 = 1.0f - Clamp(upDot, 0.0f, 1.0f);

    // Speed-dependent steering: at very low speed, reduce turning so we don't jitter.
    float speed01 = 1.0f;
    if (rig.minTurnSpeed > 0.0f)
    {
        speed01 = Clamp(speed / rig.minTurnSpeed, 0.0f, 1.0f);
    }

    float velRate = rig.velTurnRateMin + (rig.velTurnRate - rig.velTurnRateMin) * speed01;
    velRate += rig.velTurnRateByBank * bank01;

    float maxAng = velRate * dt;
    vDir = RotateTowards(vDir, fwd, maxAng);

    // Optional: damp sideways (sideslip) relative to nose (rate in 1/s)
    if (rig.sideslipDampRate > 0.0f)
    {
        float tt = ExpSmoothingT(rig.sideslipDampRate, dt);
        XMVECTOR blended = XMVectorAdd(XMVectorScale(vDir, 1.0f - tt), XMVectorScale(fwd, tt));
        vDir = SafeNormalize(blended, fwd);
    }

    XMVECTOR newVel = XMVectorScale(vDir, newSpeed);
    Store3(rig.velocity, newVel);

    // 8) Integrate position using VELOCITY (not forced nose direction)
    XMVECTOR pos = XMLoadFloat3(&tr.position);
    pos = XMVectorAdd(pos, XMVectorScale(newVel, dt));
    XMStoreFloat3(&tr.position, pos);

    tr.dirty = true;
}
