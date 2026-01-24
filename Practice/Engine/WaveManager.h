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

private:
    EntityId PickSpawn(SceneContext& ctx) const;
    void CleanupDead(SceneContext& ctx);

private:
    std::vector<WaveSpec> m_waves;          // 웨이브 테이블
    WaveRuntime m_rt;
    std::vector<EntityId> m_currentEnemies; // 현재 살아있는 적(추적용)
    std::vector<SoundHandle> m_deathSounds;

};