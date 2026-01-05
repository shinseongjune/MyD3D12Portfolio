#pragma once
#include <cstdint>
#include "EntityId.h"
#include "SoundHandle.h"
#include "AudioCommand.h"

class World;
class SoundManager;

class AudioSystem
{
public:
    AudioSystem();
    ~AudioSystem();

    void Initialize();
    void Shutdown();

    // --- Request API ---
    void PlayOneShot(SoundHandle clip, const AudioPlayDesc& desc);
    void PlayFromEntity(EntityId e);
    void StopInstance(uint32_t instId);
    void StopEntity(EntityId e);

    // BGM (지속/단일 슬롯)
    void PlayBGM(SoundHandle clip, float volume = 1.0f);
    void StopBGM();

    // --- Execute ---
    void Update(World& world, SoundManager& sounds);

private:
    uint32_t ExecutePlay(SoundHandle clip, const AudioPlayDesc& desc, EntityId owner, SoundManager& sounds);

private:
    class Impl;
    Impl* m_impl = nullptr;
};
