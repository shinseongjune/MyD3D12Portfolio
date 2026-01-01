#pragma once
#include <vector>
#include <DirectXMath.h>
#include "TextureHandle.h"
#include <cstdint>

struct MaterialSlot
{
    DirectX::XMFLOAT4 color{ 1,1,1,1 };
    TextureHandle albedo{ 0 };     // 0 = checker / default
};

struct MaterialComponent
{
    std::vector<MaterialSlot> slots;

    MaterialComponent() = default;

    // 기존 코드 호환: MaterialComponent{ color, albedo }를 비슷하게 유지하고 싶으면 생성자 제공
    MaterialComponent(const DirectX::XMFLOAT4& c, TextureHandle t)
    {
        slots.push_back(MaterialSlot{ c, t });
    }

    // “단일 머티리얼” 접근을 편하게
    MaterialSlot& Primary()
    {
        if (slots.empty()) slots.push_back(MaterialSlot{});
        return slots[0];
    }
    const MaterialSlot& Primary() const
    {
        static MaterialSlot dummy{};
        return slots.empty() ? dummy : slots[0];
    }
};