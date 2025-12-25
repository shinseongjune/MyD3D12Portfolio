#pragma once
#include <DirectXMath.h>

// 지금 단계에선 색만.
// 나중에 PSO/Texture/Sampler 등을 포함하는 MaterialHandle로 확장.
struct MaterialComponent
{
    DirectX::XMFLOAT4 color{ 1.f, 1.f, 1.f, 1.f };
};
