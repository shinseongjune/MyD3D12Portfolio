#pragma once
#include <cstdint>
#include <vector>
#include <dxgiformat.h>
#include "ImportTypes.h"

// WIC로 로드한 "CPU 텍스처 표준" (초기엔 RGBA8만)
struct TextureCpuData
{
    uint32_t width = 0;
    uint32_t height = 0;

    // 픽셀 데이터는 항상 RGBA8 (4 bytes per pixel)
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;

    // 렌더러 업로드 시 SRGB 포맷으로 만들지 결정하기 위한 힌트
    ImageColorSpace colorSpace = ImageColorSpace::SRGB;

    std::vector<uint8_t> pixels; // size = width * height * 4
};