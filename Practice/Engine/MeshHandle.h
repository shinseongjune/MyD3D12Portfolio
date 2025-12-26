#pragma once
#include <cstdint>

struct MeshHandle
{
    uint32_t id = 0;

    bool IsValid() const { return id != 0; }
    bool operator==(const MeshHandle&) const = default;
};
