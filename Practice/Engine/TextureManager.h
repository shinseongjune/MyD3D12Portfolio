#pragma once
#include "TextureHandle.h"
#include "TextureCpuData.h"          // 네가 만든 TextureCpuData + LoadTextureRGBA8_WIC 선언 포함(또는 include 분리)
#include <unordered_map>
#include <functional>
#include <string>

class TextureManager
{
public:
    // 1) CPU 데이터로 직접 등록(테스트용/절차 분리용)
    TextureHandle Create(const TextureCpuData& tex);

    // 2) 파일에서 로드 + 등록(보통은 이걸 씀)
    Result<TextureHandle> Load(const std::string& utf8Path,
        ImageColorSpace colorSpace = ImageColorSpace::SRGB,
        bool flipY = false);

    const TextureCpuData& Get(TextureHandle h) const;
    bool IsValid(TextureHandle h) const;

    void Destroy(TextureHandle h);

    using OnDestroyCallback = std::function<void(uint32_t texId)>;
    void SetOnDestroy(OnDestroyCallback cb) { m_onDestroy = std::move(cb); }

private:
    uint32_t m_nextId = 1;

    // id -> CPU texture
    std::unordered_map<uint32_t, TextureCpuData> m_textures;

    // (선택) path 캐시: 같은 파일은 한 번만 로드하고 같은 핸들 반환
    std::unordered_map<std::string, uint32_t> m_pathToId;

    OnDestroyCallback m_onDestroy;
};
