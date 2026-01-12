#pragma once

#include "Behaviour.h"
#include "CameraRig.h"

// Wraps CameraRig as a Behaviour so it updates inside ScriptSystem (after movement scripts).
// Input is still fed only through PlayScene::ExecuteCommand.
class CameraRigComponent : public Behaviour
{
public:
    void SetCamera(EntityId cam) { m_rig.SetCamera(cam); }
    void SetTarget(EntityId target) { m_rig.SetTarget(target); }
    void SetFollowEnabled(bool on) { m_rig.SetFollowEnabled(on); }

    void OnLook(float dx, float dy) { m_rig.OnLook(dx, dy); }
    void Impulse(float duration, float strength) { m_rig.Impulse(duration, strength); }

    int UpdateOrder() const { return 100; }
    void Update(SceneContext& ctx) override;

private:
    CameraRig m_rig;
};
