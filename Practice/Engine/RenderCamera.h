#pragma once
#include <DirectXMath.h>

struct RenderCamera
{
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 proj;
    DirectX::XMFLOAT3 positionWS{ 0,0,0 };
    float _pad0 = 0.0f;
};