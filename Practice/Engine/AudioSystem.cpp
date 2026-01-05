#include "AudioSystem.h"

#include "World.h"
#include "SoundManager.h"
#include "SoundClip.h"
#include "AudioSourceComponent.h"
#include "SoundHandle.h"
#include "EntityId.h"

#include <xaudio2.h>
#include <wrl.h>

#include <unordered_map>
#include <vector>
#include <cstdint>
#include "Utilities.h"

#pragma comment(lib, "xaudio2.lib")

using Microsoft::WRL::ComPtr;

struct AudioInstance
{
    uint32_t id = 0;
    IXAudio2SourceVoice* voice = nullptr;

    EntityId owner{};     // StopEntity 지원
    SoundHandle clip{};
    bool active = false;
};

class AudioSystem::Impl
{
public:
    // XAudio2 core
    ComPtr<IXAudio2> xaudio;
    IXAudio2MasteringVoice* master = nullptr;

    // command queue (B안)
    std::vector<AudioCommand> pending;
    std::vector<AudioCommand> processing;

    // instances
    uint32_t nextInstanceId = 1;
    std::vector<AudioInstance> instances;
    std::unordered_map<uint32_t, size_t> idToIndex;

    // entity -> instance (최소 정책: 엔티티당 1개만 추적)
    std::unordered_map<uint32_t, uint32_t> entityToInstance;

    // bgm slot
    uint32_t bgmInstanceId = 0;

public:
    AudioInstance* FindInstance(uint32_t id)
    {
        auto it = idToIndex.find(id);
        if (it == idToIndex.end()) return nullptr;
        return &instances[it->second];
    }

    void RebuildIndexFor(size_t idx)
    {
        if (idx < instances.size())
            idToIndex[instances[idx].id] = idx;
    }

    void RemoveInstanceAt(size_t idx)
    {
        const uint32_t deadId = instances[idx].id;
        idToIndex.erase(deadId);

        if (idx != instances.size() - 1)
        {
            std::swap(instances[idx], instances.back());
            RebuildIndexFor(idx);
        }
        instances.pop_back();
    }

    void StopAndDestroyVoice(AudioInstance& inst)
    {
        if (!inst.voice) return;

        inst.voice->Stop(0);
        inst.voice->FlushSourceBuffers();
        inst.voice->DestroyVoice();
        inst.voice = nullptr;

        inst.active = false;

        // entity mapping cleanup
        if (inst.owner.index != 0)
        {
            auto eit = entityToInstance.find(inst.owner.index);
            if (eit != entityToInstance.end() && eit->second == inst.id)
                entityToInstance.erase(eit);
        }

        // bgm slot cleanup
        if (bgmInstanceId == inst.id)
            bgmInstanceId = 0;
    }

    // 폴링으로 끝난 voice 회수
    void CollectFinishedVoices()
    {
        for (size_t i = 0; i < instances.size();)
        {
            AudioInstance& inst = instances[i];

            if (!inst.voice)
            {
                RemoveInstanceAt(i);
                continue;
            }

            XAUDIO2_VOICE_STATE state{};
            inst.voice->GetState(&state);

            // loop면 BuffersQueued가 계속 1 이상일 수 있음.
            // non-loop에서 0이면 재생 종료.
            if (inst.active && state.BuffersQueued == 0)
            {
                StopAndDestroyVoice(inst);
                RemoveInstanceAt(i);
                continue;
            }

            ++i;
        }
    }
};

AudioSystem::AudioSystem()
{
    m_impl = new Impl();
}

AudioSystem::~AudioSystem()
{
    Shutdown();
    delete m_impl;
    m_impl = nullptr;
}

void AudioSystem::Initialize()
{
    if (m_impl->xaudio) return;

    ThrowIfFailed(XAudio2Create(m_impl->xaudio.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR));
    ThrowIfFailed(m_impl->xaudio->CreateMasteringVoice(&m_impl->master));
}

