#include "CameraRig.h"
#include "World.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

float CameraRig::Clamp(float v, float lo, float hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

float CameraRig::Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

XMFLOAT3 CameraRig::Lerp3(const XMFLOAT3& a, const XMFLOAT3& b, float t)
{
    return { Lerp(a.x,b.x,t), Lerp(a.y,b.y,t), Lerp(a.z,b.z,t) };
}

float CameraRig::Smooth01(float k, float dt)
{
    return 1.0f - std::exp(-k * dt);
}

void CameraRig::OnLook(float dx, float dy)
{
    m_yaw += dx * m_sensitivity;
    m_pitch += dy * m_sensitivity;

	constexpr float yawLimit = XMConvertToRadians(180.f);
    constexpr float pitchLimit = XMConvertToRadians(55.f);
	m_yaw = Clamp(m_yaw, -yawLimit, +yawLimit);
    m_pitch = Clamp(m_pitch, -pitchLimit, +pitchLimit);

    m_hadLookInputThisFrame = true;
    m_remainingTime = m_returnDelay;
}

void CameraRig::Impulse(float duration, float strength)
{
    m_shakeTimeLeft = std::max(m_shakeTimeLeft, duration);
    m_shakeStrength = std::max(m_shakeStrength, strength);

    std::uniform_real_distribution<float> dist(-1.f, 1.f);
    m_shakeTarget = { dist(m_rng) * m_shakeStrength, dist(m_rng) * m_shakeStrength, 0.f };
}

void CameraRig::Update(World& world, float dt)
{
    if (!m_followEnabled) return;
    if (!world.IsAlive(m_cam) || !world.IsAlive(m_target)) return;

    // 입력 없으면 점차 정면 복귀
    if (!m_hadLookInputThisFrame)
    {
		m_remainingTime -= dt;
        if (m_remainingTime < 0)
        {
            m_remainingTime = 0.f;
            float t = Smooth01(m_returnSpeed, dt);
            m_yaw = Lerp(m_yaw, 0.f, t);
            m_pitch = Lerp(m_pitch, 0.f, t);
		}
    }
    m_hadLookInputThisFrame = false; // 다음 프레임을 위해 리셋

    // shake update
    if (m_shakeTimeLeft > 0.f)
    {
        m_shakeTimeLeft -= dt;

        XMFLOAT3 diff{ m_shakeTarget.x - m_shakeOffset.x, m_shakeTarget.y - m_shakeOffset.y, 0.f };
        float len = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        if (len < m_shakeEps)
        {
            std::uniform_real_distribution<float> dist(-1.f, 1.f);
            m_shakeTarget = { dist(m_rng) * m_shakeStrength, dist(m_rng) * m_shakeStrength, 0.f };
        }
    }
    else
    {
        m_shakeTarget = { 0,0,0 };
        m_shakeStrength = 0.f;
    }

    {
        float t = Smooth01(m_shakeLerp, dt);
        m_shakeOffset = Lerp3(m_shakeOffset, m_shakeTarget, t);
    }

    // 타겟 추적 + orbit
    XMFLOAT3 targetPos = world.GetWorldPosition(m_target);
    XMFLOAT3 lookAtPos = { targetPos.x + m_lookAtOffset.x, targetPos.y + m_lookAtOffset.y, targetPos.z + m_lookAtOffset.z };

    XMVECTOR base = XMLoadFloat3(&m_baseOffset);
    XMMATRIX rot = XMMatrixRotationRollPitchYaw(m_pitch, m_yaw, 0.f);
    XMVECTOR offV = XMVector3TransformCoord(base, rot);

    XMFLOAT3 off;
    XMStoreFloat3(&off, offV);

    XMFLOAT3 camPos{
        targetPos.x + off.x + m_shakeOffset.x,
        targetPos.y + off.y + m_shakeOffset.y,
        targetPos.z + off.z + m_shakeOffset.z
    };

    world.SetLocalPosition(m_cam, camPos);

    // look-at 회전
    XMVECTOR camV = XMLoadFloat3(&camPos);
    XMVECTOR atV = XMLoadFloat3(&lookAtPos);
    XMVECTOR upV = XMVectorSet(0, 1, 0, 0);

    // 카메라가 보는 방향으로 뷰행렬 만들기
    XMMATRIX view = XMMatrixLookAtLH(camV, atV, upV);

    // view는 카메라 기준 행렬이니까, 카메라의 월드 회전은 inverse(view)의 rotation 부분
    XMMATRIX invView = XMMatrixInverse(nullptr, view);

    // 회전 추출 -> 쿼터니언
    XMVECTOR q = XMQuaternionRotationMatrix(invView);

    // World에 quat로 세팅
    XMFLOAT4 qf;
    XMStoreFloat4(&qf, q);
    world.SetLocalRotation(m_cam, qf);
}
