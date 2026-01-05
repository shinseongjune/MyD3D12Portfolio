#include "SoundManager.h"
#include <cassert>

SoundHandle SoundManager::Create(const SoundClip& clip)
{
    const uint32_t id = m_nextId++;
    m_sounds.emplace(id, clip);
    return SoundHandle{ id };
}

const SoundClip& SoundManager::Get(SoundHandle h) const
{
    auto it = m_sounds.find(h.id);
    assert(it != m_sounds.end() && "Invalid SoundHandle");
    return it->second;
}

bool SoundManager::IsValid(SoundHandle h) const
{
    return m_sounds.find(h.id) != m_sounds.end();
}

void SoundManager::Destroy(SoundHandle h)
{
    auto it = m_sounds.find(h.id);
    if (it == m_sounds.end())
        return;

    if (m_onDestroy)
        m_onDestroy(h.id);

    m_sounds.erase(it);
}