void AudioSystem::Shutdown()
{
    if (!m_impl) return;

    // stop all voices
    for (auto& inst : m_impl->instances)
        m_impl->StopAndDestroyVoice(inst);

    m_impl->instances.clear();
    m_impl->idToIndex.clear();
    m_impl->entityToInstance.clear();
    m_impl->bgmInstanceId = 0;

    if (m_impl->master)
    {
        m_impl->master->DestroyVoice();
        m_impl->master = nullptr;
    }

    m_impl->xaudio.Reset();

    m_impl->pending.clear();
    m_impl->processing.clear();
}

void AudioSystem::PlayOneShot(SoundHandle clip, const AudioPlayDesc& desc)
{
    AudioCommand cmd{};
    cmd.type = AudioCommandType::PlayOneShot;
    cmd.clip = clip;
    cmd.desc = desc;
    m_impl->pending.push_back(cmd);
}

void AudioSystem::PlayFromEntity(EntityId e)
{
    AudioCommand cmd{};
    cmd.type = AudioCommandType::PlayFromEntity;
    cmd.entity = e;
    m_impl->pending.push_back(cmd);
}

void AudioSystem::StopInstance(uint32_t instId)
{
    AudioCommand cmd{};
    cmd.type = AudioCommandType::StopInstance;
    cmd.instanceId = instId;
    m_impl->pending.push_back(cmd);
}

void AudioSystem::StopEntity(EntityId e)
{
    AudioCommand cmd{};
    cmd.type = AudioCommandType::StopEntity;
    cmd.entity = e;
    m_impl->pending.push_back(cmd);
}

void AudioSystem::PlayBGM(SoundHandle clip, float volume)
{
    AudioPlayDesc d{};
    d.volume = volume;
    d.pitch = 1.0f;
    d.loop = true;
    d.bus = (uint8_t)AudioBus::BGM;

    AudioCommand cmd{};
    cmd.type = AudioCommandType::PlayBGM;
    cmd.clip = clip;
    cmd.desc = d;
    m_impl->pending.push_back(cmd);
}

void AudioSystem::StopBGM()
{
    AudioCommand cmd{};
    cmd.type = AudioCommandType::StopBGM;
    m_impl->pending.push_back(cmd);
}

static AudioPlayDesc MakeDescFromComponent(const AudioSourceComponent& c)
{
    AudioPlayDesc d{};
    d.volume = c.volume;
    d.pitch = c.pitch;
    d.loop = c.loop;
    d.bus = (uint8_t)c.bus;
    return d;
}

uint32_t AudioSystem::ExecutePlay(SoundHandle clip, const AudioPlayDesc& desc, EntityId owner, SoundManager& sounds)
{
    if (!clip.IsValid()) return 0;
    if (!m_impl->xaudio) return 0;
    if (!sounds.IsValid(clip)) return 0;

    const SoundClip& sc = sounds.Get(clip);
    if (sc.pcm.empty()) return 0;

    IXAudio2SourceVoice* sv = nullptr;
    ThrowIfFailed(m_impl->xaudio->CreateSourceVoice(
        &sv,
        &sc.wfx,
        0,
        XAUDIO2_DEFAULT_FREQ_RATIO,
        nullptr
    ));

    // pitch
    if (desc.pitch > 0.0f && desc.pitch != 1.0f)
        ThrowIfFailed(sv->SetFrequencyRatio(desc.pitch));

    // volume
    if (desc.volume != 1.0f)
        ThrowIfFailed(sv->SetVolume(desc.volume));

    // submit buffer
    XAUDIO2_BUFFER buf{};
    buf.AudioBytes = (UINT32)sc.pcm.size();
    buf.pAudioData = (const BYTE*)sc.pcm.data();
    buf.pContext = nullptr;

    if (desc.loop)
    {
        buf.LoopBegin = 0;
        buf.LoopLength = 0;
        buf.LoopCount = XAUDIO2_LOOP_INFINITE;
    }

    ThrowIfFailed(sv->SubmitSourceBuffer(&buf));
    ThrowIfFailed(sv->Start(0));

    AudioInstance inst{};
    inst.id = m_impl->nextInstanceId++;
    inst.voice = sv;
    inst.owner = owner;
    inst.clip = clip;
    inst.active = true;

    const size_t idx = m_impl->instances.size();
    m_impl->instances.push_back(inst);
    m_impl->idToIndex[inst.id] = idx;

    // entity -> instance (최소 정책: 엔티티당 1개만)
    if (owner.index != 0)
    {
        auto it = m_impl->entityToInstance.find(owner.index);
        if (it != m_impl->entityToInstance.end())
        {
            // 기존 재생이 있으면 stop하고 교체
            const uint32_t prev = it->second;
            if (auto* prevInst = m_impl->FindInstance(prev))
                m_impl->StopAndDestroyVoice(*prevInst);
        }
        m_impl->entityToInstance[owner.index] = inst.id;
    }

    return inst.id;
}

