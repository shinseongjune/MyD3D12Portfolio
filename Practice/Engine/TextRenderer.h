#pragma once
#include <string_view>
#include <vector>
#include <DirectXMath.h>
#include "UIDrawItem.h"
#include "IFontFace.h"

class TextRenderer
{
public:
    explicit TextRenderer(IFontFace& font) : m_font(font) {}

    void AppendText(std::vector<UIDrawItem>& out,
        float x, float yTop,
        DirectX::XMFLOAT4 color,
        float z,
        std::string_view s) const
    {
        out.clear();

        const float baselineY = yTop + (float)m_font.BaselinePx();

        float penX = x;
        float penY = baselineY;

        for (char ch : s)
        {
            if (ch == '\n')
            {
                penX = x;
                penY += (float)m_font.LineHeightPx();
                continue;
            }
            if (ch == '\t')
            {
                penX += (float)(m_font.GetGlyph((uint8_t)' ').advance * 4);
                continue;
            }

            const uint8_t a = (uint8_t)ch;
            const Glyph& g = m_font.GetGlyph(a);

            // 공백 등: 비트맵이 없으면 advance만
            if (g.w > 0 && g.h > 0 && m_font.Atlas().IsValid())
            {
                UIDrawItem it{};
                it.x = penX + (float)g.offX;
                it.y = penY + (float)g.offY;   // bounds.top이 음수면 위로 올라감(정상)
                it.w = (float)g.w;
                it.h = (float)g.h;
                it.u0 = g.u0; it.v0 = g.v0;
                it.u1 = g.u1; it.v1 = g.v1;
                it.tex = m_font.Atlas();
                it.color = color;
                it.z = z;

                out.push_back(it);
            }

            penX += (float)g.advance;
        }
    }

private:
    IFontFace& m_font;
};
