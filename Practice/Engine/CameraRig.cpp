#include "CameraRig.h"
#include "World.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

static float SmoothDamp1(float current, float target, float& currentVelocity, float smoothTime, float maxSpeed, float dt)
{
    smoothTime = (smoothTime < 1e-4f) ? 1e-4f : smoothTime;

    float omega = 2.0f / smoothTime;
    float x = omega * dt;
    float exp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

    float change = current - target;
    float originalTo = target;

    // clamp max speed
    float maxChange = maxSpeed * smoothTime;
    change = (change > maxChange) ? maxChange : (change < -maxChange ? -maxChange : change);
    target = current - change;

    float temp = (currentVelocity + omega * change) * dt;
    currentVelocity = (currentVelocity - omega * temp) * exp;

    float output = target + (change + temp) * exp;

    // prevent overshoot
    if ((originalTo - current > 0.0f) == (output > originalTo))
    {
        output = originalTo;
        currentVelocity = (output - originalTo) / dt;
    }
    return output;
}

static DirectX::XMFLOAT3 SmoothDamp3(DirectX::XMFLOAT3 cur, DirectX::XMFLOAT3 tgt,
    DirectX::XMFLOAT3& vel, float smoothTime, float maxSpeed, float dt)
{
    DirectX::XMFLOAT3 out;
    out.x = SmoothDamp1(cur.x, tgt.x, vel.x, smoothTime, maxSpeed, dt);
    out.y = SmoothDamp1(cur.y, tgt.y, vel.y, smoothTime, maxSpeed, dt);
    out.z = SmoothDamp1(cur.z, tgt.z, vel.z, smoothTime, maxSpeed, dt);
    return out;
}

void CameraRig::SetTarget(EntityId target)
{
    m_target = target;

    m_orbitYaw = 0.0f;
    m_orbitPitch = DirectX::XMConvertToRadians(-20.0f);

    m_remainingTime = 0.0f;
    m_hadLookInputThisFrame = false;

    m_hasPrevBasis = false;
    m_hasPrevTargetQ = false;

    // stop shake
    m_shakeTimeLeft = 0.0f;
    m_shakeDuration = 0.0f;
    m_shakeStrength = 0.0f;
}

void CameraRig::OnLook(float dx, float dy)
{
    m_orbitYaw += dx * m_orbitSensitivity;
    m_orbitPitch += dy * m_orbitSensitivity;

    m_orbitYaw = Clamp(m_orbitYaw, -m_orbitYawLimit, +m_orbitYawLimit);
    m_orbitPitch = Clamp(m_orbitPitch, -m_orbitPitchLimit, +m_orbitPitchLimit);

    m_hadLookInputThisFrame = true;
    m_remainingTime = m_returnDelay;
}

void CameraRig::Impulse(float duration, float strength)
{
    if (duration <= 0.0f || strength <= 0.0f) return;

    m_shakeTimeLeft = std::max(m_shakeTimeLeft, duration);
    m_shakeDuration = std::max(m_shakeDuration, duration);
    m_shakeStrength = std::max(m_shakeStrength, strength);

    std::uniform_real_distribution<float> phaseDist(0.f, DirectX::XM_2PI);
    std::uniform_real_distribution<float> freqDist(16.f, 28.f); // 약간 랜덤한 진동수
    m_shakePhaseX = phaseDist(m_rng);
    m_shakePhaseY = phaseDist(m_rng);
    m_shakeFreqX = freqDist(m_rng);
    m_shakeFreqY = freqDist(m_rng);
}

