#pragma once
#include <vector>
#include <DirectXMath.h>
#include "World.h"
#include "SceneContext.h"

class EnemyIndicatorHUD
{
public:
    void Initialize(SceneContext& ctx, int maxSlots);
    void SetEnabled(bool e) { m_enabled = e; }
    void Update(SceneContext& ctx, EntityId cam, EntityId player,
        const std::vector<EntityId>& enemies);

private:
    struct Slot
    {
        // 브라켓은 선으로 그릴 거라 별도 엔티티 없음
        bool active = false;
    };

    bool m_enabled = true;
    std::vector<Slot> m_slots;
};
