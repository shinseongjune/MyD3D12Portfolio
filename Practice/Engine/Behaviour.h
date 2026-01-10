#pragma once
#include "EntityId.h"

struct SceneContext;

class Behaviour
{
public:
    virtual ~Behaviour() = default;

    EntityId Entity() const { return m_entity; }

    // Unity-like callbacks
    virtual void Awake(SceneContext& ctx) {}
    virtual void Start(SceneContext& ctx) {}
    virtual void Update(SceneContext& ctx) {}
    virtual void FixedUpdate(SceneContext& ctx) {}
    virtual void OnDestroy() {}

private:
    friend class World;
    void _SetEntity(EntityId e) { m_entity = e; }

    EntityId m_entity = EntityId::Invalid();
};
