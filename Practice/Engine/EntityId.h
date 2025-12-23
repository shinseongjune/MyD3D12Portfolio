#pragma once
#include <cstdint>

struct EntityId
{
    uint32_t index = 0;
    uint32_t generation = 0;

    static constexpr uint32_t InvalidIndex = 0xFFFFFFFFu;

    static constexpr EntityId Invalid() { return { InvalidIndex, 0 }; }
    constexpr bool IsValid() const { return index != InvalidIndex; }

    friend constexpr bool operator==(const EntityId& a, const EntityId& b)
    {
        return a.index == b.index && a.generation == b.generation;
    }
    friend constexpr bool operator!=(const EntityId& a, const EntityId& b)
    {
        return !(a == b);
    }
};
