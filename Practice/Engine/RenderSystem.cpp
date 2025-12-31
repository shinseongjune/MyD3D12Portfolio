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

        // Material이 없으면 기본값
        if (world.HasMaterial(e))
        {
            const auto& mat = world.GetMaterial(e);
            it.color = mat.color;
            it.srvIndex = mat.srvIndex;
        }
        else
        {
            it.color = { 0.7f, 0.7f, 0.9f, 1.0f };
            it.srvIndex = 0;
        }

        outItem.push_back(it);
    }
}