#include "WaveManager.h"
#include <string>
#include "SceneContext.h"
#include "MyRandom.h"
#include "StatsComponent.h"

void WaveManager::Start(const std::vector<EntityId>& spawnPositions)
{
    m_spawns = spawnPositions;

    // 웨이브 테이블
    m_waves.clear();
    m_waves.push_back({ 1, 1, 1.5f });
    m_waves.push_back({ 2, 2, 1.2f });
    m_waves.push_back({ 3, 3, 1.0f });

    StartWave(0);
}

void WaveManager::StartWave(int waveIndex)
{
    m_rt.waveIndex = waveIndex;
    m_rt.spawned = 0;
    m_rt.timer = 0.f;
    m_rt.active = true;

    m_currentEnemies.clear();
}

void WaveManager::Update(SceneContext& ctx, float dt)
{
    if (!m_rt.active) return;
    if (m_spawns.empty()) return;
    if (m_rt.waveIndex < 0 || m_rt.waveIndex >= (int)m_waves.size()) return;

    CleanupDead(ctx);

    const WaveSpec& spec = m_waves[m_rt.waveIndex];

    // 스폰이 이미 끝났으면 "전멸"만 기다림
    if (m_rt.spawned >= spec.totalCount)
    {
        if (m_currentEnemies.empty())
        {
            // 클리어
            m_rt.active = false;

            if (m_rt.waveIndex + 1 >= (int)m_waves.size()) 
            {
                GameManager::GetInstance().SetState(GameManager::State::Win);
            }

            StartWave(m_rt.waveIndex + 1);
        }
        return;
    }

    // 동시에 살아있는 수 제한
    if ((int)m_currentEnemies.size() >= spec.maxAlive)
        return;

    // 타이머 누적
    m_rt.timer += dt;
    if (m_rt.timer < spec.spawnInterval)
        return;

    m_rt.timer -= spec.spawnInterval;

    // 스폰 1회
    EntityId sp = PickSpawn(ctx);
    CreateCruiser(ctx, sp);
    m_rt.spawned++;
}

void WaveManager::CleanupDead(SceneContext& ctx)
{
    auto& world = ctx.world;

    size_t write = 0;
    for (size_t i = 0; i < m_currentEnemies.size(); ++i)
    {
        EntityId e = m_currentEnemies[i];
        if (world.IsAlive(e))
        {
            m_currentEnemies[write++] = e;
        }
    }
    m_currentEnemies.resize(write);
}

EntityId WaveManager::PickSpawn(SceneContext& ctx) const
{
    int n = (int)m_spawns.size();
    int idx = RandRangeInt(0, n - 1);
    return m_spawns[idx];
}

void WaveManager::CreateCruiser(SceneContext& ctx, EntityId spawnPos)
{
    auto& world = ctx.world;

    std::string name = std::string("enemy") + std::to_string(enemyNumber++);
    EntityId e = ctx.SpawnModel(model_cruiser, SpawnModelOptions{ name.c_str()}).value;

    // 1) spawnPos의 Transform 복사 (위치 + 회전)
    auto pos = world.GetLocalPosition(spawnPos);
    auto rot = world.GetLocalRotation(spawnPos);

    world.SetLocalPosition(e, pos);
    world.SetLocalRotation(e, rot);

    // 2) 머티리얼/AI/체력 등 붙이기
    world.AddMaterial(e, mat_cruiser);

    // Attach collider & rigidbody
    {
        ColliderComponent col;
        col.shapeType = ShapeType::Sphere;
        col.localCenter = { 0, -1.0f, 0.5f };
        col.sphere.radius = 8.0f;
        col.layer = 1 << 1;
        ctx.world.AddCollider(e, col);

        RigidBodyComponent rb;
        rb.type = BodyType::Dynamic;
        rb.useGravity = false;
        ctx.world.AddRigidBody(e, rb);
    }
    // Attach Stats
    {
        auto stats = std::make_unique<StatsComponent>();
        stats->SetHp(100);
        stats->SetQuadMesh(m_quad);
        stats->SetDieAnims(m_deathAnimsContainer[RandRangeInt(0, 2)]);
        ctx.world.AddScript(e, std::move(stats));
    }

    m_currentEnemies.push_back(e);
}