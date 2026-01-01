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
    // path 캐시
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

bool TextureManager::IsValid(TextureHandle h) const
{
    return h.id != 0 && m_textures.find(h.id) != m_textures.end();
}

void TextureManager::Destroy(TextureHandle h)
{
    if (!IsValid(h)) return;

    if (m_onDestroy)
        m_onDestroy(h.id);

    // (선택) 역방향 캐시 제거까지 하려면 id->path 맵도 필요
    m_textures.erase(h.id);
}
