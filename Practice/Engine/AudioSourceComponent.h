#pragma once
#include "SoundHandle.h"
#include <cstdint>

struct AudioSourceComponent
{
    SoundHandle clip{};
    AudioBus bus = AudioBus::SFX;

    float volume = 1.0f;
    float pitch = 1.0f;
    bool  loop = false;

    uint32_t playingInstanceId = 0; // 0ÀÌ¸י none
};