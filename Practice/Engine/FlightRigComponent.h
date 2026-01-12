#pragma once
#include <DirectXMath.h>
#include "Behaviour.h"

// Input for the flight rig. Values are expected to be in [-1, +1] range.
struct FlightInput
{
    float pitch = 0.0f;        // nose up/down
    float roll  = 0.0f;        // bank
    float yaw   = 0.0f;        // rudder

    float throttleDelta = 0.0f; // -1..+1 (changes throttle01 over time)
    bool  airbrake = false;
};

// A Unity-like script component that drives an entity like an arcade aircraft.
// This is intentionally Scene-driven: input should be fed from PlayScene::ExecuteCommand.
class FlightRigComponent : public Behaviour
{
public:
    // State
    DirectX::XMFLOAT3 velocity{ 0,0,0 };
    // Arcade orientation state (authoritative).
    // IMPORTANT: forwardDir + upDir are persisted and advanced by local-axis rotations.
    // This avoids the "pole" singularity you get when yaw is tied to worldUp.
    DirectX::XMFLOAT3 forwardDir{ 0,0,1 }; // unit direction in world space
    DirectX::XMFLOAT3 upDir{ 0,1,0 };      // unit up direction in world space (orthogonalized each step)
    float rollAngle = 0.0f;               // optional debug/telemetry (radians). Not used to rebuild basis.
    float throttle01 = 0.0f;     // 0..1

    // Startup
    float initialSpeed = 10.0f;   // small starting airspeed so the plane doesn't feel "dead" at spawn
    float initialThrottle01 = 0.15f;

    // Tuning
    float minSpeed = 0.0f;
    float maxSpeed = 250.0f;

    float thrustAccel = 25.0f;   // forward accel (m/s^2-ish)
    float drag = 0.02f;          // proportional to speed
    float airbrakeDragMul = 4.0f;

    float turnRatePitch = DirectX::XMConvertToRadians(90.0f);  // rad/s
    float turnRateRoll  = DirectX::XMConvertToRadians(140.0f);
    // Yaw should be subtle (rudder), most turning comes from roll.
    float turnRateYaw   = DirectX::XMConvertToRadians(8.0f);

    float autoYawFromRoll = 0.65f; // roll -> yaw coupling (banked turn feel)

    float sideslipDamp = 0.0f; // reduces velocity component not aligned with nose

    float velAlignRate = 0.0f; // (deprecated) kept for compatibility
    float lowSpeedAlignBoost = 0.0f;

    float gravity = 9.8f; // if your physics system already applies gravity, set this to 0.
    float liftStrength = 0.0012f;  // scales speed^2
    float stallSpeed = 5.0f;
    float stallRecoverBand = 15.0f;

    float minTurnSpeed = 20.0f; // below this, steering effectiveness reduces

    // ===== Option-A (Ace-Combat-ish) velocity model =====
    // We keep an independent velocity vector and only steer its DIRECTION toward the nose
    // at a limited angular rate. This produces the "nice arc" feel even with aggressive
    // pitch/roll inputs, and avoids singularities caused by world-up yaw hacks.
    float velTurnRate = DirectX::XMConvertToRadians(140.0f); // rad/s (how fast velocity direction can bend)
    float velTurnRateMin = DirectX::XMConvertToRadians(45.0f); // rad/s at very low speeds
    float velTurnRateByBank = DirectX::XMConvertToRadians(80.0f); // extra rad/s when banked (more "bank-to-turn" feel)

    // Optional: damp sideways component relative to nose (0 = off). 0.0~6.0 is typical.
    float sideslipDampRate = 0.0f;

    // Camera shake options (kept here for future camera-rig usage)
    bool  enableCameraShake = true;
    float shakeAmpAtMax = 0.25f;
    float shakeFreq = 10.0f;

public:
    // Called by the scene (via ExecuteCommand) each frame.
    void SetInput(const FlightInput& in) { m_input = in; }

    // Clear input after consumption (optional convenience).
    void ClearInput() { m_input = {}; }

    int UpdateOrder() const { return 0; }
    // Behaviour
    void Start(SceneContext& ctx) override;
    void Update(SceneContext& ctx) override;

private:
    FlightInput m_input{};
};
