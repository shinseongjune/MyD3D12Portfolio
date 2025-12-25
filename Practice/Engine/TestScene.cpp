#include "TestScene.h"

#include "World.h"

using namespace DirectX;

void TestScene::OnLoad(World& world)
{
    m_spawned.clear();

    // Player
    {
        EntityId e = world.CreateEntity("Player");
        m_spawned.push_back(e);

        world.EnsureTransform(e);
        world.EnsureMesh(e);
        world.EnsureMaterial(e);

        world.GetMaterial(e).color = { 0.2f, 0.6f, 1.0f, 1.0f }; // 파랑
        world.SetLocalPosition(e, { 0.f, 0.f, 0.f });
    }

    // MouthSocket (Player의 자식)
    {
        EntityId player = world.FindByName("Player");
        EntityId mouth = world.CreateEntity("MouthSocket");
        m_spawned.push_back(mouth);

        world.EnsureTransform(mouth);
        world.EnsureMesh(mouth);
        world.EnsureMaterial(mouth);

        world.GetMaterial(mouth).color = { 1.0f, 1.0f, 0.2f, 1.0f }; // 노랑
        world.SetLocalPosition(mouth, { 0.f, 0.8f, 0.f });
        world.SetLocalScale(mouth, { 0.25f, 0.25f, 0.25f });

        world.SetParent(mouth, player);
    }

    // Camera (아직 Renderer에서 미사용이지만, "월드에 카메라가 있다"는 형태를 만들어둔다)
    {
        EntityId cam = world.CreateEntity("MainCamera");
        m_spawned.push_back(cam);

        world.EnsureTransform(cam);
        world.EnsureCamera(cam);

        world.SetLocalPosition(cam, { 0.f, 0.f, -6.f });
        world.GetCamera(cam).active = true;
    }
}

void TestScene::OnUnload(World& world)
{
    // 역순 파괴(부모-자식 관계가 있을 수 있어 안전)
    for (auto it = m_spawned.rbegin(); it != m_spawned.rend(); ++it)
        world.DestroyEntity(*it);
    m_spawned.clear();
}
