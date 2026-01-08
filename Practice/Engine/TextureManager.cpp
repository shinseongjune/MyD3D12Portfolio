#include "TextureManager.h"
#include <stdexcept>
#include "TextureLoader_WIC.h"

TextureHandle TextureManager::Create(const TextureCpuData& tex)
{
    TextureHandle h{};
    h.id = m_nextId++;
    m_textures.emplace(h.id, tex);
    return h;
}

Result<TextureHandle> TextureManager::Load(const std::string& utf8Path,
    ImageColorSpace colorSpace,
    bool flipY)
{
    // path Ä³½Ã
    if (auto it = m_pathToId.find(utf8Path); it != m_pathToId.end())
        return Result<TextureHandle>{ it->second };

    auto loaded = LoadTextureRGBA8_WIC(utf8Path, colorSpace, flipY);
    if (!loaded.IsOk())
    {
        Result<TextureHandle> r;
		r.error = loaded.error;
		return r;
    }


    TextureHandle h{};
    h.id = m_nextId++;
    m_textures.emplace(h.id, std::move(loaded.value));
    m_pathToId.emplace(utf8Path, h.id);

    return Result<TextureHandle>(h);
}

const TextureCpuData& TextureManager::Get(TextureHandle h) const
{
    auto it = m_textures.find(h.id);
    if (it == m_textures.end())
        throw std::runtime_error("TextureManager::Get invalid handle");
    return it->second;
}

Result<TextureHandle> TextureManager::LoadCubemap(const std::array<std::string, 6>& utf8Paths, ImageColorSpace colorSpace, bool flipY)
{
    // Build a stable cache key (order matters)
    std::string key = "cube:";
    for (size_t i = 0; i < utf8Paths.size(); ++i)
    {
        key += utf8Paths[i];
        key += '|';
    }

    if (auto it = m_cubePathToId.find(key); it != m_cubePathToId.end())
        return Result<TextureHandle>{ it->second };

    // Load 6 faces
    TextureCubeCpuData cube{};
    for (size_t face = 0; face < 6; ++face)
    {
        auto loaded = LoadTextureRGBA8_WIC(utf8Paths[face], colorSpace, flipY);
        if (!loaded.IsOk())
            return Result<TextureHandle>::Fail(loaded.error->message);

        if (face == 0)
        {
            cube.width = loaded.value.width;
            cube.height = loaded.value.height;
            cube.format = loaded.value.format;
            cube.colorSpace = loaded.value.colorSpace;

            if (loaded.value.pixels.size() != cube.width * cube.height * 4)
                return Result<TextureHandle>::Fail("Cubemap face pixel data size mismatch.");
        }
        else
        {
            // Validate all faces match
            if (loaded.value.width != cube.width || loaded.value.height != cube.height)
                return Result<TextureHandle>::Fail("Cubemap faces must have identical dimensions.");
            if (loaded.value.format != cube.format)
                return Result<TextureHandle>::Fail("Cubemap faces must have identical formats.");
            if (loaded.value.pixels.size() != cube.width * cube.height * 4)
                return Result<TextureHandle>::Fail("Cubemap face pixel data size mismatch.");
        }

        cube.pixels[face] = std::move(loaded.value.pixels);
    }

    TextureHandle h{};
    h.id = m_nextId++;
    m_cubemaps.emplace(h.id, std::move(cube));
    m_cubePathToId.emplace(std::move(key), h.id);

    return Result<TextureHandle>(h);
}

const TextureCubeCpuData& TextureManager::GetCube(TextureHandle h) const
{
    auto it = m_cubemaps.find(h.id);
    if (it == m_cubemaps.end())
        throw std::runtime_error("TextureManager::GetCube invalid handle");
    return it->second;
}

bool TextureManager::IsCubemap(TextureHandle h) const
{
    return h.id != 0 && m_cubemaps.find(h.id) != m_cubemaps.end();
}

bool TextureManager::IsValid(TextureHandle h) const
{
    return h.id != 0 &&
        (m_textures.find(h.id) != m_textures.end() ||
            m_cubemaps.find(h.id) != m_cubemaps.end());
}

void TextureManager::Destroy(TextureHandle h)
{
    if (!IsValid(h)) return;

    if (m_onDestroy)
        m_onDestroy(h.id);

    m_textures.erase(h.id);
    m_cubemaps.erase(h.id);
}