#pragma once
#include "framework.h"
#include <vector>
#include <cstdint>
#include <mmreg.h> // WAVEFORMATEX

struct SoundClip
{
    WAVEFORMATEX wfx{};
    std::vector<uint8_t> pcm; // interleaved PCM bytes
};