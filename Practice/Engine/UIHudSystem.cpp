#include "UIHudSystem.h"

#include <algorithm>
#include "UIDrawItem.h"

// UI 좌표계:
// - screen space 픽셀
// - (0,0) = 화면 좌상단, +x 오른쪽, +y 아래
//
// Resolve 규칙(간단판):
// - uiParent가 없으면 부모 rect = 화면 전체
// - uiParent가 있고 UIElementComponent가 있으면 부모 rect = 부모의 계산된 rect(1단계만)
// - anchor는 부모 rect 내부 [0..1] 정규화 위치
// - anchoredPosPx는 anchor에서의 오프셋(px)
// - pivot은 자기 rect 내부 [0..1] 정규화 (0,0=좌상단)

static void ComputeRect(
    float parentX, float parentY, float parentW, float parentH,
    const UIElementComponent& c,
    float& outX, float& outY, float& outW, float& outH)
{
    outW = c.sizePx.x;
    outH = c.sizePx.y;

    const float ax = parentX + c.anchor.x * parentW;
    const float ay = parentY + c.anchor.y * parentH;

    // anchor 기준 위치 + 오프셋
    const float px = ax + c.anchoredPosPx.x;
    const float py = ay + c.anchoredPosPx.y;

    // pivot 적용해서 좌상단 구함
    outX = px - c.pivot.x * outW;
    outY = py - c.pivot.y * outH;
}

void UIHudSystem::Build(const World& world, uint32_t screenW, uint32_t screenH, std::vector<UIDrawItem>& outItems) const
{
    outItems.clear();

    const auto& ents = world.GetUIElementEntities();
    if (ents.empty())
        return;

    outItems.reserve(ents.size());

    // 1) 먼저 엔티티 순서대로 UIDrawItem 생성
    for (EntityId e : ents)
    {
        if (!world.IsAlive(e) || !world.HasUIElement(e))
            continue;

        const UIElementComponent& c = world.GetUIElement(e);
        if (!c.enabled)
            continue;

        // parent rect
        float parentX = 0.0f, parentY = 0.0f;
        float parentW = (float)screenW, parentH = (float)screenH;

        if (c.uiParent.IsValid() && world.IsAlive(c.uiParent) && world.HasUIElement(c.uiParent))
        {
            const UIElementComponent& pc = world.GetUIElement(c.uiParent);

            // 부모의 부모는 screen으로 두고(1단계), 부모 rect 계산
            float ppx, ppy, ppw, pph;
            ComputeRect(0.0f, 0.0f, (float)screenW, (float)screenH, pc, ppx, ppy, ppw, pph);

            parentX = ppx; parentY = ppy; parentW = ppw; parentH = pph;
        }

        float x, y, w, h;
        ComputeRect(parentX, parentY, parentW, parentH, c, x, y, w, h);

        UIDrawItem it{};
        it.x = x; it.y = y; it.w = w; it.h = h;
        it.u0 = c.u0; it.v0 = c.v0; it.u1 = c.u1; it.v1 = c.v1;
        it.tex = c.texture;
        it.color = c.color;
        it.z = c.z;
        outItems.push_back(it);
    }

    if (outItems.size() <= 1)
        return;

    // 2) z로 정렬(안정 정렬: 같은 z는 입력 순서 유지)
    std::stable_sort(outItems.begin(), outItems.end(),
        [](const UIDrawItem& a, const UIDrawItem& b)
        {
            return a.z < b.z;
        });
}
