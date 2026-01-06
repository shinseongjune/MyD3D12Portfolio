#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include "TextureHandle.h"

struct UIDrawItem
{
    // 화면 픽셀 좌표(좌상단 기준)
    float x = 0;   // left
    float y = 0;   // top
    float w = 0;
    float h = 0;

    // UV (0~1)
    float u0 = 0, v0 = 0;
    float u1 = 1, v1 = 1;

    // 텍스처
    TextureHandle tex{ 0 }; // 0이면 default texture

    // 틴트/알파
    DirectX::XMFLOAT4 color{ 1,1,1,1 };

    // 레이어링
    float z = 0.0f;
};
