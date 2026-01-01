#pragma once
#include <cstdint>

struct TextureHandle
{
    uint32_t id = 0;

    bool IsValid() const { return id != 0; }
    bool operator==(const TextureHandle&) const = default;
};