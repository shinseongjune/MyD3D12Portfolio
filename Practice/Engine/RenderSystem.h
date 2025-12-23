#pragma once
#include <vector>
#include "RenderItem.h"
#include "World.h"

class RenderSystem
{
public:
    // 이번 프레임의 RenderItem 리스트 생성
    void Build(const World& world, std::vector<RenderItem>& outItems) const;
};
