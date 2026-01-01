#pragma once
#include <string>
#include "ImportTypes.h" // Result<T> 재사용
#include "TextureCPUData.h"

// path(UTF-16)로 로드
Result<TextureCpuData> LoadTextureRGBA8_WIC(
    const std::wstring& path,
    ImageColorSpace colorSpace = ImageColorSpace::SRGB,
    bool flipY = false);

// path(UTF-8)로 로드 (AssetPipeline이 std::string 쓰는 점 고려)
Result<TextureCpuData> LoadTextureRGBA8_WIC(
    const std::string& utf8Path,
    ImageColorSpace colorSpace = ImageColorSpace::SRGB,
    bool flipY = false);
