#pragma once
#include "SoundHandle.h"
#include "SoundClip.h"
#include <unordered_map>
#include <functional>

class SoundManager
{
public:
    // 이미 디코딩된 SoundClip(PCM)을 등록하고 핸들을 돌려줌
    SoundHandle Create(const SoundClip& clip);

    const SoundClip& Get(SoundHandle h) const;
    bool IsValid(SoundHandle h) const;

    void Destroy(SoundHandle h);

    // (선택) 파괴 시점에 재생 중인 인스턴스 정리 같은 걸 AudioSystem에서 처리할 수 있게
    using OnDestroyCallback = std::function<void(uint32_t soundId)>;
    void SetOnDestroy(OnDestroyCallback cb) { m_onDestroy = std::move(cb); }

private:
    uint32_t m_nextId = 1;
    std::unordered_map<uint32_t, SoundClip> m_sounds;

    OnDestroyCallback m_onDestroy;
};
