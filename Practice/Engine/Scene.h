#pragma once

// 아주 가벼운 Scene 인터페이스(엔티티 생성/정리만 담당)
// - Unity의 Scene처럼 "월드를 어떻게 채울지"를 캡슐화한다.
// - 시스템(Physics/Animation 등)은 아직 없으므로 우선은 OnLoad/OnUnload만 둔다.

class World;

class Scene
{
public:
    virtual ~Scene() = default;
    virtual void OnLoad(World& world) = 0;
    virtual void OnUnload(World& world) = 0;

    virtual void OnUpdate(World& world, float deltaTime) = 0;
};
