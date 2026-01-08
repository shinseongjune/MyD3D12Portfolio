#pragma once
#include <cstdint>
#include <Windows.h>
#include <vector>

#include "RenderItem.h"
#include "UIDrawItem.h"
#include "RenderCamera.h"
#include "UITextDraw.h"

class IRenderer
{
public:
    virtual ~IRenderer() = default;

    virtual void Initialize(HWND hwnd, uint32_t width, uint32_t height) = 0;
    virtual void Resize(uint32_t width, uint32_t height) = 0;

    // 프레임 렌더(드로우 리스트 받기)
    virtual void Render(const std::vector<RenderItem>& items, const RenderCamera& cam, TextureHandle skybox, const std::vector<UIDrawItem>& ui, const std::vector<UITextDraw>& text) = 0;
    virtual void RenderUI(const std::vector<UIDrawItem>& ui) = 0;

    virtual void Shutdown() = 0;
};
