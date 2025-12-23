#include "RenderSystem.h"
#include <DirectXMath.h>

using namespace DirectX;

void RenderSystem::Build(const World& world, std::vector<RenderItem>& outItems) const
{
    outItems.clear();

    // 1) Player(ÆÄ¶û)
    EntityId player = world.FindByName("Player");
    if (world.HasTransform(player))
    {
        RenderItem it{};
        it.world = world.GetWorldMatrix(player);
        it.color = { 0.2f, 0.6f, 1.0f, 1.0f }; // ÆÄ¶û
        outItems.push_back(it);
    }

    // 2) MouthSocket(³ë¶û + ÀÛ°Ô)
    EntityId mouth = world.FindByName("MouthSocket");
    if (world.HasTransform(mouth))
    {
        RenderItem it{};
        it.world = world.GetWorldMatrix(mouth);
        it.color = { 1.0f, 1.0f, 0.2f, 1.0f };
        outItems.push_back(it);
    }
}
