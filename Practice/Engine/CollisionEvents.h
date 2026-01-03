#pragma once
#include "EntityId.h"
#include <cstdint>

enum class CollisionEventType : uint8_t
{
    Enter,
    Stay,
    Exit
};

struct CollisionEvent
{
    CollisionEventType type;
    EntityId a;
    EntityId b;

    bool aIsTrigger;
    bool bIsTrigger;
};
