#pragma once
#include <DirectXMath.h>

struct MaterialComponent
{
    DirectX::XMFLOAT4 color{ 1.f, 1.f, 1.f, 1.f };

    uint32_t srvIndex = 0;
};
