#pragma once
#include <DirectXMath.h>

struct RenderCamera
{
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 proj;
};