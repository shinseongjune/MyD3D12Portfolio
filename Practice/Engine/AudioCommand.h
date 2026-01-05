#pragma once
#include "SoundHandle.h"
#include "EntityId.h"
#include <cstdint>

struct AudioPlayDesc
{
    float volume = 1.0f;
    float pitch = 1.0f;
    bool  loop = false;
    uint8_t bus = 0;
};

enum class AudioCommandType : uint8_t
{
    PlayOneShot,
    PlayFromEntity,
    StopInstance,
    StopEntity,

    PlayBGM,
    StopBGM,
};

struct AudioCommand
{
    AudioCommandType type{};

    SoundHandle clip{};
    AudioPlayDesc desc{};

    EntityId entity{};
    uint32_t instanceId = 0;
};