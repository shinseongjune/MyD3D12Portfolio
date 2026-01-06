#pragma once
#include "World.h"
#include "UIDrawItem.h"

class UIHudSystem
{
public:
    void Build(const World& world, uint32_t screenW, uint32_t screenH,
        std::vector<UIDrawItem>& outItems) const;

};
