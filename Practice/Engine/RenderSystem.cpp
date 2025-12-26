#include "RenderSystem.h"
#include <DirectXMath.h>

using namespace DirectX;

void RenderSystem::Build(const World& world, std::vector<RenderItem>& outItems) const
{
#if defined(_DEBUG)
    assert(world.TransformsUpdatedThisFrame() && "RenderSystem::Build called before World::UpdateTransforms()");
#endif

    outItems.clear();

    // "Transform + Mesh + Material" 조합을 가진 엔티티를 모두 그린다.
    // (지금은 RenderItem이 최대 MaxDrawsPerFrame까지만 그려지므로, 우선 테스트에 충분)
    const auto& entities = world.GetTransformEntities();
    outItems.reserve(entities.size());

    for (EntityId e : entities)
    {
        if (!world.HasMesh(e) || !world.HasMaterial(e))
            continue;

        RenderItem it{};
        it.world = world.GetWorldMatrix(e);
        it.meshId = world.GetMesh(e).meshId;
        it.color = world.GetMaterial(e).color;
        outItems.push_back(it);
    }
}
