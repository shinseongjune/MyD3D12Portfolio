#pragma once
#include <vector>
#include "ModelAsset.h"
#include "MaterialComponent.h"
#include "EntityId.h"
#include "MeshHandle.h"
#include "TextureHandle.h"
#include "SoundHandle.h"

struct SceneContext;

struct WaveSpec
{
	int totalCount = 5;          // 이 웨이브에서 총 몇 대 스폰?
	int maxAlive = 4;            // 동시에 살아있는 최대 수(스폰 몰아치기 방지)
	float spawnInterval = 2.0f;  // 스폰 간격(초)
};

struct WaveRuntime
{
	int waveIndex = 0;
	int spawned = 0;     // 이번 웨이브에서 지금까지 스폰한 수
	float timer = 0.f;
	bool active = false;
};

class WaveManager
{
public:
    ModelAsset model_cruiser;
    MaterialComponent mat_cruiser;
    int enemyNumber = 0;
    MeshHandle m_quad;
    std::vector<std::vector<TextureHandle>> m_deathAnimsContainer;
    std::vector<EntityId> m_spawns;         // 후보 스폰 포인트들

    void Start(const std::vector<EntityId>& spawnPositions);
    void StartWave(int waveIndex);
    void Update(SceneContext& ctx, float dt);

    void CreateCruiser(SceneContext& ctx, EntityId spawnPos);
    void SetDieSounds(std::vector<SoundHandle> sounds) { m_deathSounds = sounds; }
    void SetGunAssets(TextureHandle tex, std::vector<TextureHandle> hitAnims, std::vector<SoundHandle> fireSounds, std::vector<SoundHandle> hitSounds)
    {
        m_bulletTex = tex;
        m_bulletHitAnims = hitAnims;
        m_gunFireSounds = fireSounds;
        m_bulletHitSounds = hitSounds;
    }

    void SetMissileAssets(ModelAsset model, TextureHandle tex, std::vector<TextureHandle> hitAnims, SoundHandle launchSound, SoundHandle boomSound)
    {
        m_missile = model;
        m_missileTex = tex;
        m_missleHitAnims = hitAnims;
        m_missileLaunchSound = launchSound;
        m_missileBoomSound = boomSound;
    }

    void SetAIData(EntityId player, float radius, float groundBoundary)
    {
        m_player = player;
        m_playBoundary = radius;
        m_groundBoundary = groundBoundary;
    }

    const std::vector<EntityId>& GetCurrentEnemies() const { return m_currentEnemies; }
    int GetCurrentEnemiesCount() const { return (int)m_currentEnemies.size(); }
    int GetCurrentWaveIndex() const { return m_rt.waveIndex; }
    int GetCurrentWaveEnemiesCound() const { return m_waves[m_rt.waveIndex].totalCount; }

private:
    EntityId PickSpawn(SceneContext& ctx) const;
    void CleanupDead(SceneContext& ctx);

private:
    std::vector<WaveSpec> m_waves;          // 웨이브 테이블
    WaveRuntime m_rt;
    std::vector<EntityId> m_currentEnemies; // 현재 살아있는 적(추적용)
    std::vector<SoundHandle> m_deathSounds;

    // gun
    TextureHandle m_bulletTex;
    std::vector<TextureHandle> m_bulletHitAnims;
    std::vector<SoundHandle> m_gunFireSounds;
    std::vector<SoundHandle> m_bulletHitSounds;

    // missile
    ModelAsset m_missile;
    TextureHandle m_missileTex;
    std::vector<TextureHandle> m_missleHitAnims;
    SoundHandle m_missileLaunchSound;
    SoundHandle m_missileBoomSound;

    // player & map
    EntityId m_player;
    float m_playBoundary;
    float m_groundBoundary;

};