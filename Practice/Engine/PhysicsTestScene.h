#pragma once
#include <DirectXMath.h>
#include <vector>
#include "Scene.h"
#include "EntityId.h"
#include "MeshHandle.h"

class PhysicsTestScene final : public Scene
{
public:
    void OnLoad(SceneContext& ctx) override;
    void OnUnload(SceneContext& ctx) override;
    void OnUpdate(SceneContext& ctx) override;

private:
    // 테스트용으로 메쉬 핸들을 멤버로 들고 있으면, 공 여러 개 찍어낼 때 편함
    MeshHandle m_boxMesh{};
    MeshHandle m_sphereMesh{};

    EntityId m_ground = EntityId::Invalid();
    EntityId m_ball = EntityId::Invalid();

    std::vector<EntityId> m_balls;
    bool m_gravityOn = true;

    void ResetWorld(SceneContext& ctx);

private:
    EntityId CreateCameraIfMissing(SceneContext& ctx);
    EntityId CreateGround(SceneContext& ctx);
    EntityId CreateBall(SceneContext& ctx, const DirectX::XMFLOAT3& pos);

};
