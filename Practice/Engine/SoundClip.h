#pragma once
#include "framework.h"
#include <vector>
#include <cstdint>
#include <mmreg.h> // WAVEFORMATEX
#include <memory>

struct SoundClip
{
    WAVEFORMATEX wfx{};
    std::shared_ptr<std::vector<uint8_t>> pcm;
};