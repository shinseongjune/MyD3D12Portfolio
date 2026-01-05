#pragma once
#include <cstdint>

enum class AudioBus : uint8_t { SFX, BGM };

struct SoundHandle
{
    uint32_t id = 0;

    bool IsValid() const { return id != 0; }
    bool operator==(const SoundHandle&) const = default;
};