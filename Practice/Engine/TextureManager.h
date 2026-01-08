#pragma once
#include "TextureHandle.h"
#include "TextureCpuData.h"
#include "TextureCubeCPUData.h"
#include "Utilities.h"
#include <unordered_map>
#include <functional>
#include <string>
#include <array>

class TextureManager
{
public:
    TextureHandle Create(const TextureCpuData& tex);

    Result<TextureHandle> Load(const std::string& utf8Path,
        ImageColorSpace colorSpace = ImageColorSpace::SRGB,
        bool flipY = false);

    // Cubemap load + register. Faces order: +X, -X, +Y, -Y, +Z, -Z
    Result<TextureHandle> LoadCubemap(const std::array<std::string, 6>& utf8Paths,
        ImageColorSpace colorSpace = ImageColorSpace::SRGB,
        bool flipY = false);

    const TextureCpuData& Get(TextureHandle h) const;
    const TextureCubeCpuData& GetCube(TextureHandle h) const;
    bool IsCubemap(TextureHandle h) const;
    bool IsValid(TextureHandle h) const;

    void Destroy(TextureHandle h);

    using OnDestroyCallback = std::function<void(uint32_t texId)>;
    void SetOnDestroy(OnDestroyCallback cb) { m_onDestroy = std::move(cb); }

private:
    uint32_t m_nextId = 1;

    std::unordered_map<uint32_t, TextureCpuData> m_textures;
    std::unordered_map<uint32_t, TextureCubeCpuData> m_cubemaps;

    std::unordered_map<std::string, uint32_t> m_pathToId;
    std::unordered_map<std::string, uint32_t> m_cubePathToId;

    OnDestroyCallback m_onDestroy;
};