XMVECTOR CameraRig::BuildLookAtQuaternionStable(FXMVECTOR camPos, FXMVECTOR lookAtPos, FXMVECTOR preferredUp, FXMVECTOR fallbackRight)
{
    // Forward (camera -> target)
    XMVECTOR fwd = XMVectorSubtract(lookAtPos, camPos);
    float fwdLen = XMVectorGetX(XMVector3Length(fwd));
    if (fwdLen < 1e-6f)
        return XMQuaternionIdentity();
    fwd = XMVectorScale(fwd, 1.0f / fwdLen);

    // Gram-Schmidt: make up orthogonal to forward
    XMVECTOR upOrtho = XMVectorSubtract(
        preferredUp,
        XMVectorScale(fwd, XMVectorGetX(XMVector3Dot(preferredUp, fwd)))
    );
    float upLen = XMVectorGetX(XMVector3Length(upOrtho));

    XMVECTOR rightV, upFinal;

    if (upLen < 1e-4f)
    {
        rightV = m_hasPrevBasis ? XMLoadFloat3(&m_prevRight) : fallbackRight;

        rightV = XMVectorSubtract(
            rightV,
            XMVectorScale(fwd, XMVectorGetX(XMVector3Dot(rightV, fwd)))
        );

        float rLen = XMVectorGetX(XMVector3Length(rightV));
        if (rLen < 1e-4f)
        {
            rightV = XMVectorSet(1, 0, 0, 0);
            rightV = XMVectorSubtract(
                rightV,
                XMVectorScale(fwd, XMVectorGetX(XMVector3Dot(rightV, fwd)))
            );
        }

        rightV = SafeNormalize(rightV, XMVectorSet(1, 0, 0, 0));
        upFinal = SafeNormalize(XMVector3Cross(fwd, rightV), XMVectorSet(0, 1, 0, 0));

        if (m_hasPrevBasis)
        {
            XMVECTOR prevUp = XMLoadFloat3(&m_prevUp);
            if (XMVectorGetX(XMVector3Dot(upFinal, prevUp)) < 0.0f)
            {
                upFinal = XMVectorNegate(upFinal);
                rightV = XMVectorNegate(rightV);
            }
        }
    }
    else
    {
        upOrtho = XMVectorScale(upOrtho, 1.0f / upLen);

        // LH: right = up x forward
        rightV = XMVector3Cross(upOrtho, fwd);
        rightV = SafeNormalize(rightV, fallbackRight);

        upFinal = SafeNormalize(XMVector3Cross(fwd, rightV), upOrtho);

        if (m_hasPrevBasis)
        {
            XMVECTOR prevRight = XMLoadFloat3(&m_prevRight);
            if (XMVectorGetX(XMVector3Dot(rightV, prevRight)) < 0.0f)
            {
                rightV = XMVectorNegate(rightV);
                upFinal = XMVectorNegate(upFinal);
            }
        }
    }

    // Save basis for continuity
    {
        XMFLOAT3 rr, uu;
        XMStoreFloat3(&rr, rightV);
        XMStoreFloat3(&uu, upFinal);
        m_prevRight = rr;
        m_prevUp = uu;
        m_hasPrevBasis = true;
    }

    // Rotation matrix from basis (rows: right, up, forward)
    XMMATRIX R;
    R.r[0] = XMVectorSelect(XMVectorSet(0, 0, 0, 1), rightV, XMVectorSelectControl(1, 1, 1, 0));
    R.r[1] = XMVectorSelect(XMVectorSet(0, 0, 0, 1), upFinal, XMVectorSelectControl(1, 1, 1, 0));
    R.r[2] = XMVectorSelect(XMVectorSet(0, 0, 0, 1), fwd, XMVectorSelectControl(1, 1, 1, 0));
    R.r[3] = XMVectorSet(0, 0, 0, 1);

    return XMQuaternionNormalize(XMQuaternionRotationMatrix(R));
}

