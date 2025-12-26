#include "RenderSystem.h"
#include <DirectXMath.h>

using namespace DirectX;

void RenderSystem::Build(const World& world, std::vector<RenderItem>& outItem) const
{
#if defined(_DEBUG)
    assert(world.TransformsUpdatedThisFrame());
#endif

    outItem.clear();

    for (EntityId e : world.GetTransformEntities())
    {
        if (!world.HasMesh(e))
            continue;

        const auto& mesh = world.GetMesh(e);
        const auto& tr = world.GetTransform(e);

        RenderItem it{};
        it.mesh = mesh.mesh;
        it.world = tr.world;
        it.color = { 0.7f, 0.7f, 0.9f, 1.0f };

        outItem.push_back(it);
    }
}