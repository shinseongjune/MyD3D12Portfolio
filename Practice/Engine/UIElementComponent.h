#pragma once
#include <DirectXMath.h>
#include "TextureHandle.h"
#include "EntityId.h"

struct UIElementComponent
{
    bool enabled = true;

    DirectX::XMFLOAT2 anchor{ 0.0f, 0.0f }; 
    DirectX::XMFLOAT2 pivot{ 0.0f, 0.0f };     
    DirectX::XMFLOAT2 anchoredPosPx{ 0.0f, 0.0f };
    DirectX::XMFLOAT2 sizePx{ 100.0f, 100.0f };

    EntityId uiParent = EntityId::Invalid();

    TextureHandle texture{ 0 };
    DirectX::XMFLOAT4 color{ 1,1,1,1 };
    float u0 = 0, v0 = 0;
    float u1 = 1, v1 = 1;

    float z = 0.0f;
};