void CameraRig::Update(World& world, float dt)
{
    if (!m_followEnabled) return;
    if (!world.IsAlive(m_cam) || !world.IsAlive(m_target)) return;
    if (!world.HasTransform(m_target)) return;

    if (!m_hadLookInputThisFrame)
    {
        m_remainingTime -= dt;
        if (m_remainingTime <= 0.f)
        {
            m_remainingTime = 0.f;
            m_orbitYaw = m_orbitYaw + (0 - m_orbitYaw) * m_returnDelay * dt;
            m_orbitPitch = m_orbitPitch + (XMConvertToRadians(-20.0f) - m_orbitPitch) * m_returnDelay * dt;
        }
    }
    m_hadLookInputThisFrame = false;

    // 2) target world pos/rot 읽기
    XMFLOAT3 targetPos{};
    XMVECTOR qTarget = XMQuaternionIdentity();

    const auto& tt = world.GetTransform(m_target);
    if (!tt.parent.IsValid())
    {
        targetPos = tt.position;
        qTarget = XMLoadFloat4(&tt.rotation);
    }
    else
    {
        world.UpdateTransformNow(m_target);
        targetPos = world.GetWorldPosition(m_target);

        XMFLOAT4X4 wm = world.GetWorldMatrix(m_target);
        XMMATRIX W = XMLoadFloat4x4(&wm);
        XMVECTOR s, r, t;
        if (XMMatrixDecompose(&s, &r, &t, W))
            qTarget = r;
        else
            qTarget = XMQuaternionIdentity();
    }

    qTarget = XMQuaternionNormalize(qTarget);

    // qTarget continuity (q/-q flip 방지)
    if (m_hasPrevTargetQ)
    {
        XMVECTOR prev = XMLoadFloat4(&m_prevTargetQ);
        if (XMVectorGetX(XMVector4Dot(prev, qTarget)) < 0.0f)
            qTarget = XMVectorNegate(qTarget);
    }
    XMStoreFloat4(&m_prevTargetQ, qTarget);
    m_hasPrevTargetQ = true;

    const XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);

    // aircraft basis
    XMVECTOR aircraftUp = XMVector3Rotate(XMVectorSet(0, 1, 0, 0), qTarget);
    XMVECTOR aircraftRight = XMVector3Rotate(XMVectorSet(1, 0, 0, 0), qTarget);
    XMVECTOR aircraftFwd = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), qTarget);

    // orbitUp: roll follow (스무딩 없음)
    float rf = Clamp(m_rollFollow, 0.f, 1.f);
    XMVECTOR orbitUp = XMVectorAdd(XMVectorScale(worldUp, 1.0f - rf), XMVectorScale(aircraftUp, rf));
    orbitUp = SafeNormalize(orbitUp, worldUp);

    // 3) orbit dir 계산 (스무딩 없음, 올바른 pitch 축)
    m_orbitYaw = Clamp(m_orbitYaw, -m_orbitYawLimit, +m_orbitYawLimit);
    m_orbitPitch = Clamp(m_orbitPitch, -m_orbitPitchLimit, +m_orbitPitchLimit);

    XMVECTOR baseDir = XMVectorNegate(aircraftFwd);
    baseDir = SafeNormalize(baseDir, XMVectorSet(0, 0, -1, 0));

    // yaw
    XMVECTOR qYaw = XMQuaternionRotationAxis(orbitUp, m_orbitYaw);
    XMVECTOR yawedDir = XMVector3Rotate(baseDir, qYaw);
    yawedDir = SafeNormalize(yawedDir, baseDir);

    // orbitRight from orbitUp & yawedDir
    XMVECTOR orbitRight = XMVector3Cross(orbitUp, yawedDir);
    orbitRight = SafeNormalize(orbitRight, aircraftRight);

    // pitch
    XMVECTOR qPitch = XMQuaternionRotationAxis(orbitRight, m_orbitPitch);

    XMVECTOR dir = XMVector3Rotate(yawedDir, qPitch);
    dir = SafeNormalize(dir, yawedDir);

    // camera right/up (for shake axis)
    XMVECTOR camRight = orbitRight;
    XMVECTOR camUp = SafeNormalize(XMVector3Cross(dir, camRight), orbitUp);
    camRight = SafeNormalize(XMVector3Cross(camUp, dir), camRight);

    // 4) shake (프레임 스무딩 없는 sin/cos, 감쇠만)
    XMVECTOR shakeWorldV = XMVectorZero();
    if (m_shakeTimeLeft > 0.f)
    {
        m_shakeTimeLeft -= dt;
        float life = (m_shakeDuration > 1e-6f) ? std::max(0.0f, m_shakeTimeLeft / m_shakeDuration) : 0.0f;

        // 부드러운 감쇠(프레임노이즈 없음)
        float amp = m_shakeStrength * (life * life);

        m_shakePhaseX += m_shakeFreqX * dt;
        m_shakePhaseY += m_shakeFreqY * dt;

        float sx = std::sin(m_shakePhaseX) * amp;
        float sy = std::cos(m_shakePhaseY) * amp;

        shakeWorldV = XMVectorAdd(XMVectorScale(camRight, sx), XMVectorScale(camUp, sy));
    }
    else
    {
        m_shakeStrength = 0.f;
        m_shakeDuration = 0.f;
    }

    // 5) camera position = target + dir * radius + shake (즉시 세팅)
    XMVECTOR targetV = XMLoadFloat3(&targetPos);
    XMVECTOR camPosV = XMVectorAdd(targetV, XMVectorScale(dir, m_orbitRadius));
    camPosV = XMVectorAdd(camPosV, shakeWorldV);

    // 6) lookAt = target + aircraft-local offset
    XMVECTOR lookAtV = targetV;
    lookAtV = XMVectorAdd(lookAtV, XMVectorScale(aircraftRight, m_lookAtOffset.x));
    lookAtV = XMVectorAdd(lookAtV, XMVectorScale(aircraftUp, m_lookAtOffset.y));
    lookAtV = XMVectorAdd(lookAtV, XMVectorScale(aircraftFwd, m_lookAtOffset.z));

    // 6.5) smooth focus point (lookAt)
    XMFLOAT3 lookAtF;
    XMStoreFloat3(&lookAtF, lookAtV);

    if (!m_hasFocus)
    {
        m_focusPos = lookAtF;
        m_focusVel = { 0,0,0 };
        m_hasFocus = true;
    }
    else
    {
        m_focusPos = SmoothDamp3(m_focusPos, lookAtF, m_focusVel, m_focusSmoothTime, m_focusMaxSpeed, dt);
    }

    XMVECTOR focusV = XMLoadFloat3(&m_focusPos);

    // 7) rotation: stable look-at
    XMVECTOR qLook = BuildLookAtQuaternionStable(camPosV, lookAtV, orbitUp, camRight);
    qLook = XMQuaternionNormalize(qLook);

    // 8) 즉시 적용(스무딩 없음)
    XMFLOAT3 camPos;
    XMFLOAT4 camRot;
    XMStoreFloat3(&camPos, camPosV);
    XMStoreFloat4(&camRot, qLook);

    world.SetLocalPosition(m_cam, camPos);
    world.SetLocalRotation(m_cam, camRot);
}
