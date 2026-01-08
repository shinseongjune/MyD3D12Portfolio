#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <dxgiformat.h>

#include "ImportTypes.h"

// Cubemap CPU texture standard: 6 faces in the order (+X, -X, +Y, -Y, +Z, -Z).
// For now we keep the same constraints as TextureCpuData: RGBA8 (4 bytes per pixel).
struct TextureCubeCpuData
{
    uint32_t width = 0;
    uint32_t height = 0;

    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    ImageColorSpace colorSpace = ImageColorSpace::SRGB;

    // Each face size must be width * height * 4.
    std::array<std::vector<uint8_t>, 6> pixels;
};
