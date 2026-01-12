#include "CameraRigComponent.h"
#include "SceneContext.h"

void CameraRigComponent::Update(SceneContext& ctx)
{
    m_rig.Update(ctx.world, ctx.dt);
}
