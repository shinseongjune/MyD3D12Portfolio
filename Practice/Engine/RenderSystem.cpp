#include "RenderSystem.h"
#include "TextureHandle.h"
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

        const auto& mc = world.GetMesh(e);
        const auto& tr = world.GetTransform(e);

        const MaterialComponent* mat = world.HasMaterial(e) ? &world.GetMaterial(e) : nullptr;

        for (const auto& d : mc.draws)
        {
            RenderItem it{};
            it.mesh = d.mesh;
            it.world = tr.world;
            it.startIndex = d.startIndex;
            it.indexCount = d.indexCount;

            if (mat)
            {
                const uint32_t idx = (d.materialIndex < mat->slots.size()) ? d.materialIndex : 0u;
                const auto& slot = mat->slots.empty() ? mat->Primary() : mat->slots[idx];
                it.color = slot.color;
                it.albedo = slot.albedo;
            }
            else
            {
                it.color = { 1,1,1,1 };
                it.albedo = TextureHandle{ 0 };
            }

            outItem.push_back(it);
        }
    }
}