void AudioSystem::Update(World& world, SoundManager& sounds)
{
    if (!m_impl->xaudio)
        return;

    // 1) finished voice 정리
    m_impl->CollectFinishedVoices();

    // 2) 커맨드 스왑(B안 핵심)
    m_impl->processing.clear();
    m_impl->processing.swap(m_impl->pending);

    // 3) 커맨드 실행
    for (const AudioCommand& cmd : m_impl->processing)
    {
        switch (cmd.type)
        {
        case AudioCommandType::PlayOneShot:
        {
            ExecutePlay(cmd.clip, cmd.desc, EntityId{}, sounds);
        } break;

        case AudioCommandType::PlayFromEntity:
        {
            if (!world.HasAudioSource(cmd.entity))
                break;

            const AudioSourceComponent& src = world.GetAudioSource(cmd.entity);
            if (!src.clip.IsValid())
                break;

            const AudioPlayDesc d = MakeDescFromComponent(src);
            ExecutePlay(src.clip, d, cmd.entity, sounds);
        } break;

        case AudioCommandType::StopInstance:
        {
            if (auto* inst = m_impl->FindInstance(cmd.instanceId))
                m_impl->StopAndDestroyVoice(*inst);
        } break;

        case AudioCommandType::StopEntity:
        {
            const uint32_t eid = cmd.entity.index;                                                                                                                                                      
            auto it = m_impl->entityToInstance.find(eid);
            if (it != m_impl->entityToInstance.end())
            {
                const uint32_t instId = it->second;
                if (auto* inst = m_impl->FindInstance(instId))
                    m_impl->StopAndDestroyVoice(*inst);
                m_impl->entityToInstance.erase(it);
            }
        } break;

        case AudioCommandType::PlayBGM:
        {
            // 기존 BGM 끄기
            if (m_impl->bgmInstanceId != 0)
            {
                if (auto* inst = m_impl->FindInstance(m_impl->bgmInstanceId))
                    m_impl->StopAndDestroyVoice(*inst);
                m_impl->bgmInstanceId = 0;
            }

            // BGM은 loop=true로 들어오는 걸 전제로 함
            const uint32_t instId = ExecutePlay(cmd.clip, cmd.desc, EntityId{}, sounds);
            m_impl->bgmInstanceId = instId;
        } break;

        case AudioCommandType::StopBGM:
        {
            if (m_impl->bgmInstanceId != 0)
            {
                if (auto* inst = m_impl->FindInstance(m_impl->bgmInstanceId))
                    m_impl->StopAndDestroyVoice(*inst);
                m_impl->bgmInstanceId = 0;
            }
        } break;

        default:
            break;
        }
    }

    // 4) stop 처리 후 한 번 더 정리
    m_impl->CollectFinishedVoices();
}
