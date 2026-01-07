#pragma once

#include <string>
#include <DirectXMath.h>

struct UITextDraw
{
    float x = 0.0f;
    float y = 0.0f;
    float sizePx = 16.0f;

    DirectX::XMFLOAT4 color{ 1,1,1,1 };

    std::wstring text;
    std::wstring fontFamily = L"Segoe UI";
};
